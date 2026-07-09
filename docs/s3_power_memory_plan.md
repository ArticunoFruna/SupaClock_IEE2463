# Migración a XIAO ESP32-S3 — Plan de Potencia, Memoria y CNN-1D HAR

> Documento vivo. Última revisión: 2026-05-23.
> Aplica al carrier `hardware/SupaClock_Carrier/` (v1, header-only).

## 1. Por qué migramos

El **ESP32-C3 SuperMini** nos limitó en tres frentes:

| Limitante (C3) | Impacto en SupaClock |
|---|---|
| Un solo core RISC-V | LVGL + BLE + I2C scan + AD8232 DMA chocaron por scheduling |
| 400 kB SRAM, sin PSRAM | LVGL doble buffer + modelo CNN ≥ 100 kB no caben |
| Sin SIMD para INT8 | El HAR INT8 cuesta ~5× más que en S3 |
| ADC con SAR único compartido | El AD8232 DMA paralelo a WiFi/BLE introducía glitches |

El **XIAO ESP32-S3** resuelve los cuatro:
- **Dual core Xtensa LX7 @ 240 MHz** (cada uno con FPU + SIMD vectorial).
- **8 MB Octal PSRAM** disponible para LVGL, modelo y arenas grandes.
- **ESP-NN** kernels TFLite-Micro acelerados (PIE 128-bit).
- **ADC1 separado** + **ULP-RISC-V** (futuro: muestrear ECG en sleep).

## 2. Asignación de cores

| Core | Tareas | Justificación |
|---|---|---|
| **Core 0** (PRO) | NimBLE host, LVGL, I2C slow sensors (temp/fuel), GUI input | Tareas latency-sensitive de UI y radio |
| **Core 1** (APP) | HAR CNN-1D, AD8232 DMA callback, MAX30102 FIFO drain | Tareas con bursts predecibles, no quieren ser interrumpidas por BLE TX |

Pin de la task del HAR:
```c
xTaskCreatePinnedToCore(har_task, "har_task", 4096, NULL, 4, NULL, /*core=*/ 1);
```

## 3. Asignación de memoria

| Buffer | Tamaño | Memoria | Razón |
|---|---|---|---|
| LVGL frame buffer × 2 | 240·280·2 B · 2 ≈ **268 kB** | **PSRAM** | No cabe en SRAM, se accede secuencialmente vía DMA bounce |
| LVGL bounce buffer DMA | ~16 kB | **SRAM interna** | Requerido por SPI DMA (PSRAM no es DMA-capable directa) |
| Ring buffer HAR (200×6×2) | 2.4 kB | **SRAM interna** | Accedido en hot path por la task de inferencia |
| Tensor arena TFLite | 24 kB (configurable) | **PSRAM** (`HAR_TENSOR_ARENA_IN_PSRAM=1`) | Pesa, vive durante toda la app |
| Modelo `.tflite` blob | 30–60 kB | **Flash** (rodata) | Const, mapeado por XIP |
| AD8232 DMA buffer | 1 kB × 4 | **SRAM interna** | Latencia crítica |
| BLE TX queues | ~8 kB | **SRAM interna** (NimBLE default) | NimBLE no soporta PSRAM para sus colas |
| Heap general | resto | **PSRAM** vía `CONFIG_SPIRAM_USE_MALLOC=y` | `malloc()` grande → PSRAM, chico → SRAM |

`sdkconfig.defaults` ya tiene:
```
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384   # <16 kB → SRAM
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768 # reservar 32 kB para ISR / DMA
```

### 3.1 Mediciones Reales de Compilación (Fase 6 Touch UI)

Tras la implementación de toda la arquitectura del router táctil, las aplicaciones nativas completas y la integración de las fuentes e iconos custom (FontAwesome solid/brands y rango de acentos de español), las métricas estáticas del firmware son:

*   **SRAM Estática (RAM)**: **47.1%** (154,500 bytes usados de 327,680 bytes).
*   **Flash (Programa)**: **34.6%** (1,087,377 bytes usados de 3,145,728 bytes).

Las fuentes agregadas se mapean en Flash/XIP por lo que consumen 0 bytes de RAM adicionales, garantizando que más del 50% de la SRAM quede disponible para heap dinámico (buffers DMA, colas NimBLE, etc.). Esto valida con creces la holgura del ESP32-S3.

## 4. Plan de bajo consumo

### 4.1 DVFS + Light-Sleep automático

```c
esp_pm_config_t pm = {
    .max_freq_mhz = 240,   /* HAR / DMA bursts */
    .min_freq_mhz = 80,    /* idle típico       */
    .light_sleep_enable = true,
};
esp_pm_configure(&pm);
```

Con `CONFIG_FREERTOS_USE_TICKLESS_IDLE=y`, el SoC entra en light-sleep cuando todas las tasks duermen. BLE NimBLE coopera vía `CONFIG_BT_CTRL_MODEM_SLEEP_MODE_1=y`.

### 4.2 Cadencias por modo (de `power_modes.h`)

| Sensor | SPORT | NORMAL | SAVER |
|---|---|---|---|
| BMI160 ODR | 100 Hz | 100 Hz | 50 Hz (LP) |
| HAR inferencia | 1 Hz | 1 Hz | gated por varianza |
| MAX30102 | continuo | spot 30 s | spot 5 min |
| MAX30205 | 5 s | 30 s | 5 min |
| MAX17048 | 10 s | 60 s | 5 min |
| LVGL refresh | 30 fps | 15 fps | 5 fps + dimmed |

### 4.3 Gating del CNN
- En `POWER_MODE_SAVER`, el HAR mide varianza de |a| sobre 1 s; sólo invoca el intérprete si supera un umbral mínimo. El detector de caídas sigue corriendo (es trivial).
- En reposo prolongado (>5 min sin actividad), `har_cnn1d_pause()` apaga la task completa.

### 4.4 Periféricos en shutdown
- AD8232: `ad8232_power_down()` por defecto (SDN low), sólo encender al entrar a la pantalla ECG.
- MAX30102: SHDN entre mediciones spot (`hrm_shdn_between=true`).
- Backlight LCD: PWM duty 0 cuando display está auto-off; el ST7789 entra en SLPIN.

### 4.5 Estimación de corriente (target)
| Estado | Corriente promedio | Notas |
|---|---|---|
| Display on + HAR + BLE conn. | ~85 mA | LCD domina (50 mA backlight 60 %) |
| Display off + HAR + BLE adv. | ~12 mA | CPU mayormente dormida |
| SAVER, screen off, HAR gated | ~3 mA | Sólo BMI160 LP @50 Hz + RTC |

Bate de 250 mAh → ~80 h en SAVER, ~20 h en NORMAL display-off, ~3 h pantalla 100 %.

## 5. CNN-1D HAR — arquitectura y plan de entrenamiento

### 5.1 Entrada y salida
- 6 canales (ax, ay, az, gx, gy, gz) raw int16 del BMI160.
- Ventana 2 s @ 100 Hz = 200 muestras. Hop 1 s.
- Clases:
  - `RESTING` / `WALKING` / `RUNNING` (softmax de 3 vías)
  - Evento `FALL` reportado por separado (head heurístico/binario)

### 5.2 Por qué Fall en cabeza separada
Las caídas son sub-segundo, raras, y mezclarlas en un softmax desestabiliza las 3 clases base. El detector heurístico (free-fall → impacto → quietud) ya implementado tiene precisión >95 % en literatura UCI con costo ~5 instrucciones por muestra. Más adelante puede reemplazarse por una mini-CNN binaria que comparta el backbone.

### 5.3 Topología propuesta (≈ 40 kB INT8)
```
Input (200, 6)
 └─ Conv1D(16, k=5, stride=2)  + ReLU + BN     → (100, 16)
 └─ Conv1D(32, k=5, stride=2)  + ReLU + BN     → ( 50, 32)
 └─ Conv1D(64, k=3, stride=2)  + ReLU + BN     → ( 25, 64)
 └─ GlobalAveragePooling1D                       → (   64)
 └─ Dense(3, softmax)                            → (    3)
```
Cuantización post-entrenamiento a INT8 (representative dataset = capturas BLE reales del `test_imu` corriendo en el dispositivo).

### 5.4 Pipeline de entrenamiento (a crear en `tools/har/`)
1. **Captura** — `test_imu` + app Flutter logean a CSV etiquetado por clase.
2. **Preprocesado** — `numpy` ventaneo deslizante, balanceo de clases, splits estratificados.
3. **Train** — `tensorflow==2.16`, Keras Sequential; 30–50 épocas, augmentation por jitter + scaling.
4. **Quant** — `tf.lite.TFLiteConverter.from_keras_model` con `optimizations=[Optimizations.DEFAULT]` + representative dataset → `.tflite`.
5. **Embed** — `xxd -i har_model.tflite > lib/har_cnn1d/har_model.cc` (genera `har_model_tflite[]` y `_len`).
6. **Validate** — `test_har` en el dispositivo + matriz de confusión BLE.

### 5.5 Integración TFLite-Micro
Componente managed: `idf.py add-dependency "espressif/esp-tflite-micro"`. Luego descomentar los `#include` y la función `har_runner_run` en `lib/har_cnn1d/har_model_runner.cpp`. El intérprete corre con `MicroMutableOpResolver` declarando sólo los ops necesarios: `Conv2D`, `FullyConnected`, `Softmax`, `Reshape`, `Quantize`, `Dequantize`. Esto reduce el footprint del kernel registry de 80 kB → 12 kB.

## 6. Checklist de lo que falta tras esta migración

- [ ] Probar `pio run -e test_imu` en el carrier real.
- [ ] Capturar dataset etiquetado (mínimo 30 min/clase) con el dispositivo puesto en muñeca.
- [ ] Entrenar y exportar `har_model.tflite`.
- [ ] Añadir `esp-tflite-micro` como managed component y descomentar el runner.
- [ ] Cablear UI: una tarjeta LVGL "Activity" con la clase actual + icono.
- [ ] Cablear evento FALL → notificación BLE custom + banner rojo en LCD + log NVS.
- [ ] Verificar `esp_pm_lock` en las tasks de DMA (AD8232) para que no bajen el clock durante una captura.

# SupaClock — Protocolo BLE y Pipeline HAR (ESP32-S3)

> Documento de referencia para el equipo de la aplicación Flutter.  
> Describe el flujo completo desde la captura inercial en el firmware hasta la
> interpretación del estado de actividad en la app, incluyendo todos los formatos
> de byte exactos del protocolo BLE.  
> **Cualquier cambio en la app que toque UUIDs, offsets o valores enumerados
> debe verificarse contra este documento para evitar regresiones.**

---

## Índice

1. [Visión general del sistema](#1-visión-general-del-sistema)
2. [Pipeline HAR en el firmware](#2-pipeline-har-en-el-firmware)
   - 2.1 Captura IMU (BMI160)
   - 2.2 Ring buffer y ventana de inferencia
   - 2.3 Modelo TFLite Micro (ResNet-1D + LSTM)
   - 2.4 Filtro EMA y consolidación de estado
   - 2.5 Callback y publicación BLE
3. [Protocolo BLE](#3-protocolo-ble)
   - 3.1 Servicio y características
   - 3.2 Característica 0xFF01 — IMU raw
   - 3.3 Característica 0xFF02 — Telemetría agregada TLV
   - 3.4 Característica 0xFF03 — Streaming ECG
   - 3.5 Característica 0xFF04 — Comandos (escritura)
4. [Formato de los records TLV](#4-formato-de-los-records-tlv)
   - 4.1 Header del paquete agregado
   - 4.2 TLV 0x01 — Frecuencia cardíaca (HR)
   - 4.3 TLV 0x02 — Saturación de oxígeno (SpO2)
   - 4.4 TLV 0x03 — Temperatura
   - 4.5 TLV 0x04 — Batería
   - 4.6 TLV 0x05 — Pasos acumulados
   - 4.7 TLV 0x06 — Cambio de modo de energía
   - 4.8 TLV 0x07 — Resultado SPOT (HR+SpO2 on-demand)
   - 4.9 TLV 0x08 — Estado HAR (actividad)
5. [Cadencias de publicación por modo](#5-cadencias-de-publicación-por-modo)
6. [Enumeraciones de referencia](#6-enumeraciones-de-referencia)
7. [Implementación en Flutter (BleService)](#7-implementación-en-flutter-bleservice)
8. [Contratos de no-rotura](#8-contratos-de-no-rotura)

---

## 1. Visión general del sistema

```
┌─────────────────────────────── ESP32-S3 ───────────────────────────────┐
│                                                                        │
│  BMI160 (I2C)                                                          │
│     │ ax,ay,az,gx,gy,gz  int16 @ 100 Hz                               │
│     ▼                                                                  │
│  imu_task (core 0)──────────────────────────────► step_algo           │
│     │                                                 │ steps_sw       │
│     │ int16[6]  @ modo                               ▼                │
│     ├─► ble_telemetry_send_imu()  ──► 0xFF01 (notify directo)         │
│     └─► app_state (shared_sensor_data_t)                              │
│                                                                        │
│  har_task (core 1, prio 4)                                             │
│     │ Promedia 2 muestras físicas → 50 Hz efectivos                   │
│     │ Ring buffer circular 200×6 int16 (= 4.0 s @ 50 Hz)             │
│     │ Warmup: espera 200 pushes antes de inferir                       │
│     │ Inferencia cada 100 pushes (= 2 s, overlap 50%)                 │
│     │                                                                  │
│     ▼ ResNet-1D + LSTM INT8 TFLite Micro (624 KB en PSRAM)            │
│     │ Arena 384 KB en SRAM interna                                     │
│     │ Probs[4]: RESTING / WALKING / RUNNING / STAIRS                  │
│     │                                                                  │
│     ▼ EMA α=0.5 sobre probs + consolidación 3 consecutivas            │
│     │                                                                  │
│     └─► on_har_result() callback                                       │
│              │                                                         │
│              ├─► app_state.har_state (uint8)                           │
│              └─► ble_tx_push(0x08, state, 1B) ──► 0xFF02 (TLV flush) │
│                                                                        │
│  hrm_task / system_task / ecg_task                                     │
│     └─► ble_tx_push(tipo, datos) ──► buffer agregado                  │
│                                                                        │
│  ble_tx_task                                                           │
│     └─► ble_tx_flush() periódico ──► 0xFF02 notify                    │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
                          ▼ BLE (NimBLE / GATT)
┌──────────────── Flutter App (BleService) ──────────────────────────────┐
│  _onImuData()   ← 0xFF01                                              │
│  _onAggData()   ← 0xFF02  (parsea header + TLV loop)                  │
│  _onEcgData()   ← 0xFF03                                              │
│  sendCommand()  → 0xFF04                                               │
│                                                                        │
│  SupaClockTelemetry.harState (int 0-3) → UI                           │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Pipeline HAR en el firmware

### 2.1 Captura IMU (BMI160)

- **Sensor**: BMI160 vía I2C, rango de fábrica ±2 g (acelerómetro) y ±250 dps (giroscopio).
- **Resolución**: int16, rango completo ±32 768 LSB (a plena escala).
- **Task**: `imu_task` (core 0) muestrea a `p->imu_poll_ms` según el modo de energía activo:
  - SPORT → 20 ms (50 Hz físicos)
  - NORMAL → 40 ms (25 Hz físicos)
  - SAVER → 80 ms (12.5 Hz físicos)

> **Nota para la app**: la característica 0xFF01 refleja esta cadencia. En modo SAVER
> habrá notificaciones más espaciadas. No asumir 50 Hz fijo.

El `imu_task` escribe `imu_raw[6]` en `app_state` y, si `app_state_imu_tx_enabled()` es true,
llama a `ble_telemetry_send_imu()` (envío directo, sin buffer agregado).

---

### 2.2 Ring buffer y ventana de inferencia

El `har_task` corre en core 1 como **consumidor de inferencia**. **No lee el
sensor**: `imu_task` (core 0) drena el FIFO del BMI160 a **50 Hz** y alimenta el
ring del HAR con cada muestra vía `har_cnn1d_push_sample()`. Así hay un único
lector del bus I2C (sin contención ni FIFO overflow). El modelo es 50 Hz nativo.

```
BMI160 FIFO @50 Hz → imu_task → har_cnn1d_push_sample() → ring buffer (50 Hz)
                                                          → notify → har_task (infiere)
```

> **Nota:** el diseño anterior leía el BMI160 a 100 Hz desde la har_task y
> promediaba 2→1; se reemplazó por este feed único de 50 Hz (2026-06-25).

**Ring buffer** `s_ring[200][6]` (int16, 2.4 KB en SRAM interna):
- Tamaño: `HAR_WINDOW_SIZE = 200` muestras × 6 canales.
- Ventana temporal: 200 / 50 Hz = **4.0 segundos**.
- Escritura circular: `s_write_idx` apunta siempre a la próxima posición a escribir.

**Warmup de arranque**: la primera inferencia se ejecuta solo después de que el
ring buffer esté completamente lleno con datos reales (`s_total_ring_pushes >= 200`),
lo que ocurre a los **4 s desde el boot**. Antes de ese tiempo, el HAR no emite
ningún resultado. La app **no debe asumir** que recibirá un TLV 0x08 inmediatamente
tras conectarse.

**Cadencia de inferencia**:
- Hop size: `HAR_HOP_SIZE = 100` muestras = **2 s de datos nuevos**.
- Overlap: 50 % (las últimas 100 muestras de la ventana anterior se reutilizan).
- Resultado: **1 inferencia cada 2 s**, comenzando a los 4 s del boot.

**Construcción de la ventana**: cuando se dispara la inferencia con `s_write_idx = W`,
la ventana cronológica se construye como:
```
posición 0  → s_ring[(W + 0)   % 200]  ← muestra más antigua
posición 1  → s_ring[(W + 1)   % 200]
...
posición 199→ s_ring[(W + 199) % 200]  ← muestra más reciente
```

---

### 2.3 Modelo TFLite Micro (ResNet-1D + LSTM)

**Arquitectura** (`tools/train_har_cnn.py`):

```
Input: (1, 200, 6)  ← [batch, tiempo, canales: ax,ay,az,gx,gy,gz]
  │
  ├─ Conv1D(32, k=5, relu) → MaxPool1D(2)      → (1, 100, 32)
  │
  ├─ Bloque Residual 1:
  │    shortcut: Conv1D(64, k=1)
  │    main: Conv1D(64,k=3) → BN → ReLU → Conv1D(64,k=3) → BN
  │    add(main, shortcut) → ReLU → MaxPool1D(2)           → (1, 50, 64)
  │
  ├─ Bloque Residual 2:
  │    shortcut: Conv1D(128, k=1)
  │    main: Conv1D(128,k=3) → BN → ReLU → Conv1D(128,k=3) → BN
  │    add(main, shortcut) → ReLU → MaxPool1D(2)            → (1, 25, 128)
  │
  ├─ Bloque Residual 3:
  │    shortcut: Conv1D(256, k=1)
  │    main: Conv1D(256,k=3) → BN → ReLU → Conv1D(256,k=3) → BN
  │    add(main, shortcut) → ReLU → MaxPool1D(2)            → (1, 12, 256)
  │
  ├─ LSTM(64, unroll=True, return_sequences=False)           → (1, 64)
  ├─ Dense(64, relu)
  ├─ Dropout(0.3)  [solo en entrenamiento]
  └─ Dense(4, softmax)                                       → (1, 4)
```

**Cuantización**: INT8 completo (`converter.inference_input_type = tf.int8`,
`converter.inference_output_type = tf.int8`). Los parámetros `scale` y `zero_point`
del tensor de entrada y salida se imprimen al boot:
```
TFLite Init: input type=9, scale=X, zp=Y; output type=9, scale=A, zp=B
```

**Normalización de entrada** (`har_model_runner.cpp`): los int16 del ring se
normalizan a `[-1.0, 1.0]` dividiendo por `32768.0f` antes de aplicar la
cuantización INT8:
```c
float ax = (float)window[i][0] / 32768.0f;  // → [-1.0, 1.0]
// luego: q = ax / scale + zero_point
```

**Operadores TFLite registrados** en el resolver (`har_model_runner.cpp`):

| Operador | Uso en el modelo |
|---|---|
| `CONV_2D` | Conv1D de todas las capas (TFLite mapea Conv1D → Conv2D) |
| `MAX_POOL_2D` | MaxPooling1D entre bloques |
| `MEAN` | promedio global si se usa GAP |
| `FULLY_CONNECTED` | capas Dense |
| `SOFTMAX` | capa de salida |
| `RESHAPE` | ajuste de dimensiones |
| `QUANTIZE` / `DEQUANTIZE` | bordes del grafo INT8 |
| `EXPAND_DIMS` | adaptación de forma |
| `ADD` | skip-connections de los bloques residuales |
| `RELU` | **ReLU standalone después de cada ADD residual** |
| `LOGISTIC` / `TANH` | puertas de la LSTM (sigmoid/tanh) |
| `FILL`, `MUL`, `PACK`, `SHAPE`, `SPLIT`, `STRIDED_SLICE`, `TRANSPOSE`, `UNPACK` | LSTM desenrollada |

> **Crítico**: Si se actualiza el modelo (reentrenamiento) y la nueva arquitectura
> introduce operadores no listados aquí, `AllocateTensors()` fallará silenciosamente
> (el firmware cae a estado `RESTING` por seguridad). Verificar con los logs de boot
> que `model_ok = true`.

**Memoria**:
- Modelo `.tflite`: ~624 KB copiado a **PSRAM** al init (`heap_caps_malloc MALLOC_CAP_SPIRAM`).
- Tensor Arena: **384 KB** en **SRAM interna** (`heap_caps_aligned_alloc MALLOC_CAP_INTERNAL`).
  Si no hay SRAM suficiente, fallback a PSRAM (log `LOGW: intentando en PSRAM`).

---

### 2.4 Filtro EMA y consolidación de estado

Las probabilidades brutas `probs[4]` del modelo pasan por dos capas de filtrado
antes de generar el estado final:

**Capa 1 — EMA (Exponential Moving Average)**:
```c
// α = 0.5, inicializado en { 1.0, 0.0, 0.0, 0.0 }  (RESTING dominante al boot)
s_probs_ema[i] = 0.5f * probs[i] + 0.5f * s_probs_ema[i];
```

Con α = 0.5, si el modelo predice `WALKING=0.8` consistentemente:
- Ventana 1 tras boot: EMA_rest ≈ 0.75,  EMA_walk ≈ 0.25
- Ventana 2: EMA_rest ≈ 0.55,  EMA_walk ≈ 0.45
- Ventana 3: EMA_rest ≈ 0.35,  EMA_walk ≈ 0.60 ← argmax flipa a WALKING
- Ventana 4+: converge a WALKING

Tiempo desde arranque hasta que el EMA reporta WALKING por primera vez:
≈ **3 ventanas × 2 s = ~10 s** (incluyendo el warmup de 4 s).

**Capa 2 — Consecutivas (histeresis)**:
```c
// 3 ventanas consecutivas del mismo argmax(EMA) → consolida el estado
if (s_consecutive_count >= 3) s_consolidated_state = s_candidate_state;
```

**Latencia total mínima desde el boot hasta detectar WALKING**:
- Warmup buffer: 4 s
- Primera inferencia donde EMA flipa: ~+6 s
- 3 consecutivas: ~+6 s
- **Total mínimo: ~16 s** desde que el usuario empieza a caminar con el reloj encendido.

Tras una transición (ej. reposo → caminata), la latencia es solo la de EMA + consecutivas
(el warmup ya pasó): **~10 s**.

> **Implicación para la app**: no mostrar "sin actividad" ni alarmas por ausencia de
> TLV 0x08 durante los primeros 20 s después de conectar. El estado inicial correcto
> es `RESTING (0)`.

**Logs de diagnóstico** (visibles en monitor serial mientras los TEGs de diagnóstico
están activos en el firmware):
```
I HAR: HAR ok=1 raw=[R:0.05 W:0.82 Ru:0.11 F:0.02]
I HAR: HAR ema=[R:0.15 W:0.75 Ru:0.09 F:0.01] cand=1 consec=2 → state=0
```
`state=0` aún porque `consec < 3`. Al llegar a `consec=3`, `state` cambia a 1
y se dispara el callback y el TLV 0x08.

---

### 2.5 Callback y publicación BLE

El callback `on_har_result()` en `supaclock_app.c` se invoca desde `har_task`
(core 1) cada vez que sale una inferencia procesada:

```c
static void on_har_result(const har_result_t *result, void *user) {
    // 1. Actualiza app_state (lectura por UI local del reloj)
    sd->har_state      = (uint8_t)result->state;
    sd->har_updated_ms = now_ms();

    // 2. Empuja TLV 0x08 al buffer agregado BLE (NO flush inmediato)
    uint8_t state_val = (uint8_t)result->state;
    ble_tx_push(BLE_TLV_TYPE_HAR_STATE, &state_val, 1, 0xFF);
    //                                                    ^^^^^
    //                               0xFF = no forzar flush ahora
}
```

El TLV llega a la app cuando `ble_tx_task` ejecuta el siguiente `ble_tx_flush()`,
cuya cadencia depende del modo (ver §5).

---

## 3. Protocolo BLE

### 3.1 Servicio y características

| UUID corto | UUID completo | Dirección | Tamaño | Descripción |
|---|---|---|---|---|
| `0xFF00` | `0000FF00-0000-1000-8000-00805F9B34FB` | — | — | Servicio principal SupaClock |
| `0xFF01` | `0000FF01-0000-1000-8000-00805F9B34FB` | Notify | **12 B** | IMU 6-DOF raw |
| `0xFF02` | `0000FF02-0000-1000-8000-00805F9B34FB` | Notify | variable | Telemetría agregada TLV |
| `0xFF03` | `0000FF03-0000-1000-8000-00805F9B34FB` | Notify | **20 B** | Streaming ECG |
| `0xFF04` | `0000FF04-0000-1000-8000-00805F9B34FB` | Write | **1 B** | Comandos desde app |

El nombre BLE del dispositivo contiene `SupaClock` (usado por `BleService.startScan()`).

---

### 3.2 Característica 0xFF01 — IMU raw

Notificación directa (no pasa por buffer agregado) desde `imu_task`.

**Formato** (12 bytes, little-endian):
```
Offset  Tipo    Campo
  0     int16   ax   (acelerómetro X, ±32768 LSB = ±2 g)
  2     int16   ay
  4     int16   az
  6     int16   gx   (giroscopio X, ±32768 LSB = ±250 dps)
  8     int16   gy
 10     int16   gz
```

**Cadencia**: depende del modo activo (SPORT≈50 Hz, NORMAL≈25 Hz, SAVER≈12.5 Hz),
solo cuando `app_state_imu_tx_enabled() == true` (controlado por el menú del reloj
y habilitado por defecto).

**Conversión a unidades físicas**:
```dart
double toG(int raw)   => raw / 16384.0;    // para ±2 g (16384 LSB/g)
double toDps(int raw) => raw / 131.0;      // para ±250 dps (131 LSB/dps)
```

---

### 3.3 Característica 0xFF02 — Telemetría agregada TLV

Paquete con header fijo de 6 bytes seguido de N records TLV. El firmware acumula
varios TLVs en un buffer y hace un flush periódico, lo que permite enviar múltiples
tipos de datos en una sola notificación BLE.

**Parsing Flutter** (`_onAggData`):
```dart
if (data.length < 6) return;          // validar mínimo
final powerMode  = data[4];           // byte 4
final payloadLen = data[5];           // byte 5
var off = 6;                          // inicio del payload
final end = (6 + payloadLen).clamp(0, data.length);

while (off + 2 <= end) {
    final type = data[off];
    final len  = data[off + 1];
    off += 2;
    if (off + len > end) break;       // record truncado → abortar
    // procesar payload de `len` bytes en data[off..off+len)
    off += len;
}
```

---

### 3.4 Característica 0xFF03 — Streaming ECG

Notificación directa (no pasa por buffer agregado). Solo activa cuando el modo ECG
está habilitado (comando 0x01 desde la app o desde el menú del reloj).

**Formato** (20 bytes, little-endian):
```
Offset  Tipo    Campo
  0     int16   muestra ECG 0   (ADC raw, downsampled a ≈500 Hz)
  2     int16   muestra ECG 1
  ...
 18     int16   muestra ECG 9
```
10 muestras int16 por paquete. El ADC es de 12 bits (valores 0–4095 sobre GND–3.3V),
promediado cada `ECG_DOWNSAMPLE_RATIO = 40` muestras del DMA para reducir ruido.

---

### 3.5 Característica 0xFF04 — Comandos (escritura)

La app escribe 1 byte sin respuesta (`withoutResponse: true`).

| Valor | Acción |
|---|---|
| `0x00` | Detener streaming ECG |
| `0x01` | Iniciar streaming ECG |

---

## 4. Formato de los records TLV

Cada record tiene la estructura:
```
[type: 1B][len: 1B][data: len bytes]
```
Todos los campos multi-byte son **little-endian**.

---

### 4.1 Header del paquete agregado

Siempre los primeros 6 bytes de cada notificación 0xFF02:

```
Offset  Tipo     Campo          Descripción
  0     uint32   boot_ts_ms     ms desde el boot del dispositivo (base para deltas)
  4     uint8    power_mode     Modo de energía activo (0=SPORT, 1=NORMAL, 2=SAVER)
  5     uint8    payload_len    Bytes de records TLV que siguen inmediatamente
```

Los campos `delta_ms` dentro de los TLVs son relativos a `boot_ts_ms`.
En la implementación actual, la mayoría de TLVs pone `delta_ms = 0`
(se usa el timestamp del header como referencia del paquete completo).

---

### 4.2 TLV 0x01 — Frecuencia cardíaca (HR)

```
type = 0x01
len  = 4
data:
  Offset  Tipo    Campo
    0     uint16  delta_ms   (relativo a boot_ts_ms del header; actualmente 0x0000)
    2     uint8   bpm        (latidos por minuto; 0 = sin medición válida)
    3     uint8   quality    (1=buena, 0=sin calidad declarada)
```

En modo SPORT el HR se publica cada ~1 s (modo continuo MAX30102).
En modos NORMAL/SAVER se publica al finalizar una medición SPOT.

**Flutter**: `updates['hr_bpm'] = payload.getUint8(2)` (offset 2 dentro del data del TLV).

---

### 4.3 TLV 0x02 — Saturación de oxígeno (SpO2)

```
type = 0x02
len  = 4
data:
  Offset  Tipo    Campo
    0     uint16  delta_ms   (actualmente 0x0000)
    2     uint8   pct        (porcentaje SpO2; 0 = sin medición válida)
    3     uint8   quality    (1=buena, 0=sin calidad declarada)
```

**Flutter**: `updates['spo2_pct'] = payload.getUint8(2)`.

---

### 4.4 TLV 0x03 — Temperatura corporal

```
type = 0x03
len  = 4
data:
  Offset  Tipo    Campo
    0     uint16  delta_ms    (actualmente 0x0000)
    2     int16   temp_x100   (temperatura en °C × 100; ej. 3654 = 36.54 °C)
```

**Flutter**: `updates['temp_c'] = payload.getInt16(2, Endian.little) / 100.0`.

Sensor: MAX30205 (precisión ±0.1 °C). Cadencia: según `p->temp_period_ms` del modo.

---

### 4.5 TLV 0x04 — Batería

```
type = 0x04
len  = 5
data:
  Offset  Tipo    Campo
    0     uint16  delta_ms    (actualmente 0x0000)
    2     uint16  mv          (voltaje de batería en mV; ej. 3800 = 3.800 V)
    4     uint8   soc         (estado de carga en %; 0–100)
```

**Flutter**: `updates['bat_mv'] = payload.getUint16(2, Endian.little)` y
`updates['bat_soc'] = payload.getUint8(4)`.

> **Nota**: el `soc` del TLV es `uint8` truncado de un `float` filtrado por EMA
> (α=0.1 en `system_task`). Valores posibles: 0–100. No se envía el decimal.

---

### 4.6 TLV 0x05 — Pasos acumulados

```
type = 0x05
len  = 4
data:
  Offset  Tipo    Campo
    0     uint32  total_steps   (contador acumulado desde el último reset; little-endian)
```

**Flutter**: `updates['steps'] = payload.getUint32(0, Endian.little)`.

Cadencia: cada 30 s fijos, independiente del modo (publicado desde `system_task`).
El contador puede resetearse desde el menú del reloj (opción "Reiniciar Pasos"),
lo que hace que el próximo TLV 0x05 tenga `total_steps = 0`.

El conteo usa el algoritmo FFT de podómetro software (`step_algorithm.c`), no el
step counter hardware del BMI160 (deshabilitado en producción).

---

### 4.7 TLV 0x06 — Cambio de modo de energía

```
type = 0x06
len  = 1
data:
  Offset  Tipo    Campo
    0     uint8   new_mode   (0=SPORT, 1=NORMAL, 2=SAVER)
```

Este TLV **fuerza un flush inmediato** (`flush_now_mode = new_mode`), por lo que
llega a la app sin esperar el período de flush. La app debe actualizar cualquier
indicador visual de modo al recibirlo.

**Flutter**: `updates['power_mode'] = powerMode` (del header del mismo paquete,
ya que el `ble_tx_push` pasa `new_mode` como `flush_now_mode`).

---

### 4.8 TLV 0x07 — Resultado SPOT (HR + SpO2 on-demand)

Emitido al finalizar (o abortar) una medición SPOT iniciada desde la pantalla
`HRSPOT` del reloj. También fuerza flush inmediato.

```
type = 0x07
len  = 6
data:
  Offset  Tipo    Campo
    0     uint8   bpm        (frecuencia cardíaca medida)
    1     uint8   spo2       (% SpO2 medido)
    2     uint16  dur_ms     (duración de la medición en ms; little-endian)
    4     uint8   quality    (0=POOR, 1=FAIR, 2=GOOD)
    5     uint8   aborted    (0=completado, 1=abortado o fallido)
```

**Flutter** (`_onAggData`):
```dart
case TlvTypes.spotResult:
    if (len == 6) {
        _spotController.add(SpotResult(
            bpm:         payload.getUint8(0),
            spo2:        payload.getUint8(1),
            durationMs:  payload.getUint16(2, Endian.little),
            quality:     payload.getUint8(4),
            aborted:     payload.getUint8(5) != 0,
            completedAt: DateTime.now(),
        ));
        if (!aborted) {
            updates['hr_bpm']   = bpm;
            updates['spo2_pct'] = spo2;
        }
    }
```

El `spotStream` de `BleService` emite un `SpotResult` por cada medición terminada,
independientemente de si fue exitosa o abortada.

---

### 4.9 TLV 0x08 — Estado HAR (actividad)

**El TLV de actividad del modelo HAR.** El más relevante para el equipo de la app.

```
type = 0x08
len  = 1
data:
  Offset  Tipo    Campo
    0     uint8   state   (ver tabla de estados)
```

**Tabla de estados**:

| Valor uint8 | Constante firmware (`har_state_t`) | Significado | Pantalla reloj |
|---|---|---|---|
| `0` | `HAR_STATE_RESTING` | Reposo / inactivo | "Reposo" |
| `1` | `HAR_STATE_WALKING` | Caminando | "Caminar" |
| `2` | `HAR_STATE_RUNNING` | Corriendo | "Correr" |
| `3` | `HAR_STATE_STAIRS` | Subir/bajar escaleras | "Escaleras" |

**Frecuencia de emisión**:
- El modelo infiere cada 2 s. Solo emite TLV cuando el estado **consolidado**
  cambia (si el estado se mantiene estable, se sigue enviando en cada flush).
- La app recibirá el TLV en el siguiente flush periódico de `ble_tx_task`
  (no hay flush inmediato en `on_har_result()`).
- Si no hay BLE conectado, el callback igualmente actualiza `app_state.har_state`
  para la pantalla local del reloj.

**Flutter** (`_onAggData`):
```dart
case TlvTypes.harState:
    if (len == 1) {
        updates['har_state'] = payload.getUint8(0);
    }
```

El campo `SupaClockTelemetry.harState` es un `int?` (null antes de recibir el
primer TLV 0x08). Valores válidos: 0, 1, 2, 3.

---

## 5. Cadencias de publicación por modo

Los períodos de flush del buffer agregado (0xFF02) dependen del modo:

| Modo | `ble_agg_flush_ms` | Cadencia IMU (0xFF01) | HR auto-spot | Temp/Bat |
|---|---|---|---|---|
| SPORT (0) | definido en `power_modes.c` | ~50 Hz | continuo (cada 1 s) | más frecuente |
| NORMAL (1) | mayor que SPORT | ~25 Hz | periódico (~5 min) | cada varios min |
| SAVER (2) | mayor aún | ~12.5 Hz | periódico (>10 min) | menos frecuente |

El modo actual siempre está en el byte 4 (campo `power_mode`) del header de cada
paquete 0xFF02.

> La app puede inferir el modo activo del header del paquete o del TLV 0x06 si
> el usuario lo cambia desde el reloj.

---

## 6. Enumeraciones de referencia

### Power Mode (byte 4 del header, TLV 0x06)
```dart
const int powerModeSport  = 0;
const int powerModeNormal = 1;
const int powerModeSaver  = 2;
```

### HAR State (TLV 0x08, byte 0)
```dart
const int harStateResting = 0;
const int harStateWalking = 1;
const int harStateRunning = 2;
const int harStateStairs  = 3;
```

### TLV Types (byte `type` de cada record)
```dart
class TlvTypes {
  static const int hr         = 0x01;
  static const int spo2       = 0x02;
  static const int temp       = 0x03;
  static const int bat        = 0x04;
  static const int steps      = 0x05;
  static const int modeEvt    = 0x06;
  static const int spotResult = 0x07;
  static const int harState   = 0x08;
}
```
Estos valores coinciden exactamente con las macros `BLE_TLV_TYPE_*` en
`lib/ble_telemetry/ble_telemetry.h`.

### SpotResult Quality
```dart
const int spotQualityPoor = 0;
const int spotQualityFair = 1;
const int spotQualityGood = 2;
```

---

## 7. Implementación en Flutter (BleService)

### Flujo de conexión

1. `startScan()` → busca dispositivo con nombre que contenga `"SupaClock"`.
2. `connectToDevice()` → conecta y llama a `_discoverAndSubscribe()`.
3. `_discoverAndSubscribe()` → resuelve los 4 UUIDs y suscribe:
   - `_imuChar` → `_onImuData`
   - `_aggChar` → `_onAggData`
   - `_ecgChar` → `_onEcgData`
4. `_cmdChar` se guarda para escritura bajo demanda.

### Streams disponibles

| Stream | Tipo | Descripción |
|---|---|---|
| `imuStream` | `Stream<ImuSample>` | Muestras IMU brutas (0xFF01) |
| `ecgStream` | `Stream<List<int>>` | Lotes de 10 muestras ECG (0xFF03) |
| `telemetryStream` | `Stream<SupaClockTelemetry>` | Telemetría acumulada; se emite en cada flush 0xFF02 |
| `spotStream` | `Stream<SpotResult>` | Solo emite al finalizar una medición SPOT (TLV 0x07) |

### `SupaClockTelemetry` — campos relevantes

```dart
class SupaClockTelemetry {
  final int?    heartRate;    // bpm; null = sin datos aún
  final int?    hrQuality;
  final int?    spo2;         // %; null = sin datos aún
  final int?    spo2Quality;
  final double? temperature;  // °C; null = sin datos aún
  final int?    steps;        // total; null = sin datos aún
  final int?    batteryMv;    // mV
  final int?    batterySoc;   // 0–100
  final int?    powerMode;    // 0/1/2
  final int?    harState;     // 0–3; null antes del primer TLV 0x08 (~20 s tras boot)
  final DateTime timestamp;   // DateTime.now() del último merge
}
```

El método `merge()` actualiza solo los campos presentes en el `Map<String, dynamic>`
del parse del TLV, preservando los valores anteriores para los campos no actualizados.

### Envío de comandos

```dart
await bleService.startEcgStream();  // escribe 0x01 en 0xFF04
await bleService.stopEcgStream();   // escribe 0x00 en 0xFF04
await bleService.sendCommand(0xXX); // comando arbitrario (para futuros comandos)
```

---

## 8. Contratos de no-rotura

Los siguientes aspectos son **estables** entre firmware y app. Cualquier cambio
en uno de los lados requiere actualización coordinada en el otro:

### ✅ Estable — NO cambiar sin coordinar

| Elemento | Valor fijo |
|---|---|
| UUID del servicio | `0xFF00` |
| UUID característica IMU | `0xFF01` |
| UUID característica AGG | `0xFF02` |
| UUID característica ECG | `0xFF03` |
| UUID característica CMD | `0xFF04` |
| Tamaño fijo IMU packet | **12 bytes** exactos |
| Tamaño fijo ECG packet | **20 bytes** exactos (10 × int16) |
| Endianness de todos los campos | **little-endian** |
| Offset de `power_mode` en header | **byte 4** |
| Offset de `payload_len` en header | **byte 5** |
| Inicio del primer TLV en el paquete AGG | **byte 6** |
| Valores `type` de todos los TLVs (0x01–0x08) | fijos (ver §4) |
| `len` de cada tipo TLV | fijos por tipo (ver §4) |
| Valores HAR state (0–3) | fijos (ver §6) |
| Valores power mode (0–2) | fijos (ver §6) |
| Byte de comando CMD start/stop ECG | `0x01` / `0x00` |

### ⚠️ Puede cambiar — requiere coordinación

| Elemento | Situación actual | Impacto si cambia |
|---|---|---|
| Cadencia de flush 0xFF02 | variable por modo | La app no debe asumir período fijo; usar `timestamp` |
| Cadencia 0xFF01 | variable por modo | Igual |
| Latencia primera inferencia HAR | ~16–20 s tras boot | La app debe tolerar `harState = null` sin error |
| Número de clases HAR | 4 actualmente | Si se añade clase 5, la app debe manejar `harState = 4` sin crash |
| Precisión de pasos | calibrado empiricamente | No es un valor de verdad absoluta |
| Arquitectura del modelo (`.tflite`) | ResNet-1D + LSTM | El protocolo BLE no cambia; solo afecta precisión |

### ❌ No implementado / fuera de scope

- Historial de datos (no hay almacenamiento en el reloj).
- Timestamps absolutos (solo `boot_ts_ms` relativo al boot del dispositivo).
- Comandos de configuración desde la app hacia el reloj (más allá de ECG on/off).
- Sincronización de hora desde la app al reloj.

---

*Generado el 2026-06-18. Mantener actualizado ante cambios en
`lib/ble_telemetry/ble_telemetry.h`, `lib/app_state/app_state.h`,
`lib/supaclock_app/supaclock_app.c` o `app/lib/services/ble_service.dart`.*

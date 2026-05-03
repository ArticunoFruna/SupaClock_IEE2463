# Brief de edición — Informe Avance 2 SupaClock

Otro agente debe ejecutar este plan completo sobre `docs/Entrega2/main.tex`.
El usuario revisará el resultado, no las acciones intermedias.

---

## 0. Contexto

`main.tex` está desactualizado respecto al estado real del firmware. Esta semana
se añadieron: modos de energía persistentes, light sleep dinámico con PM locks,
cuarta característica BLE (CMD), formato TLV agregado en 0xFF02, SM SPOT del
MAX30102, mejoras del MAX17048, refactor de DMA del AD8232, y se decidió
migrar de ESP32-C3 a Seeed XIAO ESP32-S3. Toda la información de respaldo está en:

- `docs/test_general_demo.md` — descripción exhaustiva del firmware actual.
- `docs/changes_summary.md` — resumen de la optimización energética (cifras 75/15/9 %).
- `docs/Entrega2/presentacion.tex` — fuente de las figuras/datos cuantitativos validados.
- `lib/power_modes/power_modes.h`, `lib/ble_telemetry/ble_telemetry.h` — APIs reales.
- `src/tests/test_general.c`, `src/tests/test_ad8232_raw.c` — código actual.

**Pesos de rúbrica** (`docs/Rubrica_informe_avance_2.pdf`):
Implementación 30 % > Impacto/estándares 25 % > Modelos/simulaciones 20 % >
Diagrama 10 % = Planificación 10 % > Set-up 5 %.

Concentrar esfuerzo en Implementación y Modelos. Impacto/estándares ya está bien.

---

## 1. Reglas a respetar (NO violar)

1. **NO inventar números**. Las únicas cifras de consumo válidas están en la
   presentación (figuras 10, 11, 12). Toda otra cifra de autonomía debe
   declararse como estimación o pendiente de validación.
2. **NO mencionar el simulador PC con SDL** (sólo se usa para screenshots).
3. **NO mencionar el pico histórico de 110 mA** salvo como referencia pasada
   en el contexto de los brown-outs / motivación de la migración a S3.
4. **NO usar emojis** ni en el .tex ni en archivos auxiliares.
5. **NO crear documentos nuevos** salvo lo que se indica explícitamente abajo.
6. **NO tocar** la sección 4 (Impacto y estándares) salvo retoques mínimos
   indicados — la rúbrica se cubre bien tal como está.
7. **NO tocar** el `presentacion.tex` ni los archivos `fig_*.tex` existentes
   sin revisar antes su contenido.
8. **Comentarios en código embebido (lstlisting): mínimos**, sólo el "porqué"
   no obvio.

---

## 2. Hechos validados (usar libremente)

### Firmware — arquitectura
- 7 tareas FreeRTOS: `gui_task` (5, 30 Hz), `imu_task` (6, 50/25/12.5 Hz por modo),
  `hrm_task` (5, poll 10 Hz), `system_task` (3, cada 2 s), `ble_tx_task`
  (4, flush 1/10/60 s por modo), `ecg_task` (7, DMA bursts), `perf_monitor_task`
  (2, cada 10 s).
- 2 mutex: `xSensorDataMutex` (datos compartidos), `xGuiSemaphore` (LVGL).
- Botones: **2** (NEXT GPIO10, SELECT GPIO1+wakeup), short/long press.

### Modos de energía (`lib/power_modes`)
| Cadencia | SPORT | NORMAL | SAVER |
|---|---|---|---|
| HRM polling | 100 ms continuo | 100 ms spot c/10 min | 100 ms spot c/30 min |
| SpO2 auto | cada 5 min | cada 30 min | sólo manual |
| HRM SHDN entre medidas | no | sí | sí |
| IMU polling | 20 ms (50 Hz) | 40 ms (25 Hz) | 80 ms (12.5 Hz) |
| Temp | 30 s | 5 min | 15 min |
| Batería | 30 s | 30 s | 30 s |
| BLE flush | 1 s | 10 s | 60 s |
| Display auto-off | 30 s | 15 s | 8 s |

Persistencia NVS namespace `supaclock`: `power_mode`, `off_sport_s`,
`off_normal_s`, `off_saver_s`.

### BLE NimBLE (`lib/ble_telemetry`)
- Servicio primario `0xFF00`, **4 características**:
  - `0xFF01` IMU 12 B raw (ax,ay,az,gx,gy,gz int16) — directo, no buffer.
  - `0xFF02` Telemetría agregada TLV: `ble_agg_header_t` (boot_ts_ms u32 +
    power_mode u8 + payload_len u8) + records TLV.
  - `0xFF03` ECG streaming, chunks 10×i16 = 20 B.
  - `0xFF04` Comandos RX 1 B (0x01 start ECG, 0x00 stop ECG).
- TLV types: HR(0x01,4B), SPO2(0x02,4B), TEMP(0x03,4B), BAT(0x04,5B),
  STEPS(0x05,4B), MODE_EVT(0x06,1B), SPOT_RESULT(0x07,6B).
- Advertising 1000 ms, sin bonding (compatibilidad BlueZ/tests).
- Modem Sleep BT activado en `sdkconfig.defaults`.

### GUI LVGL — 7 pantallas
HOME, BIO, HRSPOT, ECG, MENU (ciclables); MODE, SETTINGS (sub-pantallas).
Items menú: Modo Energía, Auto-off Pant., Reiniciar Pasos, Vincular BLE,
Apagar (deep sleep), Tx IMU ON/OFF, Reset Batería.
Backlight PWM, auto-off por modo, gui_task baja a 10 Hz con pantalla off.

### Power management dinámico
- DFS 10–160 MHz, light sleep enable.
- 3 PM locks: ECG (`ESP_PM_NO_LIGHT_SLEEP` durante DMA), ADC continuo (APB max
  while DMA running), NimBLE (durante eventos de conexión).
- Resultado medido vía `vTaskGetRunTimeStats`: **75 % SLEEP, 15 % APB_MAX,
  9 % CPU_MAX** (post-optimización). Antes: 0 % SLEEP, 92 % APB_MAX.

### MAX30102 — SM SPOT
`IDLE → SETTLING (5 s) → MEASURING → DONE / FAILED / ABORTED`.
Modo continuous (sólo SPORT) o spot (NORMAL/SAVER, con SHDN entre medidas).
Motion gating por jerk del IMU (`max30102_set_motion_level`).

### MAX17048
POR detection (RI bit STATUS), Quick Start automático en init en frío,
hibernate (HIBRT 0x80/0x30), VRESET configurado, reset SW por menú
(POR + Quick Start), expone `max17048_reset()`.

### AD8232 / ECG
- ADC1 CH0 GPIO0, modo continuous DMA, **20 kHz HW** (mínimo C3),
  frame DMA 1024 B = 256 muestras, notify cada ~12.8 ms.
- Decimación 1:40 con promedio (sum/40, boxcar antialias) → **500 Hz** efectivos.
- Chunks 10×i16 = 20 B por notify a 0xFF03.
- Refactor: `adc_continuous_new_handle/config` movidos de `init_dma` a
  `start_dma`, `adc_continuous_deinit` en `stop_dma` → **el lock APB_MAX se
  adquiere sólo dentro de la pantalla ECG**. Antes lo retenía siempre.
- Existe `test_ecg_raw` (env PlatformIO) con decimación pura sin promedio y
  sin esp_pm para diagnóstico HW vs SW.

### Consumo en batería (mediciones reales con Tektronix TX3)
**Disclaimer obligatorio**: el TX3 no tiene ancho de banda para resolver
los pulsos de light sleep, por lo que estas cifras corresponden a **consumo
en estado activo**, no al promedio temporal:
- 17 mA — pantalla off, modo ECO/SAVER
- 22 mA — teórico (datasheet)
- 27 mA — pantalla off, modo SPORT
- 63 mA — pantalla on
- 69 mA — pico medido tras optimización (peor caso: ECG + BLE + pantalla on)

Pico histórico pre-optimización: ~110 mA (causa de brown-outs, NO usar como
referencia actual).

### Migración a Seeed XIAO ESP32-S3 (encargada, en tránsito)
Justificaciones (ya documentadas en presentacion.tex slide 13):
1. **Brown-out resuelto**: buck SGM6029 integrado (~800 mA) reemplaza al
   LDO LLV8 (insuficiente para peaks BLE+display).
2. **Memoria**: 512 KB SRAM (vs 320 KB en C3) + PSRAM externa hasta 8 MB.
   Heap libre actual en C3 ~99 KB, modelo CNN 1D + scratch necesita 20–100 KB.
3. **ESP-NN con SIMD**: inferencia CNN 1D int8 5–20 ms (S3) vs 50–200 ms (C3).
4. **Dual-core 240 MHz**: separa ML del firmware sin contención.
5. **BMS + fuel gauge integrados** en el módulo Seeed → menos componentes en PCB.

---

## 3. Edición sección por sección

### Sección 1 — Introducción / Resumen ejecutivo
- Cambiar "cinco tareas concurrentes" → **siete tareas concurrentes**.
- Mencionar como hitos cumplidos: modos de energía persistentes, light sleep
  dinámico con PM locks (75 % SLEEP), migración a ESP32-S3 encargada.
- Cliente PC `supaclock_monitor.py` ya es la herramienta oficial de validación.

### Sección 2 — Diagrama de bajo nivel
- Editar `tab:specs` (línea ~199):
  - BLE: "3 chr." → "**4 chr.** (FF01 IMU, FF02 TLV agregado, FF03 ECG, FF04 CMD)".
  - GUI: "4 pantallas" → "**7 pantallas**".
  - Botones: "3 botones" → "**2 botones (NEXT, SELECT) short/long-press**".
  - **Añadir fila** *Power Management dinámico*: interfaz "esp_pm + locks",
    spec "DFS 10–160 MHz, light sleep, 3 PM locks", estado verde 100 %.
  - **Añadir fila** *Power Modes*: interfaz "NVS + power_get_profile()",
    spec "3 perfiles (SPORT/NORMAL/SAVER), 8 cadencias parametrizadas",
    estado verde 100 %.
- Bullet list "Cambios respecto del diagrama original" (línea ~166):
  - Corregir "tres a tres botones" → "tres a **dos** botones".
  - LVGL: "cuatro pantallas" → "**siete pantallas (5 ciclables + 2 sub)**".
  - **Añadir bullet** sobre la migración encargada al ESP32-S3
    (la rúbrica pide cambios justificados).

### Sección 3 — Planificación
- En `tab:gantt25` (línea ~234) **añadir 3 filas**:
  - "Optimización energética (light sleep + PM locks)" — **Adelantado**
    (no estaba en plan original, ejecutado esta semana).
  - "Modos de energía persistentes (SPORT/NORMAL/SAVER)" — **Adelantado**.
  - "Migración a Seeed XIAO ESP32-S3" — **Nueva tarea** (justificada por
    brown-outs y techo SRAM, encargada y en tránsito).
- Tabla `tab:trabajo` (línea ~284): retocar aportes de Pablo y Tomás
  añadiendo PM/light sleep, modos NVS, refactor DMA AD8232, optimización
  driver ST7789. NO mencionar simulador SDL.

### Sección 4 — Impacto y estándares (NO TOCAR salvo lo siguiente)
- Bluetooth Core Spec (línea ~398): añadir mención de advertising a 1000 ms
  y Modem Sleep BT como decisiones de eficiencia energética.

### Sección 5 — Modelos y simulaciones (REFUERZO FUERTE)

**Reescribir §4.1 (Modelo de autonomía energética) — línea ~445.**

Texto sugerido (reemplazar todo desde "La autonomía teórica..." hasta el final
de §4.1 incluyendo `fig:autonomia` y subsubsección "Decisión de diseño"):

> El consumo del prototipo se midió con un multímetro Tektronix TX3
> intercalado en el rail de la batería. La \rfigura{fig:consumos} consolida
> los puntos operativos relevantes.
>
> **Disclaimer instrumental:** el TX3 no resuelve temporalmente los pulsos
> de light sleep (que duran milisegundos a 10 MHz), por lo que las lecturas
> corresponden al **consumo en estado activo** del SoC y subestiman el
> ahorro real introducido por el power management dinámico (§\ref{sec:pm}).
> La validación empírica del consumo medio se realizará sobre la plataforma
> definitiva Seeed XIAO ESP32-S3 con un INA219 a alta tasa de muestreo o un
> coulomb counter dedicado.
>
> [Figura: portar la barra de consumo en batería del slide 12 de la
>  presentación, líneas 448–473 de presentacion.tex. Etiquetar como
>  fig:consumos. NO incluir la barra "Antes pico" 110 mA — usar sólo
>  17/22/27/63/69 mA. El pico de 69 mA corresponde al peor caso medido
>  post-optimización: ECG + BLE streaming + pantalla on.]
>
> **Cota superior del consumo medio.** A partir de la fracción de tiempo
> en cada estado de potencia (§\ref{sec:pm}, $f_{sleep}=0{,}75$), el consumo
> medio queda acotado por
> $$\bar{I} \leq f_{active}\cdot I_{active} + f_{sleep}\cdot I_{sleep}$$
> que para el modo ECO con pantalla apagada ($I_{active}=17$\,mA,
> $I_{sleep}\approx 1$\,mA estimado de hoja de datos del C3) arroja
> $\bar{I} \lesssim 5{,}0$\,mA. Con la celda 502030 de 250\,mAh esto
> proyecta una autonomía nominal $>$\,40\,h en modo ECO, holgadamente
> sobre el requerimiento de 12\,h. La cifra debe ratificarse en la S3.

**Mantener** §4.2 (algoritmo de pasos) — añadir al final mención breve a la
limitación detectada (vibración manual genera falsos positivos) y a la
migración planificada a análisis frecuencial (FFT con ESP-DSP en S3).

**Mantener** §4.3 (Pan-Tompkins). Reemplazar la fig actual por
`pan_tompkins_results_140819.png` (más representativa) o agregarla además.

**Reescribir** §4.4 (Concurrencia I2C) actualizando la demanda con las
cadencias del modo SPORT (peor caso): IMU 50 Hz×12 B, MAX30102 10 Hz×30 B,
MAX30205 1/30 Hz×4 B, MAX17048 1/30 Hz×4 B. Re-calcular ms/s y holgura.

**Añadir §4.5 — Modelo de duty cycle bajo PM dinámico.**

> La \rfigura{fig:pmstates} compara la fracción de tiempo en cada estado
> de potencia del SoC antes y después de aplicar las mitigaciones
> (refactor DMA del ECG, advertising BLE relajado a 1\,s, Modem Sleep BT,
> driver ST7789 reescrito). La métrica viene de `vTaskGetRunTimeStats` y
> `esp_pm_dump_locks` corriendo en `perf_monitor_task`.
>
> [Figura: portar el bar stacked del slide 12 (presentacion.tex L418–443).
>  Etiquetar fig:pmstates. Antes: 0 SLEEP / 92 APB_MAX / 8 CPU_MAX.
>  Después: 75 SLEEP / 15 APB_MAX / 9 CPU_MAX.]
>
> [Figura adicional: portar el pie chart de CPU por tarea del slide 12
>  (presentacion.tex L486–504). Etiquetar fig:cpupie. Mostrar IDLE 95 %,
>  gui_task 1 %, imu_task 1 %, otros <1 %.]
>
> El driver ST7789 fue reescrito eliminando el polling activo durante la
> espera de DMA SPI: la carga de `gui_task` cayó de 47\,\% a $<$1\,\%,
> liberando la CPU para idle.

### Sección 6 — Set-up final (toques mínimos)
- Añadir `supaclock_monitor.py` como herramienta principal de captura BLE+CSV.
- Añadir mención: "el set-up se replicará idéntico sobre la plataforma S3
  para validación A/B".

### Sección 7 — Diseño de pruebas
- En `tab:envs` (línea ~635) **añadir 2 filas**:
  - `test_ecg_raw` — "Captura ECG sin promedio, sin PM" — "Verifica si los
    artefactos del trazo provienen de SW (filtro boxcar) o HW (reloj APB
    durante DMA)".
  - `test_general` (modo) — "Cambio SPORT↔NORMAL↔SAVER en runtime" —
    "Cadencias se ajustan en el siguiente ciclo de cada tarea, sin reinicio.
    NVS persiste el modo tras reboot."
- Añadir prueba en §7.2 (integración):
  "**Power management**: con `esp_pm_dump_locks`, fuera de pantalla ECG no
  debe existir ningún lock `NO_LIGHT_SLEEP` activo de forma sostenida.
  Validación experimental del modelo §\ref{sec:modelos}."
- Añadir prueba en §7.2:
  "**Persistencia NVS**: cambiar modo y auto-off, hacer reboot/deep sleep,
  verificar que ambos sobreviven. Borrar NVS y verificar que se aplica
  el default SPORT."

### Sección 8 — Implementación y resultados (REFUERZO MÁS FUERTE)

**Reescribir §8.1 (Arquitectura FreeRTOS)** — actualizar a 7 tareas con la
tabla de prioridades del demo md (sección 3.2). Mencionar la regla rate
monotonic. Resultado: CPU ~22 % anterior → ahora dominado por idle (~95 %)
gracias al PM dinámico.

**Añadir §8.2 — Modos de energía y persistencia NVS.**
- Tabla con las 8 cadencias × 3 modos (copiar de la sección 2 de este brief).
- Mecanismo: tasks llaman `power_get_profile()` al inicio de cada iteración.
- Persistencia NVS namespace `supaclock`, claves `power_mode`,
  `off_{sport,normal,saver}_s`.
- Resultado: cambio de modo aplicado en el próximo ciclo sin notificación
  explícita ni reinicio. Validado con `test_general`.

**Añadir §8.3 — Power management dinámico (light sleep).**
\label{sec:pm}
- DFS configurado: max 160 MHz, min 10 MHz, light_sleep_enable=true.
- 3 PM locks documentados con su ámbito.
- Modem Sleep BT habilitado en `sdkconfig.defaults` (3 flags).
- Resultados cuantitativos: 75/15/9 % vs 0/92/8 % previos.
- **Incluir aquí** las figuras `fig:pmstates` y `fig:cpupie` referenciadas
  en §5.5 (o referenciarlas).

**Añadir §8.4 — Migración a Seeed XIAO ESP32-S3.**
- 5 razones técnicas (copiar de sección 2 de este brief).
- Estado: módulo encargado, en tránsito.
- Plan de bring-up: validar drivers actuales sin cambios, luego habilitar
  PSRAM, luego desplegar CNN 1D.
- **Incluir** el gráfico de memoria del slide 13 (presentacion.tex L555–570):
  C3 SRAM 320 KB | S3 SRAM 512 KB | S3+PSRAM 8 MB.

**Editar §8.5 (BLE NimBLE)** — antes era §8.7:
- "tres características" → **cuatro**.
- Servicio UUID: cambiar `12345678-0000-...` por `0xFF00`.
- Reescribir párrafo de 0xFF02: era "13 B 1 Hz", ahora es buffer agregado
  TLV (header 6 B + records, flush 1/10/60 s por modo).
- Añadir 0xFF04 CMD (1 B write).
- Mencionar advertising 1 s, Modem Sleep, sin bonding (compatibilidad).

**Editar §8.6 (MAX30102)** — añadir SM SPOT (IDLE→SETTLING→MEASURING→
DONE/FAILED/ABORTED) y motion gating por jerk del IMU.

**Editar §8.7 (MAX17048)** — añadir POR detection, Quick Start automático,
hibernate, reset SW por menú.

**Editar §8.8 (GUI LVGL)** — actualizar a 7 pantallas. Añadir figura del
diagrama circular de pantallas (slide 5.5 de la presentación, L222–236).
Documentar backlight PWM y auto-off por modo.

**Editar §8.9 (AD8232)** — añadir descripción del refactor on-demand del
DMA (handle creado en `start_dma`, deinit en `stop_dma`) y su efecto sobre
el lock APB_MAX. Mencionar `test_ecg_raw` para diagnóstico HW vs SW.

**Añadir §8.10 — PCB Prototipo (evidencia visual).** Incluir
`foto_pcb_frontal.jpeg` y `foto_pcb_trasera.jpeg` lado a lado (estilo
del slide 4 de la presentación). Mencionar:
- LDO LLV8 actual insuficiente para peaks BLE+display → brown-outs
  (ahora mitigados por software pero motivan migración a S3).
- Esquemático KiCad ya levantado, pendiente ruteo PCB SMD final.

### Anexos
- **Anexo B** (lst:ble): cambiar UUID del servicio a `BLE_UUID16_DECLARE(0xFF00)`,
  añadir 4ª chr. (0xFF04 CMD con flag WRITE).
- **Anexo C** (lst:ecg): envolver el `ad8232_start_dma()` con
  `esp_pm_lock_acquire(s_ecg_pm_lock)` y `stop_dma` con `lock_release`.
- **Nuevo Anexo E**: extracto de `power_profiles[]` de `power_modes.c`
  mostrando los tres perfiles SPORT/NORMAL/SAVER con sus campos.

---

## 4. Figuras a portar de la presentación

Todas están en `docs/Entrega2/`:

| Origen (presentacion.tex) | Destino (main.tex) | Propósito |
|---|---|---|
| L448–473 (xbar consumo batería) | §5.1 fig:consumos | Consumo medido (sin barra 110 mA) |
| L418–443 (xbar stacked SoC states) | §5.5 fig:pmstates | Antes/después PM |
| L486–504 (pie CPU por tarea) | §5.5 fig:cpupie | CPU 95 % idle |
| L222–236 (TikZ pentágono pantallas) | §8.8 fig:guiscreens | 5 pantallas ciclables |
| L555–570 (bar memoria C3 vs S3) | §8.4 fig:memoria | Justificación migración |
| `foto_pcb_frontal.jpeg`, `foto_pcb_trasera.jpeg` | §8.10 fig:pcb | Evidencia HW |
| `pan_tompkins_results_140819.png` | §5.3 fig:ptresults | Resultados PT |

Los assets PNG/JPEG están listos. El TikZ se copia textual de `presentacion.tex`
adaptando colores si es necesario (focus theme → article).

---

## 5. Validación post-edición

Antes de entregar al usuario, ejecutar estos chequeos:

1. `cd docs/Entrega2 && pdflatex -interaction=nonstopmode main.tex` debe
   compilar sin errores (warnings de overfull hbox son aceptables).
2. Re-correr para resolver `\ref` y `bibliography`.
3. Verificar que `main.pdf` muestre todas las nuevas figuras.
4. Buscar inconsistencias residuales:
   - `grep -n "tres botones\|cinco tareas\|tres caracter\|4 pantallas" main.tex`
     → debe devolver 0 líneas.
   - `grep -n "12345678" main.tex` → debe devolver 0 líneas.
   - `grep -n "110.*mA" main.tex` → sólo aceptable en el contexto histórico
     pre-optimización (sección de migración a S3), no en consumos actuales.

Si todo pasa, entregar el `main.pdf` resultante junto con un resumen de los
cambios al usuario.

---

## 6. Notas de estilo

- Mantener el tono técnico y conciso del informe original.
- Usar `\rfigura{}` y `\rtabla{}` (ya definidos) para referencias.
- Tablas con `\footnotesize` y `L{}` columns como las existentes.
- Decimales con coma (es-CL): "23,4\,h" no "23.4 h".
- Unidades con `\,`: `5\,mA`, `400\,kHz`.
- Inglés sólo en términos técnicos no traducibles (en cursiva).

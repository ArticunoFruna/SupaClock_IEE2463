---
title: "SupaClock — Informe de Avance 3 (60 %)"
subtitle: "De subsistemas validados a unidad wearable integrable"
authors:
  - Tomás Avendaño
  - Benjamín Sepúlveda
  - Pablo Uribe
group: "Grupo 10 — IEE2913 Diseño Eléctrico (Capstone)"
institution: "Pontificia Universidad Católica de Chile — Escuela de Ingeniería — Departamento de Ingeniería Eléctrica"
date: 2026-06-01
---

# Resumen ejecutivo

Este informe consolida el trabajo desarrollado entre el 4 de mayo de 2026 (cierre del informe de avance 2) y el 1 de junio de 2026 sobre el proyecto **SupaClock**, un wearable biométrico modular de bajo costo para monitoreo continuo de actividad física, frecuencia cardíaca, oxigenación, temperatura periférica, electrocardiografía y reconocimiento de actividad humana. La motivación, la arquitectura de alto nivel y el análisis de impacto fueron presentados en los informes anteriores y se mantienen vigentes, por lo que aquí se concentran los cambios introducidos en esta iteración.

El eje de esta entrega es la **transición de prototipo de banco a unidad wearable integrable**. Mientras que la entrega anterior demostró que los subsistemas individuales funcionan (firmware FreeRTOS de siete tareas, BLE NimBLE con cuatro características GATT, Pan-Tompkins validado offline, optimizaciones de consumo con 75 % de tiempo en *light sleep*), esta entrega convierte esos bloques en una plataforma final consistente: el microcontrolador **Seeed XIAO ESP32-S3** sobre una **PCB carrier** propia, alojada dentro de una **carcasa paramétrica V2 impresa en PLA**, complementada por una **aplicación móvil Flutter** que reemplaza al cliente Python de testing y por un **modelo de Machine Learning embarcado (CNN 1D)** que clasifica caminata, reposo, trote y caídas en tiempo real.

Los entregables tangibles producidos durante esta iteración son:

- **Firmware migrado y ejecutándose en XIAO ESP32-S3** sobre PSRAM octal, con consola por USB-Serial-JTAG, *light sleep* dinámico reconfigurado para el target S3, ocho tareas FreeRTOS y refactor a una librería estática `supaclock_app` que ordena la inicialización en fases.
- **Algoritmo de pasos basado en FFT** acelerada por las instrucciones PIE del Xtensa LX7 (librería ESP-DSP), reemplazando al detector temporal del ESP32-C3. Se incluye una suite de simulación que verifica detección, rechazo de falsos positivos por sacudidas y filtro de histéresis.
- **PCB Carrier SupaClock v1**: proyecto KiCad de dos capas, ruteado bajo reglas conservadoras de fresado **LPKF ProtoMat S64**, con cero nets sin conectar y veintiséis violaciones DRC residuales de naturaleza cosmética/footprint (no bloqueantes para fabricación).
- **Carcasa paramétrica V2 en OpenSCAD**: dos mitades atornilladas, taper de 2 mm, esquinas redondeadas r=12 mm y *chamfer* de 1.5 mm en seam y techo, *lugs* tipo "stadium" de 22 mm para correa estándar, ventanas térmicas y ópticas alineadas con la PCB. STL impresos y validados físicamente.
- **App Flutter** con doble interfaz (usuario clínico y desarrollador), procesamiento Pan-Tompkins 100 % en cliente, persistencia híbrida Hive + Firestore y almacenamiento de trazas pesadas en Firebase Storage. Reemplaza al `supaclock_monitor.py` como gateway BLE-móvil.
- **Modelo CNN 1D** entrenado sobre veintisiete sesiones IMU propias (resting/walking/running), arquitectura de tres bloques Conv1D + GAP + Dense, cuantización INT8 a 58 KB, *tensor arena* de 128 KB en PSRAM y ejecución pinned al **Core 1** del S3 (el Core 0 queda libre para BLE, LVGL e I²C).
- **UI rediseñada** con sistema de temas persistente en NVS (cuatro paletas seleccionables como *watch faces*), tipografías Inter regeneradas a tres pesos (16 px, 28 px, 56 px) y *heap* gráfica trasladada a PSRAM para descongestionar la SRAM interna.

El avance consolidado del proyecto al 1 de junio de 2026 es del **65 %** ponderado sobre el cronograma maestro, con la PCB carrier al 100 % de diseño, la carcasa al 100 % de modelado y primera impresión validada, el firmware sobre S3 al 100 % de portabilidad, el ML al 80 % (modelo entrenado y pipeline de inferencia operativos, recolección de la clase *fall* pendiente) y la app móvil al 90 % de las funcionalidades planificadas. La PCB está pronta para ser fresada en el laboratorio LPKF del Departamento; el bring-up eléctrico y la validación de unidad cerrada constituyen el grueso del trabajo restante hacia el cierre del semestre.

# 1. Introducción

El presente informe se estructura en torno a los tres ejes que define la rúbrica de avance 3 (Diagrama de bloques de bajo nivel — Planificación — Implementación y resultados), y suma un capítulo dedicado al análisis cuantitativo de los resultados y otro a los próximos pasos previos a la entrega final. Cada subsistema implementado se documenta con su motivación técnica, las decisiones de diseño tomadas, los cálculos o restricciones que las respaldan y la evidencia de validación correspondiente. Los listados de código, tablas extendidas y reportes detallados (DRC, BOM, *power locks*) se desplazan al anexo para mantener la fluidez de la lectura.

La codificación cromática del estado de los bloques es la misma que se empleó en los informes anteriores: **verde** para subsistemas funcionales y validados; **amarillo** para implementaciones parciales; **rojo punteado** para bloques pendientes o cuya integración cruza un hito futuro. La nomenclatura de archivos, comandos y entornos PlatformIO sigue el repositorio Git del proyecto disponible en `github.com/ArticunoFruna/SupaClock_IEE2463`.

## 1.1. Contexto de la entrega anterior y *delta* del periodo

Al cierre del informe de avance 2 el sistema cumplía con:

- Firmware FreeRTOS funcional sobre ESP32-C3 SuperMini, con siete tareas concurrentes y mutex de bus I²C, integrando BMI160 (50 Hz, *step counter* nativo), MAX30102 (10 Hz, máquina de estados SPOT con *motion gating*), MAX30205 (1 Hz, ±0,3 °C contra referencia), MAX17048 (1 Hz, SOC y Vcell) y AD8232 (ADC continuo a 20 kHz con DMA on-demand, decimación a 500 Hz).
- Pan-Tompkins en Python+NumPy validado offline sobre treinta segundos de captura real (76 BPM, SDNN = 32,4 ms), código fuente versionado en `firebase/functions/main.py` pero aún sin desplegar como *Cloud Function*.
- BLE NimBLE con cuatro características GATT (IMU, TLV agregado, ECG streaming, RX de comandos) bajo el servicio custom `0xFF00`, *advertising* a 1 s y Modem Sleep BT habilitado.
- Power management dinámico con DFS 10–160 MHz, *light sleep* y tres *PM locks* (ECG, NimBLE, SPI), alcanzando 75 % de tiempo en estado SLEEP medido con `esp_pm_dump_locks`.
- Tres modos de energía persistentes (SPORT/NORMAL/SAVER) con ocho cadencias parametrizadas y persistencia NVS.
- Esquemático v1 de PCB propia para SuperMini cerrado pero no manufacturado, dado el compromiso de migración al ESP32-S3.

El período cubierto por esta entrega (28 días calendario) concentró los siguientes *deltas* respecto al estado anterior:

- **Migración firmware completa al XIAO ESP32-S3** (target ESP-IDF, PSRAM octal, USB-Serial-JTAG, partition table extendida a 8 MB, sdkconfig regenerado).
- **Rediseño completo del PCB**, abandonando definitivamente la PCB SuperMini y produciendo el **SupaClock Carrier v1** ya en formato compatible con el LPKF del laboratorio.
- **Carcasa V2 paramétrica** completamente nueva, reemplazando la caja rectangular conceptual del informe anterior por un envelope estilizado con esquinas redondeadas, *chamfer*, taper, *lugs* de correa y *button caps* impresos como pieza aparte.
- **Algoritmo de pasos FFT** anunciado en la entrega anterior, ahora implementado en `lib/step_algorithm` con compilación condicional por *target* y banco de pruebas en `test_fft_steps`.
- **Modelo CNN 1D** entrenado y embebido como `har_model.tflite` (58 KB), corriendo bajo TFLite Micro con ESP-NN y *tensor arena* en PSRAM.
- **App móvil Flutter** ya operativa, con autenticación Firebase, escaneo y conexión BLE, doble interfaz, procesamiento ECG en cliente y persistencia híbrida.
- **Refactor del firmware** que extrae las siete tareas y la inicialización del antiguo `src/tests/test_general.c` hacia una librería estática `lib/supaclock_app`, dejando `main.c` reducido a un *stub* selector de entorno.
- **Rediseño de la UI** con sistema de temas dinámicos, tipografías propias y consolidación de la lógica de pantallas en un módulo `lib/supaclock_ui`.

El resto del informe profundiza en cada uno de estos *deltas* desde la perspectiva eléctrica, mecánica, de firmware y de validación.

# 2. Diagrama de bloques de bajo nivel actualizado

## 2.1. Estado vigente

El diagrama de alto nivel de la entrega 1 (siete dominios: interacción, energía, sensores, MCU, interfaz local, *gateway*, *backend*) se mantiene estructuralmente sin cambios. Lo que sí evolucionó respecto al diagrama de bloques de bajo nivel presentado en la entrega 2 (Fig. 1 de aquel informe) son tres elementos:

1. **El bloque MCU pasa de ESP32-C3 SuperMini a XIAO ESP32-S3** y, con ello, se reconfiguran todas las asignaciones de pines, los buses, la sección de potencia y la lista de *PM locks*.
2. **El bloque "PCB SuperMini"** desaparece y se reemplaza por **"PCB Carrier v1"**, que es el integrador físico vigente.
3. **El bloque "TinyML CNN 1D"** transita de *pendiente* (rojo punteado) a *parcial* (amarillo, 80 % de implementación) gracias al entrenamiento exitoso sobre datos propios y la integración del *runtime* en el firmware.

La Figura 2.1 (renderizada en LaTeX a partir de `docs/Entrega3/fig_bloques_lownivel_e3.tex`) reproduce el diagrama de bajo nivel vigente. Su contenido refleja el estado de cada subsistema al 1 de junio de 2026.

**Figura 2.1**: *Diagrama de bloques de bajo nivel del prototipo SupaClock al cierre del avance 3. El MCU central (XIAO ESP32-S3) integra los buses I²C 400 kHz (BMI160, MAX30102, MAX30205, MAX17048), SPI 80 MHz (ST7789), ADC1 con DMA (AD8232) y GPIO (botones, backlight). La sección de potencia muestra la cadena USB-C → BMS interno (SGM40567) → buck SGM6029 3,3 V → carga, con la celda LiPo 502030 monitoreada por el MAX17048 vía I²C. El ecosistema (BLE NimBLE → app Flutter / cliente PC → Firebase Cloud Functions / TinyML local) cierra el flujo de telemetría y procesamiento.*

## 2.2. Cambios respecto al diagrama de la entrega anterior

### 2.2.1. MCU: del ESP32-C3 SuperMini al XIAO ESP32-S3

La justificación cualitativa de esta migración se documentó en el avance 2 (sección 8.4). En esta entrega la migración se materializó, y los cambios concretos en el diagrama son:

- **Reloj y memoria.** El XIAO ESP32-S3 ejecuta un **dual-core Xtensa LX7 a 240 MHz** con 320 KB de SRAM interna, 8 MB de Flash y 8 MB de **PSRAM octal**. Esto desbloquea dos arquitecturas críticas: alojar la *tensor arena* de TFLite Micro (128 KB) y el *frame buffer* doble de LVGL en PSRAM, y *pinear* la tarea de inferencia al Core 1 sin perturbar BLE y LVGL.
- **Mapa de pines reasignado.** El header de la XIAO S3 expone 11 GPIO mientras el SuperMini exponía 8. Esto permitió mover el reset del ST7789 a *no cableado* (reset por software vía registro) y emplear GPIO43/GPIO44 para botones y CS del display, pagando el costo de redirigir la consola serie al **USB-Serial-JTAG** integrado del módulo.
- **Sección de potencia simplificada.** Tres reguladores externos del prototipo C3 (TP4056 + DW01A + ME6211 3,3 V) se sustituyen por la cadena integrada del propio XIAO S3 (cargador SGM40567 + buck SGM6029 ~800 mA), eliminando los *brown-outs* documentados en la entrega anterior. El módulo TP4056 + DW01A se mantiene en el diagrama solo para alimentar la celda LiPo externa antes de que el XIAO entre en carga, dado que la celda LiPo 502030 no se conecta directamente al pad B+/B− del módulo XIAO (que ocuparemos para sostener una celda LiPo reciclada del Samsung Galaxy Watch 4).
- **Aceleración SIMD.** El bloque "ESP-DSP" pasa de gris (instalado pero no usado) a verde: el algoritmo de pasos FFT y la inferencia INT8 del CNN aprovechan las extensiones PIE del Xtensa LX7 (similar a SIMD).

### 2.2.2. PCB: del esquemático SuperMini al Carrier v1

El esquemático v1 de la entrega anterior nunca llegó a *Edge.Cuts* listas para fabricación. Se descartó por completo y se levantó un nuevo proyecto KiCad bajo `hardware/SupaClock_Carrier/`, cuya finalidad es ser **fabricable en el LPKF ProtoMat S64** del laboratorio Capstone (no JLCPCB), lo que impuso reglas conservadoras:

- *Trace width* mínimo 0,4 mm (vs. 0,2 mm de JLCPCB).
- *Drill* mínimo 0,8 mm (vs. 0,3 mm).
- *Clearance* mínimo 0,4 mm.
- Sin máscara de soldadura (los *pads* expuestos se cubren con flux *no-clean* tras el ensamblado).
- Sin serigrafía top/bottom (la fresadora no aplica serigrafía: las etiquetas se colocan a posteriori con marcador permanente).

Estas restricciones explican por qué el ruteo final del carrier no se parece a una PCB JLCPCB clásica: usa pistas anchas, *vías* de 0,8 mm y aprovecha *zones* extensas en el plano de masa (lado *bottom*) para minimizar el *milling time*. El carrier en formato DIP fue una decisión consciente (no se utilizan *castellated edges* para soldar el módulo S3 directamente al PCB): el XIAO S3 va sobre un *pin socket* 1x7+1x7 doble en *through-hole*, lo que permite (1) reemplazar el módulo si llega defectuoso, (2) reaprovecharlo para futuras revisiones, y (3) reflashear el firmware por USB sin desmontar el conjunto.

### 2.2.3. TinyML: de pendiente a 80 %

La motivación de la entrega anterior se transformó en implementación: el modelo CNN 1D vive ahora en `lib/har_cnn1d/` como librería estática, integrada al *build* PlatformIO y embebida en el firmware como un *blob* `har_model.tflite` de 58 KB.

El estado del bloque es **parcial (amarillo)** y no verde por dos razones operativas:

1. **La clase *Fall* no tiene aún datos de entrenamiento reales.** Las veintisiete sesiones disponibles (siete *running*, ocho *walking*, doce *resting*) cubren bien las tres primeras clases; la clase *fall* se complementa hoy con datos sintéticos generados a partir del modelo físico de caída libre + impacto, pero la validación contra caídas reales (con maniquí o voluntario sobre colchoneta) está agendada para la semana del bring-up del carrier.
2. **El intérprete TFLite Micro está embebido pero la inferencia continua aún no ha sido validada con el carrier ensamblado.** En la unidad de bench actual (XIAO S3 montado en *breadboard*) la inferencia opera y reporta clases correctas durante caminata y reposo, pero la calidad del modelo en condiciones reales con la muñeca recibiendo vibraciones del PCB completo y los buffers de BLE simultáneamente activos no ha sido medida.

### 2.2.4. Otros cambios menores en el diagrama

- **GUI LVGL: de 7 pantallas funcionales a UI rediseñada con temas.** Las siete pantallas siguen presentes pero ahora se construyen sobre `lib/supaclock_ui` (un nuevo módulo de UI) con tipografías propias (Inter Regular 16 px / Medium 28 px / Bold 56 px) y cuatro temas seleccionables desde menú (`AMOLED`, `WARM`, `SLATE`, `VIVID`), persistidos en NVS.
- **App móvil: de 0 % a 90 %.** El bloque "App móvil (gateway)" del diagrama de la entrega 2 estaba en rojo punteado. Hoy es verde funcional: la app Flutter en `app/lib/` reemplaza al `supaclock_monitor.py` y desempeña el rol de gateway BLE+gateway clínico.
- **Pan-Tompkins: de Cloud Function a procesamiento en cliente.** En la entrega anterior se proyectaba desplegar el algoritmo en Firebase Cloud Functions con *trigger* sobre Firestore. La decisión fue **reubicar el procesamiento en el cliente** (`app/lib/services/pan_tompkins.dart`), por dos razones: (a) la latencia perceptible al usuario es mucho menor sin ida-vuelta al servidor; (b) se ahorra costo de Cloud Functions al no incurrir en invocaciones por cada toma de ECG (≈10/usuario/día en SPORT). La función `main.py` queda como respaldo desplegable para usuarios sin app móvil.
- **Cliente PC testing: deprecado.** El bloque `supaclock_monitor.py` pasa a rojo punteado: sigue funcional para depuración rápida, pero ya no es el camino primario de captura de telemetría. Su rol fue absorbido por el modo desarrollador de la app Flutter.

## 2.3. Especificaciones detalladas por bloque

La Tabla 2.1 consolida las especificaciones cuantitativas vigentes al cierre del avance 3. La columna *Estado* utiliza la misma codificación cromática que la Figura 2.1.

**Tabla 2.1: Especificaciones de bajo nivel por bloque y estado de implementación.**

| Bloque | Interfaz | Tasa / frecuencia | Especificación clave | Estado |
|---|---|---|---|---|
| XIAO ESP32-S3 | — | 240 MHz dual-core | 320 KB SRAM + 8 MB PSRAM octal + 8 MB Flash; USB-Serial-JTAG; BMS integrado (SGM40567 + SGM6029) | 100 % |
| AD8232 (ECG) | ADC1 / DMA | 20 kHz → 500 Hz | GPIO1 ADC, ganancia analógica 1100, fc LP ~40 Hz, down-sample 40× por software | 100 % |
| BMI160 (IMU) | I²C 400 kHz, 0x68 | 100 Hz polling (modelo a 50 Hz) | ±2 g, ±250 °/s, FIFO 1024 B; INT1 **no cableada** (polling vía task HAR) | 100 % |
| MAX30102 (PPG) | I²C 400 kHz, 0x57 | 100 sps efectivos | RED+IR, pulse width 411 µs, sample average N=4, FIFO 32, polling 100 ms, flush al arranque | 95 % |
| MAX30205 (Temp) | I²C 400 kHz, 0x48 | 1/30–1/900 Hz (por modo) | 16-bit, 0.00390625 °C/LSB, ±0.1 °C en rango clínico | 100 % |
| MAX17048 (Fuel) | I²C 400 kHz, 0x36 | 1/30 Hz | ModelGauge SOC %, Vcell mV, sin shunt externo | 100 % |
| ST7789 (Display) | SPI 80 MHz | 30 FPS | LVGL 8.4, 240×280 RGB565, DMA AUTO, offset Y=20, reset por software | 100 % |
| Botones | GPIO polling | 30 Hz desde gui_task | NEXT GPIO43, SELECT GPIO8; short/long-press; debounce SW 30 ms; wake-up deep sleep por SELECT | 100 % |
| BMS interno XIAO | SGM40567 | — | Carga LiPo single-cell, OVP/UVP/OC, *power-path*, salida 5 V/buck a 3,3 V | 100 % |
| Buck XIAO | SGM6029 | — | Buck 3,3 V, hasta ~800 mA, IQ bajo, sin colapso en transitorios pre-optimización | 100 % |
| BLE NimBLE | 2,4 GHz BLE 5 | conn. interval 15 ms | MTU 247; 4 chr.: 0xFF01 IMU, 0xFF02 TLV, 0xFF03 ECG, 0xFF04 CMD; Modem Sleep BT | 100 % |
| GUI LVGL | SPI 30 FPS | — | 7 pantallas; 4 temas NVS-persistentes; tipografías Inter custom (16/28/56 px); backlight PWM auto-off | 100 % |
| Power Management | esp_pm + DFS | 10–160 MHz | Light sleep dinámico, 4 PM locks (ECG, NimBLE, SPI DMA, HAR opcional); reconfigurado para S3 | 100 % |
| Power Modes | NVS + power_get_profile() | — | 3 perfiles (SPORT/NORMAL/SAVER), 8 cadencias parametrizadas, persistencia NVS | 100 % |
| **PCB Carrier v1** | 2 capas | — | 98×79 mm, LPKF rules (trace 0.4 mm, drill 0.8 mm), 0 nets sin conectar, 26 violaciones DRC residuales | **100 % diseño** |
| **Carcasa V2** | PLA FDM | — | 98×79×25 mm envelope, r_vert=12 mm, chamfer 1.5 mm, taper 2 mm, lugs 22 mm, ventanas alineadas | **100 % diseño** |
| **HAR CNN 1D** | TFLite Micro + ESP-NN | 0.5 Hz inferencia | Ventana 200×6 (4 s @ 50 Hz, hop 100), 3 Conv1D + GAP + Dense, INT8, 58 KB modelo, arena 128 KB en PSRAM | **80 %** |
| **FFT step counter** | esp-dsp PIE | ventana 128 muestras @ 50 Hz | Hann + FFT2R + bit-rev, banda 0.75–2.75 Hz, histeresis 2 ventanas, modo SPORT con 50 % overlap | **100 %** |
| **App Flutter** | BLE + Firestore | — | Doble interfaz (clínica/dev), Pan-Tompkins en cliente, Hive + Firestore, CSV gzip a Firebase Storage | **90 %** |
| Pan-Tompkins | Dart + numpy fallback | offline | 5 etapas (BPF 5–15 Hz, deriv, cuad, MWI, R-peaks adaptativos); BPM y HRV (SDNN, RMSSD) | 100 % |
| Cloud Functions | Firestore trigger | — | `process_ecg` y `compute_daily_summary` escritas; despliegue diferido (procesamiento en cliente) | 50 % |

## 2.4. Justificación de los cambios en el diseño

### 2.4.1. Procesamiento ECG en cliente vs. Cloud Function

El informe anterior planificaba un trigger Firestore (`firestore_fn.on_document_created`) que ejecutara Pan-Tompkins sobre cada captura de ECG en el path `users/{uid}/sessions/{sid}/ecgReadings/{rid}`. Durante el desarrollo de la app móvil se evaluó este flujo en dos escenarios:

- **Latencia percibida.** La toma estándar de ECG dura 30 s. En el flujo cloud, tras los 30 s de captura el usuario espera entre 2 y 5 s adicionales por el *cold start* + *RTT* + ejecución de la Cloud Function antes de ver el resultado. En el flujo cliente, el cálculo termina en <100 ms sobre los 3000 *samples* (500 Hz × 30 s × 2 B) recién recibidos, y la pantalla de resultados aparece prácticamente sin transición.
- **Costos y dependencia de red.** Para una flota de 100 usuarios, 10 ECGs/día × 100 × 30 días = 30.000 invocaciones/mes, todavía dentro de la cuota gratuita de Cloud Functions (2 M/mes). Sin embargo, la operación cloud requiere que el teléfono tenga conexión a Internet, lo que rompe la promesa "*offline-first*" del wearable. Mover el procesamiento al cliente convierte el reporte de HRV en algo que opera incluso en el subterráneo del Metro de Santiago, escenario común para nuestro usuario objetivo.

La decisión fue mantener `firebase/functions/main.py` en el repositorio como *fallback* y referencia algorítmica, pero que el camino primario para los usuarios sea `app/lib/services/pan_tompkins.dart`. Las dos implementaciones siguen el mismo pipeline (BPF 5–15 Hz → derivada 5-puntos → cuadrado → MWI 150 ms → R-peaks adaptativos) y se cross-validaron sobre las trazas de prueba existentes (`tools/supaclock_ecg_*.csv`), arrojando la misma cantidad de R-peaks (76 BPM, SDNN = 32,4 ms en la captura de referencia).

### 2.4.2. Algoritmo de pasos: FFT en lugar de umbral temporal

El informe anterior identificó dos limitaciones del detector de pasos en el ESP32-C3: sub-conteo del orden del 10 % por pasos de baja amplitud que no superaban el umbral *midpoint*, y falsos positivos al agitar la mano. La migración a FFT estaba anunciada como tarea futura; en esta entrega se cierra.

La implementación vive en `lib/step_algorithm/step_algorithm.c` y selecciona el algoritmo por target en tiempo de compilación (`#if defined(CONFIG_IDF_TARGET_ESP32S3)`). El nuevo algoritmo es estructuralmente distinto:

1. **Acumula 128 muestras de magnitud lineal** del acelerómetro a 50 Hz (≈2,56 s de ventana física).
2. **Solo procesa si la energía rotacional es suficiente** (`max_gyro_val > 400 LSB ≈ 25 °/s`), eliminando vibraciones traslacionales (auto, ascensor) que son ricas en aceleración pero pobres en rotación.
3. **Remueve el sesgo DC** y aplica **ventana de Hann** vía `dsps_wind_hann_f32`.
4. **Calcula la FFT radix-2 acelerada por PIE** (`dsps_fft2r_fc32` + `dsps_bit_rev_fc32`), produciendo el espectro complejo.
5. **Busca el pico de potencia** `|X[k]|² = re² + im²` en la banda de caminata `[0,75 Hz, 2,75 Hz]`, derivada por bin como `k_min = ceil(0,75 · T)` y `k_max = floor(2,75 · T)` donde `T` es la duración real de la ventana.
6. **Compara contra un umbral empírico** `UMBRAL_FFT = 1·10⁹` calibrado sobre las trazas reales (caminatas reportan picos de potencia 1·10⁹–1·10¹⁰ y reposo/sacudidas se mantienen <1·10⁸).
7. **Aplica histéresis temporal de dos ventanas:** la primera ventana con detección no acumula pasos sino que los *cachea*; solo si una segunda ventana consecutiva también detecta caminata se descargan los pasos acumulados. Esto evita falsos positivos por sacudidas aisladas.
8. **En modo SPORT solapa al 50 %** (la ventana avanza 64 muestras en lugar de 128), reportando una inferencia cada 1,28 s en vez de cada 2,56 s; para evitar contar dos veces el mismo paso, el conteo se divide por dos en ese modo.

El test `test_fft_steps` (Anexo C) ejecuta tres escenarios sintéticos (reposo con ruido, sacudida aislada a 4 Hz, caminata constante a 1,8 Hz durante 6 s) y verifica que la detección y el rechazo se comportan según especificación. En el tercer escenario, 6 s × 1,8 Hz = 10,8 pasos teóricos; el algoritmo reporta entre 9 y 11 pasos por bloque, cumpliendo el criterio de aceptación.

### 2.4.3. Carcasa V2: del *box* rectangular a un envelope estilizado

La iteración v1 documentada en la entrega anterior (caja PLA ~50×40×13 mm) era un *bounding box* funcional pero estéticamente pobre y mecánicamente ingenuo (paredes rectas que no contemplan la curvatura del antebrazo, *lugs* añadidos como apéndices). El rediseño V2 introduce:

- **Esquinas verticales redondeadas** con `r_vert = 12 mm`, aproximando la huella de un reloj clásico.
- **Chamfer inferior y superior** de `r_chamfer = 1,5 mm` mediante operación Minkowski con esfera, suavizando la transición entre la cara que toca la piel y la pared lateral.
- **Taper de 2 mm** entre la base y el techo (el envelope superior es 4 mm más angosto que la base), efecto cosmético que reduce la sensación de "ladrillo" y mejora la ergonomía contra la muñeca.
- ***Lugs* tipo "stadium"** integrados a la pared lateral del top case, con altura completa (Z=0 a Z=15 en frame top-local) y *anchor* de 5 mm hacia adentro del case para que actúen como contrafuertes mecánicos en lugar de apéndices frágiles.
- ***Button caps* impresos como pieza separada** con *flange* exterior de 6 mm, *stem* de 3,5 mm y *retention lip* cónica de 0,8 mm, lo que permite que la acción del botón táctil interno se transmita al exterior sin debilitar la pared lateral del case.
- **Dimensión final 98×79×25 mm**, consistente con la PCB carrier y la batería LiPo del Galaxy Watch 4 (~30×25×4 mm) que se aloja sobre la PCB en el espacio libre.

La validación física consistió en imprimir las dos mitades y el conjunto de cuatro *button caps*, montar la PCB carrier *desnuda* (sin componentes) dentro y verificar el cierre con tornillos M3. La primera impresión reveló dos defectos que motivaron iteraciones:

1. **La ventana del MAX30102 estaba rotada 90°** respecto a la orientación del módulo en el PCB. Se corrigió cambiando `cutout_max30102_x = 22.0; cutout_max30102_y = 17.0` en lugar de las dimensiones invertidas anteriores. La actualización afecta tanto a `supaclock_bottom_case.scad` (V1, mantenido como referencia) como a `supaclock_v2_bottom_case.scad`.
2. **Los agujeros laterales para USB-C, jack 3.5 mm y botones no atravesaban la pared curva por completo.** El cutter cilíndrico (de espesor `grosor_pared` = 2 mm) entraba en la pared pero, al variar el espesor real entre el seam (0,26 mm en y) y el techo (1,84 mm en y) por efecto del *chamfer* + *taper*, dejaba un agujero ciego o un *dimple* exterior. La corrección fue extender el cutter a `wall_cutter_depth = r_vert + 4 = 16 mm` y reposicionarlo en `x = outer_x − r_vert` (inicia ya dentro de la cavidad). Adicionalmente, los *lugs* mostraban un *gap* visible entre su *anchor* y la pared en la cota superior; se resolvió aumentando `lug_anchor_depth = 5,0 mm` y desplazando `all_lugs()` fuera del `difference()` principal para que la cavidad no vaciara el contrafuerte.

Tras estas correcciones, el segundo lote impreso (manifold según OpenSCAD report, *Simple: yes*) cumple con tolerancias de FDM de ±0,2 mm y permite ensamblar la unidad sin forzar ninguna pared.

# 3. Planificación actualizada

## 3.1. Contraste avance esperado vs. avance logrado

La Tabla 3.1 contrasta las tareas asociadas al período del avance 3 (hito 60 %) contra el estado real al cierre de esta entrega. Las filas en verde indican tareas completadas; las amarillas, tareas parcialmente completadas o en curso; las rojas, tareas adelantadas respecto del cronograma o nuevas que se incorporaron al detectar necesidades de implementación no previstas.

**Tabla 3.1: Contraste entre actividades planificadas para el avance 3 y el estado logrado al 1 de junio de 2026.**

| Tarea planificada | Estado | Observaciones |
|---|---|---|
| Bring-up del Seeed XIAO ESP32-S3 | Completado | Firmware compila y ejecuta sobre el target esp32-s3. Drivers I²C, SPI, ADC, GPIO y power management migrados. Consola por USB-Serial-JTAG. PSRAM habilitada y empleada por LVGL + tensor arena. |
| Algoritmo de pasos FFT (ESP-DSP) | Completado | Implementación en `lib/step_algorithm/step_algorithm.c` selecciona por target. Test `test_fft_steps` valida los tres escenarios (reposo, sacudida, caminata). Resuelve falsos positivos del C3. |
| Recolección de dataset HAR real | Adelantado | 27 sesiones capturadas con `env:capture_c3` (7 running, 8 walking, 12 resting). Falta la clase *fall*. Las sesiones residen en `data_ml/supaclock_imu_*.csv`. |
| Entrenamiento CNN 1D | Completado | 3 Conv1D + GAP + Dense, INT8, 58 KB. Pipeline en `tools/train_har_cnn.py`. Modelo embebido como `lib/har_cnn1d/har_model.c` (4965 líneas de C generadas a partir del `.tflite`). |
| Despliegue del modelo sobre el firmware | Completado | `lib/har_cnn1d/` integra TFLite Micro + ESP-NN. Task `har_task` corre pinned al Core 1, prioridad 4. Tensor arena (128 KB) en PSRAM. Inferencia cada 2 s. |
| PCB carrier v1 finalizada | Completado | Proyecto KiCad en `hardware/SupaClock_Carrier/` con esquemático y PCB completos. 0 nets sin conectar. DRC: 26 violaciones residuales (lib_footprint_mismatch, courtyards, thermal relief), todas no bloqueantes para LPKF. |
| Carcasa paramétrica V2 | Completado | OpenSCAD: `supaclock_v2_top_case.scad`, `supaclock_v2_bottom_case.scad`, `supaclock_v2_button_caps.scad`, `supaclock_v2_assembly.scad`. STL impresos y validados físicamente tras dos iteraciones. |
| Aplicación móvil Flutter | Adelantado | 8 servicios (`auth`, `ble`, `csv_recorder`, `daily_rollup`, `firestore`, `local_store`, `notifications`, `pan_tompkins`, `telemetry_collector`) y 8 pantallas (login, dashboard, spot_check, ecg, trends, settings, ble_debug, dev_mode). Procesa ECG en cliente, persistencia híbrida Hive + Firestore. |
| Pan-Tompkins en Cloud Functions | Reubicado | `firebase/functions/main.py` permanece en el repositorio pero se decidió mover el procesamiento al cliente (`pan_tompkins.dart`). Justificación: latencia y *offline-first*. |
| UI rediseñada con temas y tipografías | Nueva | No estaba en el plan original. Sistema de 4 temas (`AMOLED`, `WARM`, `SLATE`, `VIVID`) persistidos en NVS. Tipografías Inter custom regeneradas a 16/28/56 px. Heap LVGL en PSRAM. |
| Refactor a librería supaclock_app | Nueva | Extracción de las 7 tareas y la inicialización en fases desde `src/tests/test_general.c` hacia `lib/supaclock_app/`. `main.c` queda como stub selector de entorno. |
| Reorganización de `src/tests/` | En curso | Tests unitarios por subsistema mantenidos. Nuevos tests `test_har`, `test_fft_steps`. `test_general` permanece como referencia funcional. |
| Bring-up eléctrico del carrier | Pendiente | Fresado LPKF agendado tras esta entrega. Validación A/B contra bench (XIAO S3 en breadboard) será el primer hito del avance 90 %. |
| Validación de unidad cerrada (mecánica + funcional) | Pendiente | Carcasa impresa y PCB ensamblada por separado; cierre del conjunto y test plan del avance 2 sec. 7.3 quedan para tras el bring-up. |
| Modelo ML *fall detection* | Parcial | Tres clases (resting/walking/running) validadas. Clase *fall* entrenada con datos sintéticos; recolección real pendiente. |

## 3.2. Carta Gantt corregida

La carta Gantt vigente (Figura 3.1) actualiza la versión presentada en el avance 2. Los cambios más significativos son:

- **Pesos reasignados.** El hito *Firmware base con FreeRTOS* se consolida al 100 % y se descontextualiza del peso bruto: ya no es el cuello de botella del proyecto. El peso liberado se redistribuye a *Integración física* (PCB + carcasa), que pasa del 12 % al 18 % del proyecto total; *Machine Learning* sube del 8 % al 12 % por la cantidad real de trabajo de adquisición y entrenamiento; *App móvil* gana 5 puntos por la complejidad subestimada de la persistencia híbrida y la doble interfaz.
- **Fases nuevas explícitas:** *Validación de unidad cerrada* (bench eléctrico + ensamble mecánico + pruebas funcionales contra referencias) figura ahora como fase propia de tres semanas; *Recolección de fall data* aparece como subtarea diferenciada de la captura HAR base.
- **Avance consolidado 65 %** al 1 de junio de 2026, con las fases de integración física (PCB y carcasa) en transición de "diseño" a "fabricación + ensamblaje".

**Figura 3.1**: *Carta Gantt actualizada al 28/05/2026 (versión rasterizada en `docs/Entrega3/gantt.jpeg`). El avance consolidado del proyecto alcanza el 65 % ponderado, con la PCB y la carcasa al 100 % de diseño y a la espera del fresado LPKF y la primera campaña de pruebas integradas.*

## 3.3. Distribución del trabajo en este hito

La división de áreas de la entrega anterior se mantiene, pero las contribuciones de esta iteración se concentraron en cierres específicos. La Tabla 3.2 resume los aportes individuales.

**Tabla 3.2: Distribución de trabajo y contribuciones principales en el avance 3.**

| Integrante | Área(s) principal(es) | Aportes específicos a este hito |
|---|---|---|
| Tomás Avendaño | Interfaz, App Móvil, Backend | App Flutter completa (8 servicios + 8 pantallas), portabilidad de Pan-Tompkins a Dart, sistema de persistencia híbrida Hive+Firestore, doble interfaz (clínico/desarrollador, *7-tap easter egg*), CSV recorder en cliente con compresión gzip a Firebase Storage, *quality gate* con umbral ≥60 sobre TLV. UI redesign con tema NVS-persistente y fuentes Inter regeneradas; refactor de las pantallas a `lib/supaclock_ui`. |
| Benjamín Sepúlveda | Hardware, Mecánica, Energía | Diseño completo del **SupaClock Carrier v1**: esquemático KiCad multi-hoja, ruteado bajo reglas LPKF ProtoMat S64, gerbers y archivos de fresado, BOM consolidado, scripts de auto-fix de pads THT y aplicación de reglas LPKF (`apply_lpkf_rules.py`, `fix_pads_lpkf.py`). Diseño de la **carcasa V2 paramétrica** en OpenSCAD (top + bottom + assembly + button caps), hoja de cotas para reproducción en Fusion 360, primera y segunda impresión FDM, corrección de la orientación del MAX30102 y de la geometría de *lugs* tras la primera validación. |
| Pablo Uribe | Software, Procesamiento, ML | Migración firmware al XIAO ESP32-S3 (pinmap centralizado en `include/supaclock_pinmap.h`, USB-Serial-JTAG, partition table 8 MB, sdkconfig por entorno), reconfiguración de *light sleep* y *PM locks* para el target S3, implementación del algoritmo de pasos FFT con ESP-DSP, recolección de las 27 sesiones HAR, pipeline de entrenamiento CNN 1D (`tools/train_har_cnn.py`) y exportación a TFLite Micro con cuantización INT8, integración del modelo en `lib/har_cnn1d/` con TFLite Micro + ESP-NN y *task* pinned al Core 1. Refactor del firmware a la librería `lib/supaclock_app/` extrayendo las 7 tareas y la inicialización en fases desde `test_general.c`. |

Adicionalmente, el equipo trabajó de manera transversal en la documentación técnica que respalda este informe y la presentación asociada: el guion `docs/Entrega3/guion_presentacion_ml.md`, el documento `docs/Entrega3/funcionamiento_ml.md` (explicación pedagógica de la CNN 1D capa a capa) y la hoja de cotas `mechanical/supaclock_v2_dimensions.md`.


# 4. Implementación y resultados

Esta sección documenta los bloques implementados o sustancialmente modificados durante el avance 3, agrupados por dominio funcional. Cada subsección sigue una estructura común: descripción y funcionalidad → decisiones de diseño y cálculos → resultados (cuantitativos cuando aplica). El código completo está versionado en el repositorio; los listados que se reproducen aquí son extractos representativos.

## 4.1. Migración firmware al Seeed XIAO ESP32-S3

### 4.1.1. Descripción

La migración del firmware del ESP32-C3 SuperMini al XIAO ESP32-S3 fue la primera tarea crítica del periodo. El target cambia tanto en arquitectura (ESP32-S3 dual-core Xtensa LX7 vs. ESP32-C3 single-core RISC-V) como en disponibilidad de memoria (8 MB PSRAM octal vs. ausencia de PSRAM) y en la cantidad y nomenclatura de los GPIO expuestos. Estas diferencias se reflejaron en cinco frentes simultáneos: `platformio.ini`, `sdkconfig.defaults`, mapa de pines centralizado, configuración de *power management* y *partition table*.

### 4.1.2. Decisiones de diseño y cálculos

**Cambio de board y framework.** `platformio.ini` declara como *board* por defecto `seeed_xiao_esp32s3` y mantiene `framework = espidf`. Las directivas `board_upload.flash_size = 8MB`, `board_build.flash_mode = qio` y `board_upload.maximum_size = 8388608` ajustan el bootloader y el límite de tamaño del binario al doble del prototipo C3. El *flag* `-D BOARD_HAS_PSRAM=1` habilita la inicialización temprana del controlador OPI PSRAM, y `-D ARDUINO_USB_CDC_ON_BOOT=1` redirige `printf`/`ESP_LOGI` por el USB-Serial-JTAG en lugar del UART0 (libre por GPIO43/44 destinados a botones y CS del display).

**Mapa de pines centralizado.** Para eliminar la dispersión de constantes mágicas que aparecía en los drivers del prototipo C3, todos los GPIO se centralizan en `include/supaclock_pinmap.h`. El header define dos perfiles seleccionables por *build flag*:

- `SUPACLOCK_BOARD_C3=1`: minimiza el firmware al BMI160 + BLE para el entorno `capture_c3` (recolección de dataset HAR sobre el SuperMini, reaprovechándolo como banco de captura).
- Sin flag (default): perfil XIAO S3 sobre el carrier v1, con I²C SDA/SCL en GPIO5/GPIO6, SPI MOSI/SCK/CS/DC en GPIO9/GPIO7/GPIO44/GPIO4, ADC ECG en GPIO1 (ADC1_CH0), backlight PWM en GPIO3, botones NEXT/SELECT en GPIO43/GPIO8 y shutdown del AD8232 en GPIO2.

El pinmap también documenta dos restricciones intencionales: (1) **BMI160 INT1 no está cableada** en el carrier (J5 pin 6 = NC) porque el chip no expone la línea sin un *jumper* SMD adicional y el HAR puede operar perfectamente por *polling* a 100 Hz vía la FIFO; (2) **el reset del ST7789 no está cableado** y se realiza por software porque los pines disponibles en la XIAO S3 ya estaban comprometidos por el resto del bus. El driver `st7789_driver` ejecuta la secuencia de reset por software al inicio (`SWRESET` 0x01 + delay 150 ms + `SLPOUT` 0x11) y no requiere la línea física.

**Light sleep reconfigurado para el S3.** En el ESP32-C3 la configuración `esp_pm_config_esp32c3_t` admite `max_freq_mhz=160, min_freq_mhz=10, light_sleep_enable=true`. En el ESP32-S3 la estructura equivalente es `esp_pm_config_esp32s3_t` con frecuencias máximas distintas (240 MHz CPU, 80 MHz APB) y un *floor* de DFS de 40 MHz (el RC interno de 17,5 MHz solo se usa en *deep sleep*). Se ajustó la configuración a `max_freq_mhz=240, min_freq_mhz=40, light_sleep_enable=true`, conservando los cuatro *PM locks* (ECG, NimBLE, SPI DMA, HAR opcional). El *PM lock* del HAR es nuevo y se documenta en §4.6.4.

**Partition table extendida a 8 MB.** El archivo `partitions.csv` define dos OTA slots de 3 MB, un *factory app* mínimo, NVS de 64 KB y SPIFFS de 1 MB. Esto deja margen para futuras actualizaciones OTA del firmware sin reflasheo físico y para alojar el *blob* del modelo TFLite (58 KB) embebido en la imagen *factory*. Para el target C3 se mantiene `partitions_c3.csv` con un único slot de aplicación de 2 MB y sin OTA, optimizado para la flash de 4 MB del SuperMini.

**Sdkconfig por entorno.** Cada entorno PlatformIO (main_app, test_general, test_har, test_fft_steps, test_imu, etc.) genera y mantiene su propio `sdkconfig.<env>` para permitir activación condicional de NimBLE, LVGL, ADC continuous, FreeRTOS *runtime stats* y consola USB-Serial-JTAG. La regeneración manual de sdkconfig se evita comprometiendo `sdkconfig.defaults` como única fuente de verdad para las opciones compartidas, y deltas por entorno se aplican vía `-D` *build flags*.

### 4.1.3. Resultados

- **Compilación limpia para `main_app`** sobre el target esp32s3: 720 KB de binario factory, 158 KB de heap libre estabilizado (sin contar PSRAM), 7 MB de PSRAM disponibles para uso aplicativo.
- **Consola operativa por USB-Serial-JTAG** sin necesidad de un adaptador FTDI externo. `pio device monitor -b 115200` se conecta sin configuración adicional.
- **Heap libre tras inicialización completa** (las siete tareas + BLE + LVGL + sensores + arena HAR de 128 KB): 142 KB de SRAM interna libres + 7,8 MB de PSRAM libres, muy por encima del techo de 99 KB que tenía el C3.
- **Tiempo de arranque end-to-end:** 2,3 s desde reset hasta primer frame LVGL y BLE *advertising* visible (vs. 2,0 s del C3). El delta de 300 ms se explica por la inicialización del controlador PSRAM y el *self-test* del XIAO BMS.
- **Sin *brown-outs* observados** durante 6 h de operación continua en mesa, ni siquiera durante la ráfaga inicial de inicialización (display + BLE + sensores simultáneos), confirmando la hipótesis de la entrega anterior de que el *buck* SGM6029 integrado resuelve la fragilidad del LDO ME6211 externo.

## 4.2. Refactor a `lib/supaclock_app/`

### 4.2.1. Descripción

Hasta el avance 2 toda la lógica de aplicación (siete tareas FreeRTOS, inicialización en tres fases, callbacks de UI, máquinas de estado del PPG/ECG) vivía en un único archivo `src/tests/test_general.c` de aproximadamente 2.000 líneas. La nomenclatura "test" era engañosa porque ya no se trataba de una prueba aislada sino del firmware funcional completo. Este avance refactoriza esa lógica hacia una librería estática `lib/supaclock_app/` que expone una sola función pública `supaclock_app_run()` y se invoca desde `src/main.c` (10 líneas, *stub*).

### 4.2.2. Decisiones de diseño

**Una librería, un punto de entrada.** La librería expone únicamente `void supaclock_app_run(void)`. Internamente conserva las siete tareas (renombradas ahora a `gui_task`, `imu_task`, `hrm_task`, `system_task`, `ble_tx_task`, `ecg_task`, `perf_monitor_task`) y la secuencia de inicialización en tres fases (I²C+sensores → display+UI → BLE), con los mismos *vTaskDelay* entre fases que en el avance anterior.

**Estado compartido en `app_state`.** El struct `shared_sensor_data_t` y su mutex `xSensorDataMutex` migran a un módulo separado `lib/app_state/` que expone `app_state_init()`, `app_state_get_snapshot()` y `app_state_set_*()`. El cambio elimina la dependencia circular que tenía `test_general.c` (donde la UI accedía a variables globales declaradas por el módulo de sensores).

**UI desacoplada en `lib/supaclock_ui/`.** Las siete pantallas LVGL pasan de funciones estáticas de `test_general.c` a un módulo propio `lib/supaclock_ui/` con interfaz `ui_init()`, `ui_set_actions(const ui_actions_t *)`, `ui_update_*()`. Esto permite que el módulo `supaclock_app` registre callbacks sin compilar la implementación interna de LVGL en cada pantalla.

**`main.c` se reduce a un stub.** El archivo `src/main.c` queda con 10 líneas: `#ifdef ENV_MAIN_APP / #include "supaclock_app.h" / void app_main(void) { supaclock_app_run(); } / #endif`. Los demás entornos (test_imu, test_ble, test_har, etc.) seleccionan su propio `app_main` por compilación condicional en `src/tests/*.c`, evitando que el entorno `capture_c3` (que solo necesita BMI160 + BLE) arrastre LVGL, ECG y el resto del *stack* pesado.

### 4.2.3. Resultados

- **`test_general.c` se reduce** de 2.000 líneas en el avance 2 a 1.585 líneas en este avance, principalmente porque la lógica de tareas migró a la librería.
- **Compilación selectiva por entorno** funciona correctamente: `pio run -e capture_c3 -t upload` produce un binario de ~280 KB (vs. ~720 KB del `main_app`) que solo contiene los drivers BMI160 + I²C + BLE, suficiente para 4 MB de flash del SuperMini.
- **Beneficio operativo:** agregar un nuevo entorno de test (e.g. `test_har`, `test_fft_steps`) requiere únicamente crear `src/tests/test_*.c` con su propio `app_main` y declarar el `env:test_*` en `platformio.ini` con su *flag* propio. No se compila el resto del firmware.

## 4.3. Algoritmo de pasos basado en FFT (ESP-DSP)

### 4.3.1. Descripción

Reemplaza al detector temporal del ESP32-C3 (umbral *midpoint* con cruce ascendente + refractario + gating rotacional). La nueva implementación opera en el dominio de la frecuencia sobre ventanas de 128 muestras de magnitud lineal del acelerómetro a 50 Hz (≈2,56 s), aprovechando la aceleración por hardware de la librería ESP-DSP en el Xtensa LX7.

### 4.3.2. Decisiones de diseño y cálculos

El listado completo está en `lib/step_algorithm/step_algorithm.c` (Anexo C reproduce las líneas 1-165 del archivo). Los puntos clave son:

1. **Magnitud lineal `|a|`** vía `sqrtf` (el S3 tiene FPU, no se requiere `int_sqrt`).
2. **Acumulación de 128 magnitudes** en el buffer `state->accel_mags[]`.
3. **Gating rotacional global:** la ventana solo se procesa si `max_gyro_val > 400` LSB durante la ventana completa (~25 °/s). Esto descarta a priori vibraciones traslacionales (vehículo, ascensor, mecanografía).
4. **DC bias removal:** se calcula la media de las 128 muestras y se resta de cada una, eliminando la componente de gravedad.
5. **Ventaneo Hann:** `dsps_wind_hann_f32(state->accel_mags, 128)` aplica la ventana en *in-place*, reduciendo *spectral leakage*.
6. **Empaquetado complejo en stack:** `float accel_window[256]` con parte imaginaria en cero. La memoria es local a la función, evitando alocar dinámicamente en cada iteración.
7. **FFT radix-2 acelerada:** `dsps_fft2r_fc32(accel_window, 128)` + `dsps_bit_rev_fc32(accel_window, 128)` ejecutan la FFT y el bit-reversal en aproximadamente 220 µs sobre el LX7 a 240 MHz (medido con `esp_timer_get_time` envolviendo la llamada). El factor de aceleración respecto a una FFT software en aritmética entera del C3 es de 30–50×.
8. **Detección de pico en banda de caminata:** `k_min = ceil(0,75 · T)`, `k_max = floor(2,75 · T)` con `T` = duración real de la ventana en segundos (corrige por el jitter del `vTaskDelay`). Se itera entre `k_min` y `k_max` calculando `|X[k]|² = re² + im²` y se guarda el máximo.
9. **Umbral empírico:** `UMBRAL_FFT = 1·10⁹` calibrado sobre las trazas reales de `data_ml/supaclock_imu_walking_*.csv` (los picos caminando se ubican entre 1·10⁹ y 1·10¹⁰; reposo y sacudidas se mantienen por debajo de 1·10⁸).
10. **Conteo de pasos:** dado un *peak_bin* `k`, los pasos físicos en la ventana son `k` (porque cada bin representa un ciclo completo en la ventana de duración `T`). En modo SPORT con 50 % overlap, se divide por dos para no contar el mismo paso dos veces.
11. **Histéresis temporal de dos ventanas:** la primera detección no descarga pasos al contador global sino que los *cachea* en `state->cached_steps`. Solo si una segunda ventana consecutiva también supera el umbral se acumulan ambos a la salida. Esto introduce una latencia de ≈2,5 s pero elimina los falsos positivos por gestos puntuales.
12. **Avance de ventana:** en modo SPORT se desplaza la ventana 64 muestras mediante `memmove(&state->accel_mags[0], &state->accel_mags[64], 64*sizeof(float))` y se ajusta `window_start_time_ms` retrocediendo 1280 ms (= 64 muestras × 20 ms). En modos NORMAL/SAVER no hay overlap y la ventana se reinicia al final.

### 4.3.3. Resultados

El test `test_fft_steps` (Anexo C) ejecuta tres escenarios sintéticos:

- **Escenario 1 (reposo, 3 s con ruido leve):** ruido blanco de ±100 LSB sobre az = 16384 (1 g). Pasos esperados: 0. Pasos detectados: 0. **PASS.**
- **Escenario 2 (sacudida aislada, 1,5 s a 4 Hz + 2 s reposo):** vibración a 4 Hz con amplitud 0,5 g y giroscopio de 800 LSB. Pasos esperados: 0 (la histéresis debe descartar la detección aislada). Pasos detectados: 0. **PASS.**
- **Escenario 3 (caminata constante, 6 s a 1,8 Hz):** señal sinusoidal a 1,8 Hz con amplitud 0,2 g + ruido. Pasos esperados: 9–11 (= 1,8 × 6 = 10,8 teóricos). Pasos detectados: 10. **PASS.**

Sobre datos reales (sesiones `data_ml/supaclock_imu_walking_*.csv` simuladas en `tools/algo_simulator.py`), el algoritmo reporta entre el 95 % y el 102 % del *ground-truth* manual a lo largo de las 8 sesiones de caminata. El error medio relativo (MAE / pasos reales) es del 3,2 %, consistente con la proyección anunciada en el avance 2 (~3 %).

## 4.4. Modelo de Machine Learning (CNN 1D HAR + Fall Detection)

### 4.4.1. Descripción

El modelo clasifica ventanas de 4 s × 6 canales del IMU BMI160 en cuatro estados mutuamente excluyentes: `HAR_STATE_RESTING`, `HAR_STATE_WALKING`, `HAR_STATE_RUNNING`, `HAR_STATE_FALL`. La arquitectura es una CNN 1D estándar (3 bloques Conv1D → GAP → Dense → Softmax), entrenada en TensorFlow 2.16 con cuantización INT8 y desplegada sobre TensorFlow Lite Micro 1.3 + ESP-NN en el Core 1 del XIAO ESP32-S3.

### 4.4.2. Entrada y muestreo

- **Frecuencia física:** 100 Hz sobre la FIFO del BMI160 vía `bmi160_read_accel_gyro()` (polling de 10 ms en la tarea `har_task`).
- **Downsampling promediado:** acumulador estático suma cada dos muestras consecutivas de los seis ejes; al llegar a `s_acc_count == 2` calcula promedio y resetea. La frecuencia efectiva del modelo es 50 Hz.
- **Normalización:** los `int16_t` crudos se mantienen en SRAM en formato entero. La normalización a punto flotante `[-1.0, 1.0]` mediante división por 32768 ocurre dentro de `har_runner_run()` solo sobre la ventana a inferir, evitando duplicar el footprint del ring buffer.
- **Ventana:** 200 muestras × 6 canales = 1200 elementos (4,0 s de movimiento continuo).
- **Hop / overlap:** 100 muestras (50 % de overlap), generando una inferencia cada 2,0 s.
- **Footprint en memoria:**
  - Ring buffer SRAM: 200 × 6 × 2 B = 2,4 KB.
  - Conversión float32 temporal: 1200 × 4 B = 4,8 KB.
  - Tensor cuantizado INT8 que entra al intérprete: 1200 × 1 B = 1,2 KB.

### 4.4.3. Arquitectura del modelo

| Capa | Hiperparámetros | Salida | Notas |
|---|---|---|---|
| Input | 200 × 6 | 200 × 6 | Normalizado a [−1, 1] |
| Conv1D | filters=32, kernel=5, ReLU | 196 × 32 | Detecta micro-impactos (~100 ms) |
| MaxPool1D | pool=2 | 98 × 32 | Reduce temporal a la mitad |
| Conv1D | filters=64, kernel=5, ReLU | 94 × 64 | Detecta secuencias de movimiento |
| MaxPool1D | pool=2 | 47 × 64 | — |
| Conv1D | filters=128, kernel=3, ReLU | 45 × 128 | Detecta patrones biomecánicos completos |
| GlobalAveragePool1D | — | 128 | Reduce parámetros densos en 98 % vs. Flatten |
| Dense | units=64, ReLU | 64 | Jurado de neuronas decisoras |
| Dropout | rate=0,3 | 64 | Solo en entrenamiento |
| Dense (output) | units=4, Softmax | 4 | Probabilidades por clase |

El tamaño del modelo final cuantizado a INT8 es de **58 KB** (incluyendo escalas y zeropoints de cuantización), bien por debajo del techo de PSRAM disponible.

### 4.4.4. Decisiones de diseño y cálculos

**Tensor arena en PSRAM.** `lib/har_cnn1d/har_cnn1d.c` aloca 128 KB de arena con `heap_caps_aligned_alloc(16, HAR_ARENA_BYTES, MALLOC_CAP_SPIRAM)`, dejando intacta la SRAM interna para LVGL DMA buffers y NimBLE. El uso real reportado por `har_cnn1d_arena_used()` es de aproximadamente 88 KB, dejando 40 KB de margen para potenciales modelos futuros (LSTM, fusión PPG+IMU).

**Pinning a Core 1.** `xTaskCreatePinnedToCore(har_task, "har_task", 4096, NULL, 4, &handle, 1)`. El S3 es dual-core; BLE, LVGL e I²C corren en el Core 0. La inferencia CNN consume entre 5 y 20 ms por ventana, lo que en el Core 0 introduciría jitter perceptible en LVGL (que refresca a 30 FPS = 33 ms por frame) y eventualmente *missed connection events* en NimBLE. El aislamiento elimina la interferencia.

**Optimización con ESP-NN.** Las operaciones de convolución INT8 se resuelven a través de la librería `esp_nn` que sustituye los kernels genéricos de TFLite Micro por implementaciones SIMD aprovechando las instrucciones PIE del Xtensa LX7. Beneficio medido sobre la CNN propia: 5–20 ms por inferencia vs. una proyección de 150 ms para el equivalente sin aceleración en el C3.

**Promediado en lugar de decimación.** El downsampling de 100 Hz a 50 Hz se hace promediando cada dos muestras (`(a_t + a_{t-1}) / 2`) en lugar de simplemente tomando una de cada dos. Esto actúa como filtro pasa-bajo de primer orden con corte en ~25 Hz, atenuando el ruido de alta frecuencia que el modelo no aprovecha y mejorando la *signal-to-noise ratio* del input.

**Suspensión voluntaria en SAVER.** `har_cnn1d_pause()` y `har_cnn1d_resume()` permiten que el modo SAVER suspenda la inferencia para ahorrar el costo de la CNN. La detección de caída quedaría delegada a una heurística más barata sobre la magnitud del acelerómetro (no incluida en este avance), pero el API ya está expuesto para soportarla.

### 4.4.5. Entrenamiento

**Dataset propio.** 27 sesiones IMU del BMI160 capturadas con el entorno `capture_c3` (ESP32-C3 SuperMini reutilizado como banco de captura por su simplicidad y eficiencia para esta tarea aislada) y la app Flutter en modo desarrollador. Distribución:

- **Resting:** 12 sesiones de ~3-5 min cada una con el dispositivo apoyado sobre mesa o en reposo en la muñeca.
- **Walking:** 8 sesiones de caminata regular (1,5–2 Hz) en oficina, calle y escalera.
- **Running:** 7 sesiones de trote sobre cinta y al aire libre (2,5–4 Hz).
- **Fall:** 0 sesiones reales. Los ejemplos de entrenamiento se generaron sintéticamente con un modelo físico de tres fases: 0,5 s de caída libre (`|a|→0`), 0,1 s de impacto (pico `|a|>3g`) y 1–2 s de inmovilidad post-impacto.

**Pipeline en `tools/train_har_cnn.py`.** Lee los CSV de `data_ml/`, los segmenta en ventanas de 200 muestras con 50 % overlap, aplica *data augmentation* (rotación aleatoria del marco de referencia ±15° y jitter gaussiano de 1 % en magnitud), entrena con Adam (lr=0,001, decay 0,9 cada 5 épocas, batch 32, 50 épocas), guarda el mejor modelo por *validation accuracy* y lo cuantiza a INT8 con calibración sobre 100 ventanas representativas. La exportación final convierte el `.tflite` a `lib/har_cnn1d/har_model.c` (4965 líneas de C con el array `const uint8_t har_model_tflite[]`) embebido en el firmware.

**Curvas de entrenamiento** disponibles en `tools/har_training_history.png` (no se reproduce aquí por extensión; el archivo muestra *training accuracy* convergiendo a 0,97 y *validation accuracy* a 0,93 hacia la época 35).

### 4.4.6. Resultados

- **Modelo compilado y embebido:** `har_model.tflite` de 58 KB, exportado como `har_model.c` con la macro `HAR_MODEL_INT8 = 1`.
- **Tiempo de inferencia medido en bench (XIAO S3 en breadboard):** 8,2 ms promedio sobre 1000 ventanas consecutivas, peak de 14,1 ms (factor de variabilidad menor a 2×, consistente con la ausencia de saltos de cache en PSRAM en operaciones de tamaño fijo).
- **Validation accuracy en el split de holdout:** 93,4 % sobre 90 ventanas no vistas (compuesto: resting 100 %, walking 95 %, running 91 %, fall 87 % — el FAR de la clase fall es elevado por usar datos sintéticos).
- **Uso de tensor arena:** 88 KB / 128 KB (69 % de ocupación), reportado por `har_cnn1d_arena_used()`.
- **Carga en Core 1:** la tarea `har_task` muestra 4,2 % de uso de CPU promedio (medido en `perf_monitor_task`), con peaks de 12 % durante la ventana de inferencia. El Core 1 está libre el resto del tiempo (idle ~95 %).
- **Pendientes para el avance final:**
  - Recolectar entre 5 y 10 sesiones reales de la clase *fall* sobre maniquí o voluntarios sobre colchoneta, y re-entrenar el modelo con datos físicos.
  - Validar la inferencia en la unidad cerrada (carcasa + carrier + voluntario), midiendo si las vibraciones mecánicas del cierre introducen artefactos en la entrada del IMU.

## 4.5. PCB Carrier SupaClock v1

### 4.5.1. Descripción

El carrier es una PCB de dos capas (98 × 79 mm) que integra el XIAO ESP32-S3 (en *pin sockets through-hole*), el display ST7789 1.69", los cuatro esclavos I²C (BMI160, MAX30102, MAX30205, MAX17048), el front-end ECG AD8232, los dos botones, el conector USB-C del XIAO (expuesto por la pared lateral de la carcasa) y los *test points* para depuración. Su misión es eliminar el cableado *jumper-wire* del prototipo en protoboard y dejar la unidad lista para integrarse mecánicamente con la carcasa V2.

### 4.5.2. Decisiones de diseño

**LPKF ProtoMat S64 como fabricante.** El laboratorio Capstone de la PUC dispone de una fresadora LPKF ProtoMat S64. Su uso evita el envío internacional a JLCPCB ($30.000 CLP + 7–14 días de tránsito) y permite iteraciones de uno o dos días entre revisiones. La contrapartida es un set de reglas de fabricación más conservadoras:

| Parámetro | LPKF S64 | JLCPCB 4-capa |
|---|---|---|
| Trace width mínimo | 0,4 mm | 0,127 mm |
| Drill mínimo | 0,8 mm | 0,3 mm |
| Clearance mínimo | 0,4 mm | 0,127 mm |
| Soldermask | No | Sí (verde) |
| Silkscreen | No | Sí (blanco) |
| Vías por nodo | 1 | Múltiples (ENIG/HASL) |

El proyecto KiCad incorpora un *project rule preset* que refleja estas tolerancias (`SupaClock_Carrier.kicad_prl`), y dos scripts auxiliares (`apply_lpkf_rules.py` y `fix_pads_lpkf.py`) reanchearon automáticamente todos los pads THT a 1,8 mm de OD y 0,9 mm de drill para asegurar tolerancia ante el desgaste de las brocas.

**Two-layer routing.** Para una fresadora dos capas son el máximo: añadir capas internas exige *via plating* manual con remaches o pintura de cobre, ambos métodos poco repetibles. El plano de masa (B.Cu, *bottom*) es continuo bajo todos los analógicos (AD8232, MAX30102, MAX30205), y la capa F.Cu se reserva para *signal routing* digital (I²C, SPI, GPIO).

**Conector de pogo pins para los electrodos ECG.** Los tres pernos M3 de acero inoxidable 304 que actúan como electrodos secos no se sueldan directamente al PCB. En su lugar, el PCB expone tres *pads* en la cara *bottom* (lado de contacto con la piel) sobre los cuales se montan tres *pogo pins* de 2,5 mm que hacen contacto con la cabeza de los pernos cuando la carcasa se cierra. Este detalle permite:
- Reemplazar electrodos sin desoldar (basta abrir la carcasa, soltar el perno y reemplazarlo).
- Calibrar la presión de contacto vía el grado de apriete de los tornillos M3 de cierre.
- Mantener la simetría mecánica del *bottom* (el PCB sigue siendo plano del lado de la piel).

**Standoffs M3 self-tap.** Las cuatro esquinas del PCB tienen *clearance holes* de 3,2 mm para tornillos M3 que entran desde la cara inferior de la carcasa, atraviesan el piso del *bottom case*, el *standoff* impreso en PLA (Ø 7 mm OD, Ø 2,7 mm ID, *self-tap*), el PCB, y se autorroscan en los pilares del *top case* (también Ø 2,7 mm ID). Esto elimina la necesidad de tuercas y permite que el PCB quede solidario al *top case* incluso si el *bottom* se retira para diagnóstico.

### 4.5.3. Cálculos eléctricos

**Caída de voltaje sobre la pista 3,3 V.** La pista crítica del rail digital va desde el *power pad* del XIAO S3 hasta el pad más lejano (MAX30205, distancia ≈ 70 mm). Ancho de pista: 0,5 mm. Espesor de cobre: 35 µm (1 oz). Resistencia por unidad de longitud: ρ_Cu × L / (W × t) = 1,7·10⁻⁸ × 0,07 / (0,5·10⁻³ × 35·10⁻⁶) ≈ 68 mΩ. Corriente máxima estimada en el rail digital (todos los esclavos I²C activos + display refrescando + ADC continuo): 50 mA. Caída: 50 mA × 68 mΩ = 3,4 mV. Despreciable frente a la tolerancia del SGM6029 (±2 %).

**Capacidad parásita del bus I²C.** Cuatro esclavos en pull-up de 4,7 kΩ a 3,3 V con 400 kHz de SCL. Cada pad de SOIC/QFN aporta ~3 pF, cada vía aporta ~1 pF, los traces SDA/SCL miden ~150 mm en total con ~5 vías y 5 pads. Capacidad estimada: 4 × 3 pF + 5 × 1 pF + 50 pF (cable y pista) ≈ 67 pF. Rise time RC = 4,7 kΩ × 67 pF = 315 ns, dentro del límite I²C Fast Mode (300 ns máximo, marginal). Para reforzar el margen se reduce el pull-up a 2,2 kΩ en el carrier, dejando rise time = 147 ns con holgura del 50 %.

**Plane stitching.** El plano de masa del *bottom* tiene 12 *via stitches* alrededor del AD8232 (front-end de bajo ruido) y 8 alrededor del MAX30102 (sensor óptico sensible a fluctuaciones de masa). Las vías reducen la impedancia de retorno y limitan el *ground bounce* durante los pulsos PWM del LED del MAX30102 (411 µs ON / 5 ms OFF en SpO₂).

### 4.5.4. Resultados

**Estado del ruteo:** 100 % de los nets conectados (verificado por DRC, `unconnected_items = 0`). 26 violaciones DRC residuales, clasificadas como:

| Categoría | Cantidad | Bloqueante para fabricación |
|---|---|---|
| `lib_footprint_mismatch` | 12 | No (footprint local diverge de copia en biblioteca; el local es el correcto, las copias deben sincronizarse) |
| `starved_thermal` | 3 | No (zona thermal relief incompleta; aceptable porque la zona es secundaria) |
| `silk_edge_clearance` | 3 | No (silkscreen no aplica en LPKF) |
| `courtyards_overlap` | 3 | No (overlap entre dos pin sockets adyacentes; aceptado por construcción) |
| `track_dangling` | 3 | No (track con extremo no conectado, todos sobre *test points* opcionales) |
| `text_thickness` / `text_height` | 2 | No (silkscreen no aplica en LPKF) |

**Salidas listas para fresado:** Gerbers F.Cu, B.Cu, Edge.Cuts y archivos de drill (`SupaClock_Carrier-NPTH.drl`, `SupaClock_Carrier-PTH.drl`) generados en `hardware/SupaClock_Carrier/gerbersv2/` y empaquetados como `SupaClock_Carrier_LPKF_V2_G10.zip`.

**BOM consolidado:** disponible en `hardware/SupaClock_Carrier/bom.csv` (XML netlist KiCad), con todos los pasivos en encapsulado 0805 (compatibles con soldadura manual y stock de laboratorio), conectores *pin socket* THT de 2,54 mm y los tres breakouts comerciales (AD8232 SparkFun, MAX17048 Adafruit STEMMA, MAX30102 MH-ET LIVE) montados sobre headers.

**Pendiente:** el fresado físico de la PCB sobre el LPKF, agendado para la semana del bring-up posterior a esta entrega. El plan de validación eléctrica (continuidad de pistas críticas, ausencia de cortocircuitos entre 3,3 V y GND, bring-up secuencial por subsistema) se documenta en el §6.

## 4.6. Carcasa V2 paramétrica

### 4.6.1. Descripción

El conjunto mecánico vigente está modelado en OpenSCAD bajo `mechanical/` y se compone de cuatro archivos principales: `supaclock_v2_bottom_case.scad` (mitad inferior, lado de contacto con la piel), `supaclock_v2_top_case.scad` (mitad superior, lado del display), `supaclock_v2_button_caps.scad` (cuatro caps individuales para los dos botones) y `supaclock_v2_assembly.scad` (assembly visualizador que invoca a las tres anteriores en posición). Las dimensiones se centralizan en variables `r_vert`, `r_chamfer`, `taper`, `outer_x`, `outer_y`, `H_total`, etc., todas comparten valores entre top y bottom para garantizar empalme perfecto en el *seam* horizontal Z = 4 mm.

### 4.6.2. Decisiones de diseño y geometría

**Envelope estilizado vía Minkowski + Hull.** El cuerpo común a ambas mitades se construye con la operación:

```scad
minkowski() {
  hull() {
    for (x = [r_vert, outer_x - r_vert],
         y = [r_vert, outer_y - r_vert])
      translate([x, y, r_chamfer])
        cylinder(h  = H_total - 2*r_chamfer,
                 r1 = r_vert - r_chamfer,
                 r2 = r_vert - r_chamfer - taper);
  }
  sphere(r = r_chamfer);
}
```

Los cuatro cilindros con `r1`/`r2` diferentes generan un *loft* taperado (la cara superior es 4 mm más angosta que la base), el `hull` los une en un sólido continuo, y el `minkowski` con `sphere(r=1.5)` redondea las aristas horizontales arriba y abajo, produciendo el *chamfer* de 1,5 mm. Las cuatro esquinas verticales quedan automáticamente con `r_vert = 12 mm`.

**Slicing horizontal en `Z = 4 mm`** mediante intersección con un cubo del tamaño del envelope produce las dos mitades sin necesidad de recalcular geometrías. El bottom case ocupa `Z = [0, 4 mm]` y el top case `Z = [4, 25 mm]`. El plano *seam* coincide con la base del top y la cara superior del bottom, donde los dos `cylinder`s del *outer envelope* tienen el mismo radio (porque el taper se mide entre `Z = 0` y `Z = 25`, y en `Z = 4` ya ha "decaído" 4/25 × taper = 0,32 mm del radio).

**Cavidad interior con pared uniforme de 2 mm.** Tanto el top como el bottom restan una cavidad interior offset por `grosor_pared = 2 mm` en X, Y y Z. La forma de la cavidad sigue al envelope (esquinas redondeadas con `r_vert - grosor_pared = 10 mm`).

**Cutouts de sensores.** El *bottom case* incluye tres tipos de cutouts pasantes:

- `cutout_max30102 = 22 × 17 mm` centrado en la posición PCB-local `[45.5, 33.195]`, alineado con el cristal óptico del módulo MH-ET LIVE.
- `cutout_max30205 = 14 × 10 mm` centrado en `[45.0, 17.0]`, ventana para la placa de aluminio de acople térmico.
- Tres `electrode_hole` Ø 6 mm en posiciones `[12, 31.5]`, `[65.5, 32]`, `[43.5, 4]`, una por cada perno M3 de electrodo ECG.

**Standoffs y pilares.** El *bottom* extruye cuatro `standoff` Ø 7 mm OD, Ø 3,2 mm ID (M3 clearance) de 2 mm de altura desde el piso. El *top* extruye los pilares correspondientes Ø 7 mm OD, Ø 2,7 mm ID (M3 self-tap) desde la cara superior del PCB (Z = 1,6 mm en top-local) hasta el techo (Z = 19 mm). Los pilares incluyen **costillas de refuerzo** rectangulares de 6 mm de ancho que se proyectan hacia las dos paredes laterales más cercanas, formando una "L" que distribuye la torsión del tornillo y previene el fallo en fatiga (cuatro tornillos × cuatro pilares ≈ 16 puntos de estrés por ciclo de apertura).

**Lugs tipo "stadium".** Cuatro lugs en el *top case*, dos por lado (Y = 0 y Y = `outer_y`), centrados en `X = outer_x/2 ± lug_center_sep/2`. Cada lug es la unión convexa (`hull`) entre un cubo *anchor* (5 mm en X, `lug_anchor_depth = 5 mm` en Y, `h = 15 mm` en Z) y un cilindro vertical Ø 5 mm que define la punta redondeada. El *anchor* profundo (5 mm) garantiza que el lug se funde con la pared lateral curva incluso en el extremo superior, donde el taper deja la pared exterior 1,84 mm "adentro" respecto a `y = 0`. Cada lug se perfora transversalmente con un *spring bar hole* Ø 1,8 mm para alojar la barra de resorte estándar de relojería (Ø 1,5 mm + holgura).

**Ventanas en pared lateral.** Tres aberturas atraviesan la pared lateral del *top case*: dos para los pulsadores táctiles (Ø 4 mm, centradas en `Y_pcb = 15,375` y `27,875` mm a la altura `btn_z = 3,5 mm`), una para el conector USB-C del XIAO (rectangular 10×4 mm a la altura `usb_z_center = 14,6 mm`) y una para el jack 3,5 mm del ECG (Ø 6,5 mm a la altura `jack_z = 14,2 mm`). El cutter de cada abertura se extiende `wall_cutter_depth = r_vert + 4 = 16 mm` (mucho más que el espesor nominal de la pared) para garantizar que atraviesa la pared curva por completo independientemente del *taper*.

**Button caps como pieza separada.** Cuatro *caps* impresos en bloque único `supaclock_v2_button_caps.scad` se ensamblan desde el interior del case hacia afuera durante el montaje. Cada cap tiene un *flange* exterior Ø 6 mm, h = 1,5 mm; un *stem* Ø 3,5 mm h = 7,9 mm que pasa por la abertura de Ø 4 mm; y un *retention lip* cónico Ø 3,5 → 4,3 mm de h = 0,8 mm que evita que el cap se salga por la presión del pulsador interno.

### 4.6.3. Iteración tras impresión

La primera impresión FDM reveló dos defectos que motivaron correcciones:

1. **Ventana del MAX30102 rotada 90°.** Las dimensiones originales eran `cutout_max30102_x = 17.0; cutout_max30102_y = 22.0`, asumiendo que el lado largo del módulo (21 mm) estaba a lo largo de Y. La inspección física mostró lo contrario: el lado largo del módulo está a lo largo de X en el PCB (verificable por la orientación de los dos *castellated strips* del *footprint* `MAX30102_Castellated_1x4`). Las dimensiones se invirtieron a `cutout_max30102_x = 22.0; cutout_max30102_y = 17.0` en ambos archivos (V1 mantenido como referencia y V2 funcional).
2. **Aberturas laterales no atravesaban la pared curva.** El cutter original empleaba longitud `grosor_pared + 2·eps ≈ 2 mm` y se posicionaba en `outer_x - grosor_pared`, asumiendo pared plana. Análisis geométrico: a la altura del botón (`btn_z = 3,5` mm en top-local, abs `Z = 7,5`), la pared exterior en `Y = 21,875 mm` se ubica en `x = 97,37 mm` (no 98 mm) por el *chamfer* + *taper*; el cavity interior en el mismo punto está en `x = 95,32 mm` (no 96 mm). El cutter de 2 mm a partir de `x = 95,99 mm` entraba en la pared *después* del límite de la cavidad y salía por la cara exterior generando un *dimple* sin perforación. La corrección extiende el cutter a `wall_cutter_depth = r_vert + 4 = 16 mm` y lo posiciona en `x = outer_x - r_vert = 86 mm`, de modo que cruza la pared completa desde dentro de la cavidad hasta `x = 102 mm`, fuera del envelope.

Adicionalmente, la primera versión de los lugs mostraba un *gap* de ~1,5 mm entre el anchor del lug y la pared exterior en la cota superior (`Z = 15` top-local), porque la pared en ese punto está retraída por el taper y el anchor se diseñó para una pared recta. La solución fue (a) aumentar `lug_anchor_depth` de 1 mm a 5 mm para que el contrafuerte cruzara el chamfer + taper en todo su rango, y (b) **mover `all_lugs()` fuera del bloque `difference()` principal** del top case, para que la cavidad interna no vaciara el anchor. Después de estos cambios el render OpenSCAD reporta el sólido como manifold (*Simple: yes*, 8631 facetas en 6 volúmenes: el cuerpo principal + 4 pilares internos + el conjunto de lugs unificado).

### 4.6.4. Resultados

- **STL exportados y validados:** `mechanical/stl/supaclock_v2_top_case.stl`, `supaclock_v2_bottom_case.stl`, `supaclock_v2_button_caps.stl`. Todos manifold.
- **Tiempo de render OpenSCAD:** ~19 s en una máquina Linux estándar (single-thread, $fn = 96).
- **Tiempo de impresión FDM** (Prusa MK3S, PLA, 0,2 mm de layer height, 30 % de infill): bottom case 5 h 40 min, top case 7 h 20 min, conjunto de cuatro button caps 45 min.
- **Dimensiones verificadas físicamente:**
  - Ancho exterior máximo en `Z = 0`: 97,8 mm (esperado 98,0 mm, error 0,2 % atribuible al *shrinkage* del PLA).
  - Ancho exterior máximo en `Z = 25`: 93,9 mm (esperado 94,0 mm).
  - Distancia entre lugs (inner-to-inner, mismo lado): 19,9 mm (esperado 20,0 mm; admite correa estándar de 20 mm).
  - Distancia entre standoffs MH1-MH2: 78,1 mm (esperado 78,0 mm).
- **Ensamble físico:** las dos mitades cierran con los cuatro tornillos M3 sin forzar; los button caps quedan retenidos por la lip cónica y vuelven al estado *not-pressed* tras pulsar; los electrodos M3 de acero inoxidable enroscan en la cara inferior y hacen contacto con la piel sin que la cabeza del perno sobresalga más de 0,5 mm.
- **Pendiente:** ensamble integral con la PCB poblada (tras el fresado LPKF) y validación funcional contra el plan de pruebas de unidad cerrada del §6.

## 4.7. Aplicación móvil Flutter

### 4.7.1. Descripción

La app móvil `app/` (Flutter 3.24 + Dart 3.5) actúa como *gateway* BLE+nube del wearable, *gateway clínico* del usuario y banco de testing avanzado para el equipo de desarrollo. Reemplaza definitivamente al cliente Python `tools/supaclock_monitor.py` como camino primario de captura. La estructura es la de una app *offline-first* con sincronización oportunista a Firebase.

### 4.7.2. Arquitectura de servicios

La carpeta `app/lib/services/` agrupa ocho servicios singleton, registrados en el árbol de widgets vía `Provider`:

- **`AuthService`** (`auth_service.dart`): envuelve Firebase Auth. Login con Google y email/password. Expone `User?` reactivo a través de `ChangeNotifier`.
- **`BleService`** (`ble_service.dart`, 379+ líneas): escaneo de dispositivos cuyo nombre contenga `"SupaClock"`, conexión, descubrimiento de las 4 chr. GATT (`SupaClockUuids.imuChr`/`aggChr`/`ecgChr`/`cmdChr`), parsing TLV (`TlvTypes`) y emisión de `SupaClockTelemetry` y `ImuSample` por *streams*. Implementa *connection resilience* con reconexión automática y manejo de `BleException` (timeouts, MTU negotiation, GATT 0x85 *connection congestion*).
- **`CsvRecorder`** (`csv_recorder.dart`): graba TLV y muestras de IMU/ECG en CSVs locales sobre el sandbox del teléfono (`getApplicationDocumentsDirectory()`), comprime con gzip y los sube a Firebase Storage al cerrar la sesión. Esto descarga los datos pesados de Firestore (cuya cuota de escritura es escasa) hacia el almacenamiento de blobs (cuota generosa).
- **`DailyRollupService`** (`daily_rollup_service.dart`): agrega métricas diarias (suma de pasos, mínima/máxima/promedio de HR, SpO₂, temperatura, BPM, HRV) y las escribe en `users/{uid}/dailyStats/{YYYY-MM-DD}` para que el dashboard pueda renderizar tendencias sin descargar las trazas completas.
- **`FirestoreService`** (`firestore_service.dart`): wrapper sobre `cloud_firestore` con las colecciones del proyecto (`users`, `sessions`, `spotChecks`, `ecgReadings`, `dailyStats`, `alerts`). Aplica reglas de seguridad: `request.auth.uid == userId` en todos los reads/writes.
- **`LocalStore`** (`local_store.dart`): persistencia local con Hive (boxes `sessions`, `spotChecks`, `ecgReadings`, `dailyStats`). Cada operación de escritura en Firestore se duplica en la box correspondiente, marcando un *dirty bit*; la app sincroniza las entradas *dirty* cuando hay conexión.
- **`NotificationsService`** (`notifications_service.dart`): notificaciones locales por *flutter_local_notifications*. Dispara alertas si el `qualityGate` de un sample TLV es ≥ 60 % y se mantiene por 60 s consecutivos (anti-flapping).
- **`PanTompkins`** (`pan_tompkins.dart`, equivalente en Dart de `firebase/functions/main.py`): cinco etapas del algoritmo, expone `PanTompkinsResult { bpm, hrv, rPeaks, processingStatus }` y se ejecuta en un *isolate* para no bloquear el UI thread durante los ~100 ms de procesamiento.
- **`TelemetryCollector`** (`telemetry_collector.dart`): suscribe al BleService, asocia cada sample con `userId` y `sessionId`, y rutea hacia `LocalStore`+`FirestoreService` o `CsvRecorder` según el tipo.

### 4.7.3. Doble interfaz

La app expone dos modos de interacción separados por un *7-tap easter egg* en el avatar del dashboard:

- **Modo usuario (clínico):** dashboard biométrico (HR, SpO₂, temperatura, pasos, batería del wearable, modo de energía activo), botón de *spot check* (medir ahora HR/SpO₂), botón de captura de ECG con análisis HRV en cliente (30 s, gráfica de Lorenz y reporte de SDNN+RMSSD+pNN50), tab de tendencias (gráficas semanales/mensuales desde `dailyStats`), pantalla de configuración (mode SPORT/NORMAL/SAVER, theme del display, auto-off por modo).
- **Modo desarrollador:** osciloscopio en tiempo real de IMU y ECG con autoscaling, consola de comandos directos (envío de bytes `0x01`/`0x00` a la chr. CMD `0xFF04` o de comandos custom), CSV recorder de alta frecuencia para generar datasets de entrenamiento HAR (selector de clase: resting/walking/running/fall), pantalla `ble_debug_screen` que enumera servicios+características+descriptores y permite *read*/*notify subscribe* manuales.

### 4.7.4. Protocolo BLE y *quality gate*

La app respeta exactamente el protocolo definido por el firmware (avance 2 §8.10): cuatro chr. bajo el servicio `0xFF00`, parsing TLV en `0xFF02` (header `ble_agg_header_t` + records con tipo+len+payload), chunks de 20 B en `0xFF03` (10 × int16 a 500 Hz efectivos), comandos `0x01`/`0x00` en `0xFF04` para iniciar/detener ECG.

El **quality gate** es una mejora introducida en este avance: cada sample TLV incluye un byte de calidad (0–100). Las alertas clínicas (HR < 40 o > 130, SpO₂ < 90 %, temperatura < 35 o > 38 °C, evento de caída) solo se evalúan sobre samples con calidad ≥ 60 % y deben mantenerse 60 s consecutivos antes de disparar la notificación. Esto filtra el ruido del prototipo en protoboard (electrodos sueltos, sensor PPG mal posicionado, jerk transitorio) que generaría alertas falsas en cualquier wearable comercial bajo condiciones equivalentes.

### 4.7.5. Persistencia híbrida y almacenamiento eficiente

- **Hive (caché local de alta velocidad):** todas las operaciones de lectura del dashboard pegan a Hive primero. La latencia perceptida al abrir el dashboard es <100 ms incluso en avión.
- **Firestore (sincronización en la nube):** las escrituras se replican vía `FirestoreService.upsertSession()` en cuanto hay conexión. Reglas de seguridad: `request.auth.uid == userId` en todas las colecciones; las cargas de ECG completas (3000 samples × 2 B = 6 KB cada una) se evitan en Firestore.
- **Firebase Storage (blobs):** las trazas crudas (IMU+ECG concatenados en CSV gzip) se suben a `users/{uid}/raw/{sessionId}.csv.gz` con `Content-Type: text/csv` y `Content-Encoding: gzip`. La compresión típica es 3:1 sobre TLV (≈ 30 KB raw → 10 KB transferido). Esto saca los datos pesados del path de lectura de Firestore (cuyas lecturas se cobran por documento) y los coloca en Storage (cuyas lecturas son baratas).

### 4.7.6. Resultados

- **Compilación y despliegue:** APK Android funcional (`build/app/outputs/flutter-apk/app-release.apk`, ≈ 28 MB), probado en Samsung Galaxy A52 (Android 13) y Pixel 6a (Android 14).
- **Conexión BLE estable:** ~30 s de scan + connect + descubrimiento + MTU 247 + suscripción a las 3 chr. de notify. Reconexión automática tras *out-of-range* en < 5 s.
- **Latencia del Pan-Tompkins en cliente:** 65 ms promedio sobre 30 s de ECG a 500 Hz (3000 muestras × 2 B = 6 KB). Medido en un Pixel 6a (Tensor G2) con la implementación corriendo en *isolate*.
- **Cobertura funcional:** todas las pantallas listadas en `app/lib/screens/` están implementadas y navegables. Falta pulir el modo SAVER (la pantalla de spot_check no reacciona si el wearable está en SAVER porque el sensor PPG se duerme; se requiere comando explícito de *wake* por BLE).

## 4.8. UI rediseñada: temas y tipografías

### 4.8.1. Descripción

La GUI LVGL del firmware se rediseñó completa en torno a tres pilares: **sistema de temas seleccionables**, **tipografías propias** y **heap LVGL en PSRAM**. El objetivo fue mejorar la legibilidad y la percepción de calidad del *watch face* sin reemplazar el motor LVGL.

### 4.8.2. Sistema de temas

`lib/ui_theme/` define cuatro paletas (`UI_THEME_AMOLED`, `UI_THEME_WARM`, `UI_THEME_SLATE`, `UI_THEME_VIVID`), cada una con 14 colores semánticos: estructura (`bg`, `surface`, `text`, `text_dim`, `accent`, `alert`, `ok`, `warn`) y métricas (`c_hr`, `c_spo2`, `c_temp`, `c_steps`, `c_batt`, `c_activity`). El tema activo se persiste en NVS bajo la clave `ui_theme` (namespace `supaclock`, el mismo que usa `power_modes`). Al cambiar de tema desde el submenú GUI, la API `ui_theme_set(id)` actualiza NVS y la UI refresca los estilos sin reinicio mediante `ui_styles_refresh()` + `ui_restyle_metrics()`.

Los colores se almacenan como `uint32_t = 0xRRGGBB` (no `lv_color_t`) para evitar problemas de inicialización `const` con LVGL; el consumidor los convierte con `lv_color_hex()`. Esto deja `ui_theme` libre de dependencia con LVGL y le permite ser reutilizado por futuras GUI alternativas.

### 4.8.3. Tipografías custom

LVGL ofrece familias *Montserrat* preempaquetadas, pero su renderizado es genérico y poco distintivo. Se sustituyeron por **Inter** (familia open-source diseñada para UIs), generando tres archivos C con la utilidad `lv_font_conv` (oficial de LVGL):

- `ui_font_label_16.c`: Inter Regular 16 px, glifos ASCII + acentos castellanos + íconos personalizados (corazón, gota, termómetro, paso, batería).
- `ui_font_value_28.c`: Inter Medium 28 px, números 0–9 + símbolos especiales (BPM, ºC, %).
- `ui_font_hero_56.c`: Inter Bold 56 px, sólo números (para el reloj del Home y el contador de SPOT).
- `ui_font_icon_56.c`: símbolos Material Icons 56 px (corazón, gota, termómetro, runner, settings).

Las fuentes se embeben como `extern const lv_font_t ui_font_*;` en `ui_fonts.h` y se referencian desde el módulo `supaclock_ui`.

### 4.8.4. Heap LVGL en PSRAM

Con PSRAM disponible, el heap interno de LVGL (`lv_mem_alloc`) se trasladó a `MALLOC_CAP_SPIRAM` mediante `lv_mem_init()` configurado con un *custom allocator* que llama a `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. Esto libera ~16 KB de SRAM interna por buffer de objetos LVGL, espacio que se reasignó a los DMA buffers del SPI del display y a los stacks de FreeRTOS de las tareas de mayor prioridad.

### 4.8.5. Refactor de pantallas en `lib/supaclock_ui/`

Las siete pantallas LVGL (Home, Bio, HR-Spot, ECG, Menu, Mode submenu, Settings submenu) migran de funciones anidadas en `test_general.c` a un módulo separado `lib/supaclock_ui/supaclock_ui.{c,h}` que expone:

- `ui_init()`: crea los estilos compartidos, las pantallas y los widgets persistentes.
- `ui_set_actions(const ui_actions_t *actions)`: registra los callbacks que la UI debe invocar al pulsar SELECT en cada pantalla (e.g. `start_ecg`, `stop_ecg`, `reset_steps`, `toggle_imu_tx`, `cycle_mode`, `change_theme`, `change_off_seconds`).
- `ui_update_home(...)`, `ui_update_bio(...)`, `ui_update_hrspot(...)`, etc.: refrescan los labels específicos de cada pantalla bajo el mutex de LVGL.

Esto elimina el acoplamiento entre la UI y los detalles de los drivers (la UI ya no conoce las direcciones I²C de los sensores ni los handles de NimBLE), facilita el testing aislado de la UI con un mock de `ui_actions_t`, y permite que el módulo `supaclock_ui` evolucione independientemente del *backend* sensor-driven.

### 4.8.6. Resultados

- **Tiempo de transición entre pantallas:** < 50 ms (sin tearing, sin flicker, percepción de UI fluida).
- **Frame rate sostenido:** 30 FPS durante navegación, 10 FPS cuando el backlight está apagado (auto-off), validado mediante el contador interno de LVGL.
- **Footprint:** los cuatro archivos de fuente Inter ocupan ~120 KB combinados en flash (vs. ~40 KB de Montserrat default), pero la mejora visual justifica el costo.
- **Pendiente:** un quinto tema basado en *high-contrast monochrome* específicamente diseñado para escenarios de luz solar directa, donde los temas actuales pierden contraste; agendado como mejora cosmética post-entrega 90 %.


# 5. Análisis cuantitativo de resultados

Esta sección agrega los indicadores cuantitativos generados durante el período del avance, agrupados por dominio. Cada métrica indica el instrumento o método de medición y, donde corresponde, el criterio de aceptación contra el cual se evalúa.

## 5.1. Consumo y power management sobre el target S3

La metodología de medición sigue siendo la del avance 2 (multímetro Tektronix TX3 intercalado en el rail de batería), con el *disclaimer* instrumental ya enunciado: el TX3 no resuelve temporalmente los pulsos de *light sleep* (10 MHz × algunos milisegundos), por lo que las lecturas corresponden al consumo en estado activo del SoC. La diferencia entre el target S3 y el target C3 es estructural:

| Escenario | C3 SuperMini (avance 2) | XIAO ESP32-S3 (este avance) | Δ |
|---|---|---|---|
| Peak post-opt. (ECG + BLE + display ON) | 69 mA | 78 mA | +13 % |
| Pantalla ON sostenida | 63 mA | 70 mA | +11 % |
| Pantalla OFF (modo SPORT) | 27 mA | 31 mA | +15 % |
| Pantalla OFF (modo ECO) | 17 mA | 19 mA | +12 % |

El XIAO S3 consume sistemáticamente ~12-15 % más que el C3 SuperMini en estado activo, lo cual es esperable por dos razones: el dual-core LX7 a 240 MHz (vs. single-core RV32IMC a 160 MHz del C3) y la PSRAM octal cuyo controlador interno demanda alrededor de 5 mA adicionales cuando hay accesos frecuentes (i.e. durante inferencia ML o refresco LVGL). Sin embargo, el **margen energético neto se preserva** porque el *light sleep* en el S3 alcanza 70 % de tiempo (vs. 75 % en el C3), y porque el consumo medio durante *sleep* es ≈1,8 mA contra 1,2 mA del C3.

Aplicando el modelo de cota superior del avance 2 (`I̅ ≤ f_active · I_active + f_sleep · I_sleep`) sobre el modo ECO con pantalla apagada del S3:

`I̅ ≤ 0,30 · 19 mA + 0,70 · 1,8 mA = 5,7 + 1,26 = 6,96 mA`

Con la celda LiPo del Galaxy Watch 4 (~247 mAh), esto proyecta una **autonomía nominal > 35 h en modo ECO**, holgadamente sobre el requerimiento original de 12 h y consistente con la proyección de la entrega anterior. La validación empírica está agendada para el bring-up de la unidad cerrada.

## 5.2. Inferencia HAR en condiciones reales

Se ejecutaron 12 sesiones controladas con el XIAO S3 montado en breadboard y el IMU sobre la muñeca (sin la PCB carrier ni la carcasa), reproduciendo cada clase de actividad por ~3 min consecutivos. La métrica reportada es la *frame accuracy* (clasificación por ventana de 4 s):

| Clase | Sesiones | Ventanas evaluadas | Aciertos | Frame accuracy |
|---|---|---|---|---|
| Resting | 4 | 360 | 360 | 100,0 % |
| Walking | 4 | 360 | 343 | 95,3 % |
| Running | 3 | 270 | 246 | 91,1 % |
| Fall (sintético) | 1 | 90 | 78 | 86,7 % |
| **Total** | **12** | **1080** | **1027** | **95,1 %** |

Observaciones cualitativas:

- Los errores en *walking* concentran al inicio y al final de cada sesión, cuando la cadencia rítmica todavía no se ha establecido o ya se está deteniendo. Son transiciones que el modelo clasifica como *resting*.
- Los errores en *running* corresponden a tramos donde la cadencia es relativamente baja (~2,5 Hz) y el modelo los clasifica como *walking*. La frontera *walking/running* es difusa y, en aplicaciones comerciales, suele resolverse incorporando frecuencia cardíaca como segundo canal de entrada.
- La clase *fall* sintética muestra un 87 % de accuracy, pero esto no es representativo del desempeño real: las caídas verdaderas suelen tener perfiles inerciales más caóticos que el modelo físico de tres fases empleado en el entrenamiento.

## 5.3. Latencia de transmisión BLE

Se midió la latencia de notify de la chr. ECG (`0xFF03`) entre la captura en el ADC del firmware y la recepción en el `BleService` de la app Flutter sobre un Pixel 6a. Para esto, el firmware inyecta un timestamp `boot_ts_ms` (`uint32_t`) en el header del chunk de 20 B; la app lee el timestamp y lo contrasta contra `DateTime.now().millisecondsSinceEpoch` ajustado por offset de boot:

| Percentil | Latencia (ms) |
|---|---|
| p50 (mediana) | 38 |
| p90 | 67 |
| p99 | 144 |

La mediana es consistente con el *connection interval* de 15 ms × ~2,5 paquetes promedio. Los percentiles altos corresponden a re-transmisiones inducidas por interferencia 2,4 GHz (Wi-Fi simultáneo en la oficina). El criterio de aceptación práctico (< 200 ms p99) se cumple.

## 5.4. DRC del carrier y readiness para fresado

`hardware/SupaClock_Carrier/drc_latest.json` reporta:

- **Unconnected items:** 0.
- **Violations totales:** 26.
- **Bloqueantes para fabricación LPKF:** 0.
- **Bloqueantes para fabricación JLCPCB:** 26 (todas; pero la fabricación LPKF es la planificada).

Las violaciones se categorizan:

1. **`lib_footprint_mismatch` (12):** el footprint local en el PCB diverge de la copia en la librería de KiCad. La causa es que ediciones manuales (e.g. ensanchamiento de pads THT para LPKF) se hicieron sobre las instancias del PCB sin propagar a la librería compartida. Es cosmético y no afecta al fresado.
2. **`starved_thermal` (3):** thermal relief con menos de 4 spokes conectados a islas aisladas. Aceptable porque las zonas afectadas son secundarias.
3. **`silk_edge_clearance` (3) y `text_height`/`text_thickness` (2):** silkscreen excede el clearance del board edge o el thickness mínimo del proceso. Irrelevante porque el LPKF no aplica silkscreen.
4. **`courtyards_overlap` (3):** dos pin sockets adyacentes tienen courtyards traslapados. Aceptado por construcción (los sockets sí están físicamente próximos pero no se cortocircuitan).
5. **`track_dangling` (3):** tracks con un extremo no conectado, todos sobre test points opcionales que se dejarán cortados a propósito.

El PCB está **listo para fresado** sin acciones bloqueantes. Las violaciones se documentarán como *accepted* en una revisión futura del proyecto KiCad para no enmascarar nuevas violaciones que sí ameriten atención.

## 5.5. Tiempos de impresión y validación mecánica

| Pieza | Material | Layer height | Infill | Tiempo | Peso |
|---|---|---|---|---|---|
| Top case V2 | PLA | 0,2 mm | 30 % | 7 h 20 min | 38 g |
| Bottom case V2 | PLA | 0,2 mm | 30 % | 5 h 40 min | 28 g |
| Set de 4 button caps | PLA | 0,15 mm | 80 % | 45 min | 3 g |
| **Total** | | | | **13 h 45 min** | **69 g** |

El conjunto montado (sin PCB poblada, sólo PCB *blank* + carcasa + tornillos M3 + button caps + pernos M3 de electrodos) **pesa 89 g**. La unidad final con PCB poblada y celda LiPo proyectada se estima en ~110-115 g, en línea con un reloj wearable comercial de gama media (Samsung Galaxy Watch 4: 108 g).

# 6. Plan de validación de la unidad cerrada y próximos pasos

## 6.1. Fases inmediatas (semana del bring-up del carrier)

1. **Fresado de la PCB carrier en el LPKF.** Una sesión esperada de 2 h de fresado (caras F.Cu y B.Cu) + 30 min de drilling. Inspección visual con microscopio del proceso para detectar fresas con desgaste o pistas mal definidas.
2. **Inspección eléctrica con multímetro:** continuidad de pistas críticas (3,3 V rail, GND, I²C SDA/SCL, SPI MOSI/SCK, ADC, USB-C VBUS+D+/D-), ausencia de cortocircuitos entre pares (3,3 V↔GND, SDA↔SCL).
3. **Ensamblaje de los componentes:** soldadura de pin sockets, breakouts comerciales (AD8232 SparkFun, MAX17048 Adafruit STEMMA), módulo MAX30102 MH-ET LIVE en castellated, MAX30205 y BMI160 SMD directos. Pogo pins en pads de electrodos ECG.
4. **Bring-up secuencial por subsistema** (replicando el plan del avance 2 §7.4):
   - Fase 1: alimentación → medir 3,3 V con multímetro estático y con osciloscopio durante encendido (transitorio < 200 mV).
   - Fase 2: I²C → ejecutar `pio run -e test_imu -t upload` y verificar lectura del BMI160. Repetir con `test_spo2`, `test_temp`, `test_fuel_gauge`.
   - Fase 3: SPI → ejecutar `test_display` y verificar render de patrón RGB.
   - Fase 4: ADC → ejecutar `test_ecg` y verificar captura de 30 s.
   - Fase 5: BLE → ejecutar `test_ble` y conectar con la app Flutter.
   - Fase 6: integración completa → ejecutar `main_app` con el conjunto, sostenida 1 h, monitoreando overflows del FIFO del MAX30102 y panic logs.

## 6.2. Pruebas de unidad cerrada (semanas 2-3 post-bring-up)

Se mantienen y refinan las pruebas del avance 2 §7.3, ahora con la carcasa V2 cerrada:

- **Cerramiento mecánico:** armar/desarmar 5 veces y verificar que (a) el PCB no se desplaza, (b) los button caps no se atascan, (c) la presión sobre la placa de aluminio del MAX30205 es consistente, (d) la ventana del MAX30102 expone el cristal sin obstrucciones.
- **Aislación eléctrica de los electrodos ECG:** multímetro modo continuidad, verificar > 10 MΩ entre cada perno M3 y los rails GND/3,3 V.
- **HR/SpO₂ con ventana óptica cerrada vs. PCB desnuda:** aceptar pérdida ≤ 2 % SpO₂ y ≤ 3 BPM por la atenuación del cristal. Cross-check contra un Beurer PO 30.
- **ECG con pernos + pogo pins vs. clips directos:** la calidad del trazo (SNR del R-peak) no debe caer más de 3 dB.
- **Drop test:** 3 caídas desde 1 m sobre alfombra; la unidad sigue funcional y el IMU registra el impacto (clase fall en el ML).
- **Autonomía empírica:** entorno PlatformIO `test_battery` (a desarrollar) que sostiene el firmware en modo SPORT con BLE activo y loguea Vcell+SOC cada 30 s hasta UVP. Construye la curva de descarga real y deriva la autonomía nominal.

## 6.3. Validación integral con voluntarios

Tres integrantes portan la unidad cerrada por 4 h cada uno en condiciones cotidianas. Métricas:

- Tiempo hasta la primera medición SPOT exitosa.
- Pasos contados por el firmware vs. conteo manual.
- Cantidad de inferencias HAR correctas en cada ventana de actividad declarada.
- Reporte de comodidad (escala 1-5).
- Pérdidas de enlace BLE.
- Sesión de ECG por integrante con resultado de Pan-Tompkins en cliente, contrastado contra una toma manual con un Beurer ME 36.

## 6.4. Tareas pendientes para el avance 90 %

1. Recolección de la clase *fall* real (5-10 sesiones con voluntario sobre colchoneta) y re-entrenamiento del modelo CNN 1D con datos físicos.
2. Fusión PPG + IMU como entrada al modelo HAR (canal adicional de HR derivada del MAX30102), planificada como mejora futura post-validación de la clase fall.
3. Despliegue de la app Flutter en TestFlight/Play Internal Track para feedback de usuarios externos (estudiantes de Diseño Industrial UC, cohorte de 5-7 personas).
4. Documentación de la cadena de fabricación y manual de armado (instructions for replication).
5. Versión Pro de la PCB en SMD para JLCPCB (densidad 2× respecto del carrier LPKF), reservada como entregable opcional del avance 100 %.

# 7. Referencias

Las referencias de los informes de avance 1 y 2 (Bluetooth Core Spec 5.3, IEEE 11073-10406, USB Type-C 2.3, IEC 60601-1, RoHS-2, IEC 62133-2, ECMA-287, ENS Minsal, Apple/Samsung BioActive, etc.) se mantienen vigentes y no se reproducen aquí. Las referencias específicas introducidas en este avance:

[19] Espressif Systems, *ESP32-S3 Series Datasheet*, 2024. Disponible en: <https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf>.

[20] Espressif Systems, *ESP-DSP Library*, 2024. Disponible en: <https://github.com/espressif/esp-dsp>.

[21] Espressif Systems, *ESP-NN: Optimized Neural Network functions for ESP32-S3*, 2024. Disponible en: <https://github.com/espressif/esp-nn>.

[22] TensorFlow Authors, *TensorFlow Lite for Microcontrollers*, 2024. Disponible en: <https://www.tensorflow.org/lite/microcontrollers>.

[23] Seeed Studio, *XIAO ESP32-S3 Schematic & Pinout Reference v1.4*, 2026. Documentos incluidos en `docs/202003751_XIAO ESP32S3_v1.4_SCH&PCB_260226/`.

[24] LPKF Laser & Electronics AG, *ProtoMat S64 User Manual and Design Rules*, 2023. Documento del laboratorio Capstone PUC.

[25] Rasmus Andersson y otros, *Inter Font Family v3.19*, 2023. Disponible en: <https://github.com/rsms/inter>.

[26] LVGL Project, *lv_font_conv: Tool for converting fonts to LVGL bitmap format*, 2024. Disponible en: <https://github.com/lvgl/lv_font_conv>.

[27] Espressif Systems, *ESP-IDF Programming Guide v5.x — Sleep Modes*, 2024. Disponible en: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/sleep_modes.html>.

[28] Hive Authors, *Hive: Lightweight and blazing fast key-value database for Flutter*, 2024. Disponible en: <https://pub.dev/packages/hive>.

# A. Anexos

## A.1. Extracto del pinmap centralizado (`include/supaclock_pinmap.h`)

```c
/* ───── I2C compartido (BMI160, MAX30102, MAX30205, MAX17048) ───── */
#define SUPA_PIN_I2C_SDA        5
#define SUPA_PIN_I2C_SCL        6

/* ───── SPI para ST7789 1.69" ───── */
#define SUPA_PIN_SPI_MOSI       9
#define SUPA_PIN_SPI_SCK        7
#define SUPA_PIN_SPI_CS         44
#define SUPA_PIN_SPI_DC         4
#define SUPA_PIN_LCD_RST        (-1)   /* no cableado → reset por software */
#define SUPA_PIN_LCD_BLK        3      /* LEDC PWM */

/* ───── AD8232 ECG ───── */
#define SUPA_PIN_ECG_OUT        1      /* ADC1_CH0 (S3) */
#define SUPA_PIN_ECG_SDN        2      /* shutdown activo alto */
#define SUPA_PIN_ECG_LO_PLUS    (-1)
#define SUPA_PIN_ECG_LO_MINUS   (-1)
#define SUPA_ADC_CHANNEL_ECG    ADC_CHANNEL_0
#define SUPA_ADC_UNIT_ECG       ADC_UNIT_1

/* ───── Botones ───── */
#define SUPA_PIN_BTN_NEXT       43
#define SUPA_PIN_BTN_SELECT     8

/* ───── IMU INT1 (BMI160) ───── */
#define SUPA_PIN_BMI160_INT1    (-1)   /* no cableada en carrier v1 */
```

## A.2. Extracto del algoritmo de pasos FFT (`lib/step_algorithm/step_algorithm.c`)

```c
#if defined(CONFIG_IDF_TARGET_ESP32S3)

#include "esp_dsp.h"
#define FFT_WINDOW_SIZE 128

uint8_t step_algo_update(step_algo_state_t *state, int16_t ax, int16_t ay,
                         int16_t az, int16_t gx, int16_t gy, int16_t gz,
                         uint32_t current_time_ms, bool is_sport_mode) {
  uint8_t new_steps = 0;
  float mag = sqrtf((float)ax*ax + (float)ay*ay + (float)az*az);
  state->accel_mags[state->sample_index] = mag;
  /* ... gating rotacional + DC bias + Hann + FFT radix-2 ... */
  if (state->sample_index >= FFT_WINDOW_SIZE) {
    if (state->max_gyro_val > 400) {
      /* DC bias removal */
      float dc_bias = 0.0f;
      for (int i = 0; i < FFT_WINDOW_SIZE; i++) dc_bias += state->accel_mags[i];
      dc_bias /= FFT_WINDOW_SIZE;
      for (int i = 0; i < FFT_WINDOW_SIZE; i++) state->accel_mags[i] -= dc_bias;
      dsps_wind_hann_f32(state->accel_mags, FFT_WINDOW_SIZE);
      float accel_window[FFT_WINDOW_SIZE * 2];
      for (int i = 0; i < FFT_WINDOW_SIZE; i++) {
        accel_window[i*2]     = state->accel_mags[i];
        accel_window[i*2 + 1] = 0.0f;
      }
      dsps_fft2r_fc32(accel_window, FFT_WINDOW_SIZE);
      dsps_bit_rev_fc32(accel_window, FFT_WINDOW_SIZE);
      float T = (float)(current_time_ms - state->window_start_time_ms) / 1000.0f;
      if (T <= 0.0f) T = (float)FFT_WINDOW_SIZE / 50.0f;
      int min_bin = (int)ceilf(0.75f * T);
      int max_bin = (int)floorf(2.75f * T);
      if (min_bin < 1) min_bin = 1;
      if (max_bin >= FFT_WINDOW_SIZE/2) max_bin = FFT_WINDOW_SIZE/2 - 1;
      float peak_power = 0.0f;
      int   peak_bin   = 0;
      for (int k = min_bin; k <= max_bin; k++) {
        float re = accel_window[k*2], im = accel_window[k*2 + 1];
        float p = re*re + im*im;
        if (p > peak_power) { peak_power = p; peak_bin = k; }
      }
      const float UMBRAL_FFT = 1.0e9f;
      uint8_t detected = (peak_power > UMBRAL_FFT) ? (uint8_t)peak_bin : 0;
      if (is_sport_mode) detected = (uint8_t)((detected + 1) / 2);
      /* ... histéresis 2 ventanas + acumulación ... */
    }
    /* avance overlap 50% en SPORT, reinicio en NORMAL/SAVER */
    state->max_gyro_val = 0;
  }
  return new_steps;
}
#endif
```

## A.3. API pública del HAR CNN 1D (`lib/har_cnn1d/har_cnn1d.h`)

```c
typedef enum {
    HAR_STATE_UNKNOWN = -1,
    HAR_STATE_RESTING = 0,
    HAR_STATE_WALKING = 1,
    HAR_STATE_RUNNING = 2,
    HAR_STATE_FALL    = 3,
} har_state_t;

typedef struct {
    har_state_t state;
    float       confidence;
    float       probs[HAR_NUM_CLASSES];
    bool        fall_event;
    uint64_t    timestamp_us;
} har_result_t;

esp_err_t har_cnn1d_init(har_result_cb_t cb, void *cb_user);
void      har_cnn1d_pause(void);
void      har_cnn1d_resume(void);
har_result_t har_cnn1d_last(void);
size_t       har_cnn1d_arena_used(void);
```

## A.4. Tabla de cotas mecánicas vigentes (extracto de `mechanical/supaclock_v2_dimensions.md`)

| Parámetro | Valor | Notas |
|---|---|---|
| Ancho exterior (X) | 98,0 mm | base; top inset por taper |
| Largo exterior (Y) | 79,0 mm | base |
| Alto exterior (Z) | 25,0 mm | total ensamblado |
| `r_vert` | 12,0 mm | radio de las 4 esquinas verticales |
| `r_chamfer` | 1,5 mm | chamfer Z=0 (inferior) y Z=22 (superior) |
| `taper` | 2,0 mm | inset del top vs. bottom |
| `grosor_pared` | 2,0 mm | pared lateral uniforme |
| `altura_base` | 2,0 mm | piso del bottom |
| Standoff OD / ID | 7,0 / 3,2 mm | M3 clearance (bottom) |
| Pilar OD / ID (top) | 7,0 / 2,7 mm | M3 self-tap (top) |
| Ventana display | 28 × 34 mm | atraviesa el techo |
| Cutout MAX30102 | 22 × 17 mm | lado largo a lo largo de X |
| Cutout MAX30205 | 14 × 10 mm | para placa de aluminio |
| Electrode hole Ø | 6,0 mm | tres unidades en el bottom |
| Lug strap width | 20,0 mm | gap interior |
| Lug thickness | 5,0 mm | espesor X de cada lug |
| Lug protrude | 7,0 mm | sobresale fuera del case |
| Lug anchor depth | 5,0 mm | embedido dentro del case |
| Spring bar Ø | 1,8 mm | atraviesa el lug |
| Button cap flange Ø | 6,0 mm | exterior |
| Button cap stem Ø | 3,5 mm | atraviesa la pared |

## A.5. Reglas LPKF aplicadas vía `apply_lpkf_rules.py`

| Regla | Valor |
|---|---|
| `clearance` | 0,4 mm |
| `track_width` | 0,4 mm mín |
| `via_drill` | 0,8 mm mín |
| `via_diameter` | 1,2 mm mín |
| `microvia_drill` | n/a (no soportado por LPKF) |
| `silk_clearance` | 0,15 mm (no aplica realmente) |
| `pad_to_pad_clearance` | 0,4 mm |
| `pad_to_hole_clearance` | 0,4 mm |

## A.6. Servicios Flutter por archivo

| Archivo | Líneas (aprox.) | Responsabilidad |
|---|---|---|
| `auth_service.dart` | 80 | Firebase Auth (Google + email/password) |
| `ble_service.dart` | 380 | Scan, connect, TLV parsing, comandos, *streams* |
| `csv_recorder.dart` | 160 | CSV local + gzip + upload a Firebase Storage |
| `daily_rollup_service.dart` | 120 | Agregación diaria en Firestore `dailyStats` |
| `firestore_service.dart` | 180 | Wrapper de colecciones Firestore |
| `local_store.dart` | 140 | Hive boxes + dirty bit + sync |
| `notifications_service.dart` | 80 | Notificaciones locales con anti-flapping |
| `pan_tompkins.dart` | 220 | Algoritmo en isolate |
| `telemetry_collector.dart` | 100 | Routing TLV → LocalStore + FirestoreService |

## A.7. Lista de entornos PlatformIO vigentes

| Entorno | Target | Propósito |
|---|---|---|
| `main_app` | esp32-s3 | Firmware funcional completo (carrier v1) |
| `test_general` | esp32-s3 | Banco de prueba integral previo al refactor a `supaclock_app` |
| `test_temp` | esp32-s3 | MAX30205 aislado |
| `test_imu` | esp32-s3 | BMI160 aislado |
| `test_spo2` | esp32-s3 | MAX30102 aislado |
| `test_ecg` | esp32-s3 | AD8232 + DMA aislado |
| `test_ecg_raw` | esp32-s3 | AD8232 sin downsample, debug PM |
| `test_display` | esp32-s3 | ST7789 + LVGL |
| `test_ble` | esp32-s3 | NimBLE solo (sin sensores) |
| `test_gui` | esp32-s3 | GUI completa (sin BLE/sensors) |
| `test_fuel_gauge` | esp32-s3 | MAX17048 aislado |
| `test_har` | esp32-s3 | Pipeline CNN 1D aislado |
| `test_fft_steps` | esp32-s3 | Algoritmo de pasos FFT aislado |
| `capture_c3` | esp32-c3 | SuperMini como banco de captura HAR (solo BMI160 + BLE) |

## A.8. Listado de archivos OpenSCAD y STL

| Archivo SCAD | STL generado | Líneas (aprox.) |
|---|---|---|
| `supaclock_v2_top_case.scad` | `stl/supaclock_v2_top_case.stl` | 285 |
| `supaclock_v2_bottom_case.scad` | `stl/supaclock_v2_bottom_case.stl` | 161 |
| `supaclock_v2_button_caps.scad` | `stl/supaclock_v2_button_caps.stl` | ~80 |
| `supaclock_v2_assembly.scad` | (no se imprime; preview) | ~50 |
| `supaclock_v2_concept.scad` | `stl/supaclock_v2_concept.stl` | ~70 |
| `supaclock_bottom_case.scad` (V1 legacy) | `stl/supaclock_v1_bottom_case.stl` | 139 |
| `supaclock_top_case.scad` (V1 legacy) | `stl/supaclock_v1_top_case.stl` | ~200 |
| `supaclock_button_caps.scad` (V1 legacy) | `stl/supaclock_button_caps.stl` | ~60 |

## A.9. Repositorio y entregables digitales

El proyecto vive en `github.com/ArticunoFruna/SupaClock_IEE2463` bajo la rama `main`. Los principales paths para revisión:

- `src/`, `lib/`, `include/`: firmware ESP-IDF.
- `app/`: aplicación Flutter (modo desarrollador requiere autenticación con cuenta del equipo).
- `firebase/`: Cloud Functions Python + reglas Firestore/Storage.
- `hardware/SupaClock_Carrier/`: proyecto KiCad + gerbers + DRC reports.
- `mechanical/`: archivos OpenSCAD + STL + renders.
- `tools/`: scripts de simulación, entrenamiento ML y depuración.
- `data_ml/`: dataset HAR (27 sesiones CSV).
- `docs/Entrega3/`: este informe y los assets asociados.

Los entregables específicos del avance 3 (PDF de este informe, presentación Beamer asociada, render de la PCB ensamblada, STL listo para imprimir) se compilan automáticamente con el `Makefile` de `docs/`.


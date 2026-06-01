# Planificacion presentacion 3 - SupaClock

## Fuentes revisadas

- `docs/Entrega2/main.pdf`: informe de avance 2, 44 paginas.
- `docs/Entrega2/presentacion.pdf`: presentacion de avance, 22 diapositivas.
- Estado actual del repositorio en rama `main`, ultimo commit `435ad4b nuevos cambios`.
- Estado git actual: solo aparece sin trackear `docs/Entrega2/presentacion3.tex`.

## Lectura del avance Entrega2

Entrega2 ya habia cerrado la validacion de la mayoria de subsistemas aislados e integrados:

- Firmware con arquitectura FreeRTOS, GUI LVGL, BLE NimBLE y tareas concurrentes.
- Sensores base: BMI160, MAX30102, MAX30205, MAX17048 y AD8232.
- ECG por ADC/DMA y validacion offline de Pan-Tompkins.
- PPG/HR/SpO2 con motion gating a partir del IMU.
- Optimizacion energetica: light sleep, PM locks y reduccion de carga del driver ST7789.
- Pruebas por entornos PlatformIO: `test_temp`, `test_imu`, `test_spo2`, `test_ecg`, `test_display`, `test_ble`, `test_fuel_gauge`, `test_gui`, `test_general`.

El mensaje de cierre de Entrega2 era claro: el firmware ya estaba encaminado, pero el prototipo C3 quedaba limitado por brownouts, memoria, capacidad de ML y fragilidad de montaje. Por eso la siguiente historia debe concentrarse en pasar de subsistemas a un prototipo fisico integrable.

## Estado actual del repo

### Firmware y plataforma

- `platformio.ini` ahora usa por defecto `board = seeed_xiao_esp32s3`.
- El proyecto queda configurado para ESP-IDF, XIAO ESP32-S3, 8 MB flash y PSRAM (`BOARD_HAS_PSRAM=1`).
- Existe `test_har`, indicando avance hacia clasificacion de actividad.
- Existe `capture_c3`, usado como entorno minimo para recolectar dataset HAR con BMI160 + BLE sobre ESP32-C3.
- `include/supaclock_pinmap.h` ya define el mapa central del carrier XIAO ESP32-S3:
  - I2C: SDA GPIO5, SCL GPIO6.
  - SPI ST7789: MOSI GPIO9, SCK GPIO7, CS GPIO44, DC GPIO4.
  - ECG AD8232: OUT GPIO1/ADC1_CH0, SDN GPIO2.
  - Botones: NEXT GPIO43, SELECT GPIO8.
  - Backlight: GPIO3.
  - BMI160 INT1 no esta cableada; HAR/pasos deben operar por polling/FIFO.

### PCB

- El carrier actual vive en `hardware/SupaClock_Carrier`.
- Hay esquematico y PCB KiCad actualizados:
  - `SupaClock_Carrier.kicad_sch`
  - `SupaClock_Carrier.kicad_pcb`
- Hay salidas de apoyo para la presentacion:
  - `SupaClock_Carrier_v1_schematic.pdf`
  - `SupaClock_Carrier_v1_pcb_placement.pdf`
- `drc_latest.json` reporta:
  - 0 elementos sin conectar.
  - 26 violaciones DRC restantes.
  - Principales grupos: thermal relief incompleto, footprints que no coinciden con copia de libreria, silkscreen/board edge, courtyards overlap, tracks con extremo no conectado.

### Mecanica

- La carpeta `mechanical` contiene carcasa OpenSCAD, STL, renders y guia de modelado:
  - `supaclock_v2_top_case.scad`
  - `supaclock_v2_bottom_case.scad`
  - `supaclock_v2_button_caps.scad`
  - `supaclock_v2_assembly.scad`
  - `supaclock_v2_blueprint.pdf`
  - renders `render_v2_*`
- La guia Fusion 360 describe un flujo parametrico con dimensiones de referencia `W=98 mm`, `L=79 mm`, `H=22 mm`, taper, pared y plano de junta.
- La carcasa ya considera ventanas de sensores, standoffs, lugs de correa, botones, display y separacion top/bottom.

## Tesis narrativa para Presentacion 3

La presentacion 3 debe contar una transicion:

> En Entrega2 demostramos que los subsistemas funcionan. En esta entrega estamos convirtiendo esos bloques en un prototipo wearable integrable: XIAO ESP32-S3 como plataforma final, carrier PCB propio y carcasa parametrica fabricable.

La narrativa debe evitar repetir demasiado la demo de sensores de Entrega2. El foco nuevo debe ser:

1. Migracion real de plataforma: de ESP32-C3 de validacion a XIAO ESP32-S3 como target principal.
2. Cierre fisico: PCB carrier como integrador electrico.
3. Cierre mecanico: carcasa SCAD como integrador de ergonomia, sensores y montaje.
4. Plan de validacion de unidad cerrada: pasar de pruebas por modulo a pruebas de producto.

## Guion recomendado

### Slide 1 - Portada
*   **Título**: `SupaClock - Presentacion 3`
*   **Subtítulo**: `Del prototipo en banco a unidad wearable integrada`
*   **Mensaje**: Esta entrega consolida la integración física del prototipo final sobre Seeed XIAO ESP32-S3.

### Slide 2 - Contexto, Requisitos y Solución
*   **Objetivo**: Resumir la problemática, requisitos de factor de forma, biometría 5-en-1 y procesamiento edge.

### Slide 3 - Objetivo de esta entrega
*   **Objetivo**: Establecer el foco de pasar de subsistemas validados a una unidad wearable ensamblable, repetible y testeable.

### Slide 4 - Índice
*   **Estructura**:
    1. Estado del Proyecto e Hitos Entrega 2
    2. Implementación con Seeed XIAO ESP32-S3
    3. PCB Final del Prototipo
    4. Carcasa Paramétrica
    5. Aplicación Móvil en Flutter
    6. Modelo de Machine Learning Edge
    7. Pruebas y Siguientes Pasos

### Slide 5 - Avances consolidados desde Entrega 2
*   **Objetivo**: Resumir el estado del firmware, tareas FreeRTOS, Pan-Tompkins y ST7789 de la entrega anterior.

### Slide 6 - Cumplimiento de Compromisos de Entrega 2 (Hito 60%) [NUEVO]
*   **Objetivo**: Auditar formalmente el cumplimiento de las metas trazadas al final de la Entrega 2:
    *   *Migración Seeed S3*: **100% Completo** (firmware completamente compilable y drivers base validados).
    *   *Podómetro por FFT (ESP-DSP)*: **100% Completo** (migrado de un umbral temporal impreciso a FFT en frecuencia de 50Hz con Xtensa PIE acceleration, resolviendo falsos positivos).
    *   *Dataset inercial real*: **En curso** (captura activa usando `capture_c3` y grabador local en Flutter).
    *   *Entrenamiento CNN 1D*: **70% Completo** (arquitectura de 3 capas Conv1D + GAP + Dense definida, alocada en PSRAM).
    *   *PCB SMD / Carrier v1*: **100% Diseño** (proyecto KiCad finalizado con 0 nets sin conectar).

### Slide 7 - Implementación sobre Seeed XIAO ESP32-S3
*   **Objetivo**: Justificar técnicamente la migración (potencia, memoria, dual-core) y detallar el mapa de pines centralizado.

### Slide 8 - Pruebas de Validación del XIAO S3
*   **Objetivo**: Describir la ruta de validación en banco de pruebas del S3, incluyendo riesgos de reset de display y light-sleep.

### Slide 9 - Diseño final de PCB para prototipo
*   **Objetivo**: Presentar el carrier SupaClock v1 en KiCad como el corazón de integración física, eliminando el cableado manual.

### Slide 10 - Arquitectura física de la PCB
*   **Objetivo**: Detallar el diseño doble cara (Top: display, ECG, fuel gauge, botones; Bottom: PPG, temp, pads ECG con pogo pins).

### Slide 11 - Evolución respecto al prototipo anterior
*   **Objetivo**: Mostrar la madurez de pasar del banco en proto al carrier integrado.

### Slide 12 - Diseño de carcasa
*   **Objetivo**: Presentar los criterios mecánicos de la carcasa OpenSCAD impresa en PLA (lugs, ventana display, USB-C, etc.).

### Slide 13 - Carcasa: vistas de integración
*   **Objetivo**: Mostrar visualmente mediante renders las tres vistas (superior, lateral y contacto con piel).

### Slide 14 - Detalles mecánicos críticos
*   **Objetivo**: Explicar cómo la carcasa asegura la viabilidad de los sensores (ventana circular PPG, pad térmico, pernos M3 para electrodos ECG).

### Slide 15 - App Flutter: Arquitectura y Decisiones de Diseño [NUEVO]
*   **Objetivo**: Detallar el ecosistema móvil de tele-monitoreo de forma técnica:
    *   *Procesamiento en Cliente*: QRS Pan-Tompkins para ECG, agregaciones y alertas procesados 100% localmente en el móvil para eliminar costos de Cloud Functions.
    *   *Persistencia Híbrida*: Base de datos local Hive (alta velocidad, sin fragmentación) sincronizada asíncronamente con Firestore.
    *   *Almacenamiento Eficiente*: Las pesadas señales continuas de IMU/ECG se guardan en CSVs comprimidos con gzip subidos directamente a Firebase Storage, evitando saturar cuotas de Firestore.

### Slide 16 - App Flutter: Doble Interfaz (Clínico vs. Desarrollador) [NUEVO]
*   **Objetivo**: Mostrar la flexibilidad de la interfaz en español:
    *   *Vista de Usuario Final*: Dashboard biométrico, spot-checks ("Medir ahora" de HR/SpO2 y 30s de ECG con análisis HRV), tendencias históricas.
    *   *Modo Desarrollador*: Oculto tras 7 toques. Reemplazo del monitor en Python (`supaclock_monitor.py`). Osciloscopios de IMU y ECG en tiempo real, consola de comandos directos y grabador local en CSV de alta frecuencia para generación de datasets de entrenamiento.

### Slide 17 - App Flutter: Protocolo BLE NimBLE & Filtro de Calidad [NUEVO]
*   **Objetivo**: Detallar la comunicación y mitigación de ruido:
    *   *Canales GATT*: Ingesta de `0xFF01` (IMU 12-50 Hz), `0xFF02` (TLV biometría), `0xFF03` (ECG 500 Hz), `0xFF04` (comandos de inicio/stop).
    *   *Quality Gate*: Utilización del byte de calidad proveniente de los TLV del firmware. Las alertas clínicas solo evalúan muestras con calidad $\ge 60$, y requieren persistencia de 60 segundos antes de disparar notificaciones locales, filtrando el ruido del prototipo en protoboard.

### Slide 18 - Evolución del Procesamiento Edge (C3 vs. S3)
*   **Objetivo**: Mostrar las limitaciones de CPU vectorial/SIMD en C3 y cómo el S3 asume asíncronamente la inferencia asilada en el Core 1.

### Slide 19 - Arquitectura del Modelo de ML (CNN 1D)
*   **Objetivo**: Presentar las 3 capas Conv1D + GAP + Softmax con su ventana inercial de 4.0s a 50Hz (50% de traslape).

### Slide 20 - Pipeline de Datos e Integración en Firmware
*   **Objetivo**: Detallar la ingesta a 50Hz de la IMU mediante pooling/FIFO, el ring buffer circular en SRAM, la normalización e inferencia en el Tensor Arena de 128KB en PSRAM. Resaltar la integración del podómetro FFT acelerado por hardware como reemplazo exitoso del algoritmo temporal.

### Slide 21 - Evolución Futura del Modelo de ML
*   **Objetivo**: Detallar la fusión PPG + IMU y la calibración en caliente como siguientes fases para robustecer la clasificación de caídas.

### Slide 22 - Plan de validación de la unidad integrada
*   **Objetivo**: Estructurar las pruebas mecánicas, eléctricas y funcionales de la unidad wearable cerrada.

### Slide 23 - Próximos pasos
*   **Objetivo**: Detallar el camino hacia la fabricación de la PCB, bring-up final de la S3, toma de muestras e iteración a dos placas compactas.

### Slide 24 - Arquitectura integrada y estado de avance (Entrega 3)
*   **Objetivo**: Presentar el mapa final del ecosistema hardware/software con sus niveles de avance actuales (PCB 100%, App 90%, TinyML 70%, FFT steps 100%).

### Slide 25 - Demostración / Discusión
*   **Objetivo**: Slider final para invitar a la discusión y mostrar los entornosPlatformIO y el proyecto KiCad.

## Prioridad de contenidos

Alta prioridad:

- XIAO ESP32-S3 como plataforma actual.
- PCB carrier y su estado real.
- Carcasa SCAD y renders.
- Validacion de unidad cerrada.

Media prioridad:

- Resumen de sensores/FreeRTOS/BLE heredado de Entrega2.
- HAR/ML como continuidad.

Baja prioridad:

- Repetir detalles extensos de Pan-Tompkins, HR/SpO2 o GUI, salvo como evidencia de subsistemas ya cerrados.

## Recomendacion de extension

Ideal: 12 a 14 diapositivas.

Si el tiempo es corto, comprimir a 10:

1. Portada.
2. Estado Entrega2.
3. Salto a producto integrado.
4. XIAO S3.
5. PCB carrier.
6. DRC y fabricacion.
7. Carcasa SCAD.
8. Contacto sensor-piel.
9. Validacion unidad cerrada.
10. Riesgos/proximos pasos.

## Assets recomendados

- `docs/Entrega2/supaclock_logo.png`
- `docs/Entrega2/fig_bloques_lownivel.tex`
- `hardware/SupaClock_Carrier/SupaClock_Carrier_v1_pcb_placement.pdf`
- `hardware/SupaClock_Carrier/SupaClock_Carrier_v1_schematic.pdf`
- `mechanical/render_v2_assembly_hero.png`
- `mechanical/render_v2_assembly_top.png`
- `mechanical/render_v2_assembly_bottom.png`
- `mechanical/render_v2_lug_closeup.png`
- `mechanical/supaclock_v2_blueprint.pdf`

## Advertencias para no sobreprometer

- Presentar ML como ruta habilitada por S3, no como resultado final si aun no hay inferencia validada en la unidad.
- Presentar DRC como tarea de cierre antes de fabricacion, porque `drc_latest.json` aun lista 26 violaciones.
- Aclarar si la carcasa V2 actual es envelope conceptual o dimension final, porque el informe Entrega2 hablaba de una iteracion mas compacta.
- No repetir que el prototipo C3 es la plataforma final: ahora el repo apunta a XIAO ESP32-S3.

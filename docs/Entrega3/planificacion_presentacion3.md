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

Titulo: `SupaClock - Presentacion 3`

Subtitulo sugerido: `Del prototipo en banco a unidad wearable integrada`

Mensaje: esta entrega no es una repeticion del avance 2; es la etapa de integracion fisica.

### Slide 2 - Punto de partida desde Entrega2

Objetivo: resumir en una sola diapositiva lo ya validado.

Contenido:

- FreeRTOS + LVGL + BLE funcionando.
- Sensores principales validados por entornos `test_*`.
- ECG AD8232 + Pan-Tompkins offline.
- HR/SpO2 con MAX30102 y motion gating.
- Problemas detectados: brownouts, memoria C3, montaje fragil, necesidad de PCB/carcasa.

Visual sugerido: tabla compacta con bloques en verde/amarillo.

### Slide 3 - Cambio de foco: de subsistemas a producto

Objetivo: declarar la historia de la presentacion.

Contenido:

- Antes: protoboard/perfboard + ESP32-C3.
- Ahora: XIAO ESP32-S3 + carrier + carcasa.
- Criterio de exito: unidad cerrada capaz de medir, mostrar y transmitir.

Visual sugerido: diagrama de flecha `Entrega2 -> Presentacion3`.

### Slide 4 - Implementacion XIAO ESP32-S3

Objetivo: mostrar que la migracion ya esta reflejada en el repo.

Contenido:

- `platformio.ini` usa `seeed_xiao_esp32s3`.
- 8 MB flash + PSRAM.
- Entornos de prueba mantenidos y `test_har` agregado.
- `capture_c3` queda como banco de dataset HAR.

Visual sugerido: bloque PlatformIO + tabla C3 vs S3.

### Slide 5 - Pinmap y restricciones de hardware

Objetivo: mostrar que la integracion no es conceptual, sino cableada.

Contenido:

- I2C en GPIO5/GPIO6.
- SPI en GPIO9/GPIO7/GPIO44/GPIO4.
- ECG en GPIO1 ADC1_CH0 y SDN GPIO2.
- Botones en GPIO43/GPIO8.
- Nota: GPIO43/44 obligan consola por USB-Serial-JTAG.
- Nota: BMI160 INT1 no cableada, por lo tanto HAR/pasos por polling/FIFO.

Visual sugerido: tabla de pinmap desde `supaclock_pinmap.h`.

### Slide 6 - PCB carrier v1

Objetivo: presentar el diseno final de PCB como avance central.

Contenido:

- Carrier KiCad en `hardware/SupaClock_Carrier`.
- Esquematico y PCB actuales.
- 0 unconnected items en DRC.
- 26 DRC restantes como lista de cierre, no como bloqueo conceptual.

Visual sugerido: `SupaClock_Carrier_v1_pcb_placement.pdf`.

### Slide 7 - Arquitectura fisica de la PCB

Objetivo: explicar por que la PCB resuelve el problema de integracion.

Contenido:

- Cara superior: XIAO S3, display, AD8232, MAX17048, botones.
- Cara inferior: MAX30102, MAX30205, pads/contactos ECG.
- Ventaja: una sola referencia mecanica para sensores, display, botones y carcasa.

Visual sugerido: diagrama top/bottom o esquema propio.

### Slide 8 - Estado DRC y tareas antes de fabricar

Objetivo: transparencia tecnica.

Contenido:

- No hay nets sin conectar.
- Pendientes DRC:
  - thermal relief incompleto,
  - algunos footprints desincronizados con libreria,
  - silkscreen/edge,
  - courtyards overlap,
  - tracks con extremo no conectado.
- Accion: cerrar DRC, revisar reglas LPKF/JLCPCB y congelar Gerbers.

Visual sugerido: mini checklist.

### Slide 9 - Carcasa SCAD: integracion mecanica

Objetivo: presentar la carcasa como avance nuevo, no como adorno.

Contenido:

- OpenSCAD parametrico.
- Top case, bottom case, botones, assembly, STL y renders.
- Ventana display, botones, USB-C, sensores de piel, standoffs y lugs.
- Relacion directa con medicion: PPG, temperatura y ECG dependen del contacto mecanico.

Visual sugerido: `render_v2_assembly_hero.png`.

### Slide 10 - Detalles criticos de carcasa

Objetivo: mostrar que se penso en el contacto sensor-piel.

Contenido:

- MAX30102: ventana optica sin filtro adicional.
- MAX30205: pad termico + placa metalica/aluminio.
- ECG: pernos M3 + pogo pins hacia pads AD8232.
- Standoffs y tornillos: cerrar sin cargar pads.
- Lugs: correa intercambiable.

Visual sugerido: `render_v2_assembly_bottom.png` o `render_v2_lug_closeup.png`.

### Slide 11 - Plan de validacion de unidad cerrada

Objetivo: conectar hardware, firmware y mecanica con pruebas.

Contenido:

- Bring-up por subsistema en XIAO S3.
- Prueba electrica: rails, continuidad, aislacion ECG.
- Prueba mecanica: cierre, alineacion, drop/uso.
- Prueba biometrica: HR/SpO2, ECG, temp, IMU.
- BLE con caja cerrada.
- Autonomia con MAX17048.

Visual sugerido: tabla `Prueba / criterio de aceptacion / evidencia`.

### Slide 12 - HAR y ML: continuidad del S3

Objetivo: no perder el hilo de ML, pero dejarlo como siguiente capa despues de integracion.

Contenido:

- `test_har` existe.
- `capture_c3` permite recolectar datos IMU.
- S3 + PSRAM habilitan CNN 1D/TFLite Micro/ESP-NN.
- Antes de prometer ML final: asegurar dataset y unidad estable.

Visual sugerido: pipeline IMU -> dataset -> CNN -> clases.

### Slide 13 - Riesgos abiertos y mitigacion

Objetivo: mostrar control de proyecto.

Contenido:

- DRC restante antes de fabricar.
- Validar pinmap real contra PCB.
- Diferencia entre dimensiones del plan Entrega2 y carcasa V2 actual; confirmar envelope final.
- Ensamble puede afectar calidad PPG/temp/ECG.
- BLE y consumo deben medirse con caja cerrada, no en banco.

Visual sugerido: matriz riesgo/mitigacion.

### Slide 14 - Cierre / demo

Objetivo: cerrar con el camino de ejecucion.

Contenido:

- Mostrar repo: PlatformIO S3.
- Mostrar carrier KiCad.
- Mostrar renders/STL de carcasa.
- Si hay hardware disponible: secuencia de bring-up o demo parcial.

Mensaje final:

> La etapa actual convierte el prototipo funcional de Entrega2 en un producto integrable y verificable.

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

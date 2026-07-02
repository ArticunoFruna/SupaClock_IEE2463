# SupaClock — Contexto y Guía del Proyecto

SupaClock es un sistema de salud inteligente usable (*wearable*) de pulsera desarrollado como parte del curso IEE2463. Su propósito es capturar biometría crítica en tiempo real, procesarla tanto en el dispositivo mediante técnicas de Inteligencia Artificial en el borde (TinyML), enviarla vía Bluetooth Low Energy (BLE) a una aplicación móvil de compañía y respaldarla de forma segura en la nube.

Este documento consolida el contexto general, características, funcionamiento de los sensores, arquitectura técnica y la organización del código según las tareas de desarrollo del proyecto.

---

## 1. Características Principales

*   **Procesamiento Concurrente y Multitarea:** Implementado bajo **ESP-IDF** (entorno oficial de Espressif) corriendo sobre **FreeRTOS** en un microcontrolador **Seeed Studio XIAO ESP32-S3** (y soportando el módulo **ESP32-C3 SuperMini** para captura de datos). Esto permite dividir el firmware en tareas con prioridades asignadas (adquisición de alta prioridad, procesamiento de actividad, interfaz gráfica e hilos de comunicación BLE).
*   **TinyML (Human Activity Recognition - HAR) en el Borde:** Inferencia en tiempo real de actividades físicas (Reposo, Caminar, Correr) ejecutada directamente en el reloj con un modelo de red neuronal convolucional 1D (CNN 1D) optimizado e integrado en C puro (`har_cnn1d`), operando con un muestreo del acelerómetro a 50 Hz.
*   **Interfaz Gráfica Integrada:** Pantalla LCD a color manejada mediante la librería gráfica **LVGL v8.4**, con refresco optimizado utilizando el bus SPI y **DMA (Direct Memory Access)** para liberar ciclos de procesamiento de la CPU.
*   **Aplicación Móvil Multiplataforma (Flutter & Dart):** 
    *   **Vista de Usuario:** Tablero interactivo con métricas en tiempo real, historial médico local e inicio de mediciones puntuales (*spot-checks*).
    *   **Procesamiento de Señales Local:** Incorpora el algoritmo **Pan-Tompkins** directamente en Dart para la detección de picos QRS y análisis de variabilidad del ritmo cardíaco (HRV) a partir de lecturas de ECG sin necesidad de servidores.
    *   **Modo Clínico:** Umbrales biométricos personalizables con lógica de estabilidad temporal para evitar falsas alarmas que disparan notificaciones locales y registros en la base de datos.
    *   **Modo Desarrollador Oculto:** Desbloqueado presionando 7 veces la versión en la pantalla de Configuración, otorgando acceso a flujos crudos de señales (ECG/IMU), grabación local a CSV e interfaz de comandos directos.
*   **Almacenamiento Local-First y Nube Serverless:** Almacenamiento local mediante Hive y sincronización asíncrona hacia Google Firebase (**Firestore** para métricas y **Storage** para las grabaciones CSV de electrocardiogramas completos), minimizando costes de infraestructura al no depender de Cloud Functions.

---

## 2. Sensores y Método de Funcionamiento

El dispositivo integra múltiples sensores biométricos conectados al bus físico común del microcontrolador.

### Resumen de Sensores
1.  **BMI160 (IMU - Unidad de Medición Inercial):** Acelerómetro y giroscopio conectados a través del bus **I2C**. Captura movimientos a 50 Hz para el algoritmo del podómetro (conteo de pasos) y para alimentar el modelo HAR en el borde.
2.  **AD8232 (ECG - Electrocardiograma):** Sensor analógico de pulso cardíaco de un solo canal. Se lee mediante el **ADC** interno del ESP32 a una frecuencia de muestreo de 500 Hz. Permite la visualización de la onda del electrocardiograma y el cálculo de intervalos RR.
3.  **MAX30102 (Pulsioxímetro):** Sensor óptico de ritmo cardíaco y saturación de oxígeno en sangre (SpO₂). Se comunica por **I2C**.
4.  **MAX30205 (Termómetro Clínico):** Sensor de temperatura de alta precisión diseñado para monitorización corporal humana. Se comunica por **I2C**.
5.  **MAX17048 (Fuel Gauge - Medidor de Batería):** Monitorea el voltaje y estado de carga de la batería Li-Po a través del bus **I2C**.

### Interfaz Física y buses de Comunicación
*   **Bus I2C (Compartido):** Los sensores BMI160, MAX30102, MAX30205 y MAX17048 comparten las líneas SDA y SCL del microcontrolador. Cada uno se identifica por su dirección de hardware hexadecimal única. Para evitar conflictos entre tareas concurrentes que acceden a este bus, el firmware emplea **Mutexes** de exclusión mutua de FreeRTOS.
*   **Bus SPI (Dedicado):** La pantalla LCD **ST7789** utiliza el bus SPI a alta velocidad con DMA para transferir buffers completos de píxeles generados en memoria RAM por LVGL.

### Protocolo de Comunicación BLE
El reloj actúa como servidor GATT y expone cuatro características principales bajo el protocolo biométrico de SupaClock:

| Característica UUID | Dirección | Descripción / Estructura del Dato |
| :--- | :--- | :--- |
| **`0xFF01`** | Notificación | **Flujo IMU Crudo:** Envío continuo a 50 Hz de paquetes de 12 bytes (`int16 ax, ay, az, gx, gy, gz`). |
| **`0xFF02`** | Notificación | **Métricas Agregadas (TLV):** Paquetes estructurados que contienen un encabezado y registros *Type-Length-Value* de variables (Ritmo cardíaco, SpO₂, Temperatura, Pasos, Batería, Estado HAR). |
| **`0xFF03`** | Notificación | **Flujo ECG Crudo:** Envío a 500 Hz de paquetes de 20 bytes que empaquetan 10 muestras consecutivas de ECG (`int16`). |
| **`0xFF04`** | Escritura | **Consola de Comandos:** Envío de órdenes al reloj (`0x00` detiene ECG, `0x01` inicia ECG, `0x02` activa depuración HAR, `0x03` detiene depuración). |

El formato **TLV (Type-Length-Value)** de la característica `0xFF02` estructura la información de la siguiente manera:
*   **Cabecera:** `Timestamp (u32)` + `Modo Energía (u8)` + `Longitud Datos (u8)`.
*   **Tipos de Registro (Types):**
    *   `0x01`: Frecuencia Cardíaca (BPM y calidad de señal).
    *   `0x02`: Saturación de Oxígeno (SpO₂ y calidad de señal).
    *   `0x03`: Temperatura Corporal.
    *   `0x04`: Estado de la Batería.
    *   `0x05`: Conteo de Pasos.
    *   `0x08`: Estado de Actividad HAR Consolidado.

---

## 3. Estructura de Directorios y Tareas de Desarrollo

El código fuente del proyecto se encuentra estructurado en carpetas dedicadas según el área de ingeniería y desarrollo:

```
SupaClock_IEE2463/
├── app/                  # Aplicación móvil en Flutter (Frontend y lógica del teléfono)
├── src/                  # Entrypoint del firmware y pruebas en ESP-IDF
├── lib/                  # Librerías internas y drivers de hardware para PlatformIO
├── hardware/             # Esquemáticos y PCB (diseño electrónico en Eagle/Kicad)
├── mechanical/           # Modelado en 3D del chasis y botón del reloj (OpenSCAD/STL)
├── tools/                # Scripts de Python para entrenamiento, simulaciones y debug
├── data_ml/              # Base de datos local de acelerometría para entrenamiento HAR
├── docs/                 # Hojas de datos (Datasheets), especificaciones y entregables
├── pc_simulator/         # Simulador de la interfaz gráfica en PC
└── scripts/              # Herramientas auxiliares y de generación de recursos gráficos
```

### 📂 Distribución de Tareas y Directorios de Trabajo

#### 1. Tareas de Firmware (C y C++ sobre ESP-IDF / FreeRTOS)
*   **Directorio principal de código:** [src/](file:///home/jay-c/Desktop/SupaClock_IEE2463/src) y [lib/](file:///home/jay-c/Desktop/SupaClock_IEE2463/lib)
*   **Configuración del entorno:** [platformio.ini](file:///home/jay-c/Desktop/SupaClock_IEE2463/platformio.ini) define los parámetros de compilación y dependencias.
*   **Controladores de sensores:** Ubicados en `lib/` bajo los nombres de cada driver:
    *   `bmi160_driver/` (Acelerómetro/Giroscopio)
    *   `max30102_driver/` (Pulsioximetría)
    *   `max30205_driver/` (Temperatura)
    *   `max17048_driver/` (Batería)
    *   `ad8232_driver/` & `ad8232_c3_driver/` (Electrocardiografía)
*   **Motor Gráfico (LVGL) y UI del Reloj:** Se trabaja en [lib/supaclock_ui/](file:///home/jay-c/Desktop/SupaClock_IEE2463/lib/supaclock_ui), [lib/ui_theme/](file:///home/jay-c/Desktop/SupaClock_IEE2463/lib/ui_theme) y [lib/ui_fonts/](file:///home/jay-c/Desktop/SupaClock_IEE2463/lib/ui_fonts).
*   **TinyML (CNN 1D) e Inferencia:** La lógica de red neuronal se encuentra en [lib/har_cnn1d/](file:///home/jay-c/Desktop/SupaClock_IEE2463/lib/har_cnn1d).
*   **Infraestructura BLE:** Las definiciones del protocolo e hilos de transmisión están en [lib/ble_telemetry/](file:///home/jay-c/Desktop/SupaClock_IEE2463/lib/ble_telemetry).
*   **Pruebas de Componentes:** Código para validar hardware de forma aislada en [src/tests/](file:///home/jay-c/Desktop/SupaClock_IEE2463/src/tests) (por ejemplo, tests de pantalla, BLE, sensores específicos o algoritmos particulares).

#### 2. Tareas de Desarrollo Móvil (Flutter & Dart)
*   **Directorio de trabajo:** [app/](file:///home/jay-c/Desktop/SupaClock_IEE2463/app)
*   **Interfaz de Usuario:** Vistas e interfaces en [app/lib/screens/](file:///home/jay-c/Desktop/SupaClock_IEE2463/app/lib/screens) (dashboard, settings, spot-check, dev-mode).
*   **Persistencia Local y Backend (Firebase):** Configuración de bases de datos y sincronización en [app/lib/services/](file:///home/jay-c/Desktop/SupaClock_IEE2463/app/lib/services) (`firestore_service.dart`, `storage_service.dart`, `local_store.dart` usando Hive).
*   **Procesamiento de Señal (ECG) y BLE:** Algoritmo Pan-Tompkins en `app/lib/services/pan_tompkins.dart` y parseo de tramas de telemetría en `app/lib/services/ble_service.dart`.
*   **Modelos de Datos:** Declarados en [app/lib/models/](file:///home/jay-c/Desktop/SupaClock_IEE2463/app/lib/models) para asegurar concordancia con Firebase.

#### 3. Tareas de Machine Learning y Análisis Científico (Python)
*   **Entrenamiento y Reentrenamiento HAR:** Los scripts para construir el modelo de actividad física se encuentran en [tools/train_har_cnn.py](file:///home/jay-c/Desktop/SupaClock_IEE2463/tools/train_har_cnn.py) y [tools/train_har_cnn_c3.py](file:///home/jay-c/Desktop/SupaClock_IEE2463/tools/train_har_cnn_c3.py).
*   **Recopilación de Datasets:** El historial de datasets recopilados clasificados para entrenar el clasificador se trabaja en [data_ml/](file:///home/jay-c/Desktop/SupaClock_IEE2463/data_ml) (ej. resting, walking, running, stairs).
*   **Reproducción y Debug del Modelo (Replay):** Script [tools/har_replay.py](file:///home/jay-c/Desktop/SupaClock_IEE2463/tools/har_replay.py) corre el modelo `.tflite` sobre logs de acelerometría y simula los filtros de suavizado.
*   **Calibración y Simulación de Umbrales:** `tools/calibrate_steps.py` (conteo de pasos) y `tools/debug_pt.py` (simulador Pan-Tompkins).
*   **Monitoreo del Hardware:** [tools/supaclock_monitor.py](file:///home/jay-c/Desktop/SupaClock_IEE2463/tools/supaclock_monitor.py) es una aplicación de escritorio para graficar ondas IMU/ECG y enviar comandos por puerto serie/BLE.

#### 4. Tareas de Diseño de Hardware (Electrónica / PCB)
*   **Directorio de trabajo:** [hardware/](file:///home/jay-c/Desktop/SupaClock_IEE2463/hardware)
*   Contiene los esquemáticos y placas (`.sch` y `.brd`) para el circuito portador del reloj, el módulo ECG AD8232 y el módulo medidor de batería, además de scripts de soporte de conexión como `build_v4_schematics.py` o `dump_nets.py`.

#### 5. Tareas de Diseño Mecánico (Carcasa y Botones)
*   **Directorio de trabajo:** [mechanical/](file:///home/jay-c/Desktop/SupaClock_IEE2463/mechanical)
*   Lugar donde se diseñan las cubiertas (superior, inferior y botones) en OpenSCAD (`.scad`) y se exportan a STL para manufactura mediante impresión 3D (`mechanical/stl/`, `mechanical/stl_v3/`).

#### 6. Tareas de Documentación del Proyecto
*   **Directorio de trabajo:** [docs/](file:///home/jay-c/Desktop/SupaClock_IEE2463/docs)
*   Incluye los manuales de arquitectura y conceptos (`docs/arquitectura_conceptos.md`), guías para nuevos desarrolladores (`docs/guia_introductoria.md`), protocolos de comunicación (`docs/ble_har_protocol.md`), resúmenes de avances e informes para las evaluaciones académicas del curso (`docs/Entrega1` a `docs/Entrega4`).

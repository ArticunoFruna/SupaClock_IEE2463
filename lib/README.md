# SupaClock ESP32-C3 Base Drivers (`/lib`)

Este directorio contiene las encapsulaciones nativas de los periféricos hardware requeridos por el proyecto **SupaClock**. Dado el uso de ESP-IDF bajo PlatformIO, cada componente vive en su propia carpeta para auto-resolverse como librería aislada mediante el *Library Dependency Finder* (LDF).

## Estado de Desarrollo y Progreso de Librerías

### 🟢 Módulos Completamente Activos (Base)
Estas librerías tienen lógica extensa escrita en C (~80-100+ líneas) y deben considerarse los cimientos inmutables probados del proyecto.
* **`i2c_bus`**: Orquestador principal de hardware. Gestiona el protocolo maestro en los pines por defecto (SCL=9, SDA=8) e implementa Mutex de FreeRTOS para unificar el canal y evitar que las peticiones multi-tarea choquen en el mismo hilo.
* **`st7789_driver`**: Driver básico implementado sobre el bus SPI para comunicarse con el panel LCD TFT. Contiene las plantillas de dibujo rudimentarias o rutinas de inicialización de pantalla.

### 🟡 Módulos Esqueleto (Requieren Completarse)
Estas carpetas poseen la definición mínima estructural (`~20-50` líneas en sus archivos `.c` y `.h`). Contienen los registros Hexadecimales (`0x...`) y las llamadas `_init()` dummy, pero les falta la lógica de manipulación matemática pura (por ejemplo, transaccionar datos I2C o aplicar operaciones `shift` para leer mediciones continuas). 
Serán completadas una a una haciendo uso directo de los Entornos de Pruebas aisladas (Ver `platformio.ini` y `src/tests/*`).
* **`max30205_driver`**: Sensor de Temperatura Corporal Clínica. Contiene estructura en `max30205.h` pero requiere lógica de extracción I2C y conversión a Flotantes °C reales.
* **`bmi160_driver`**: IMU de 6 Ejes (Acelerómetro/Giroscopio) de Bosch. Fundamental para el requerimiento de tesis: Procesamiento digital del Contador de Pasos **por Software**.
* **`max30102_driver`**: Front-end Óptico para lectura de Frecuencia Cardíaca y SpO2 vía I2C.
* **`max17048_driver`**: Fuel Gauge de Litio (Medidor de %). Fundamental para el widget de batería de la pantalla del reloj.
* **`gpio_buttons`**: Rutinas de Interrupciones de Hardware (ISR) debounced para interacciones con la UI del reloj.
* **`ad8232_driver` / `ecg_ad8232`**: Lógica front-end conectada normalmente al ADC contínuo interno del ESP32. Extraerá ondas de ECG nativas.
* **`ble_telemetry`**: Plantilla para levantar el NimBLE (Server GATT) e interactuar con la app de Flutter en Android.

---

> [!NOTE]
> **Metodología de Programación**:
> Jamás insertes código en `src/main.c` mientras desarrollas librerías 🟡 amarillas. Utiliza el selector de Entornos (`[env:test_xxx]`) de PlatformIO para iterar y purificar el código de cada driver individualmente antes de mezclarlo con las demás rutinas en vivo.

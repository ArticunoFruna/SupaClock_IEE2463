# Explicación de la Arquitectura y Códigos del Proyecto SupaClock

Este documento fue estructurado para que puedas copiar y pegar (o adaptar) directamente en tu informe en LaTeX. Cubre a profundidad cómo funciona el código, la arquitectura de hardware/software, qué es el diseño basado en FreeRTOS y cómo fluyen los datos hasta el backend.

## 1. Arquitectura de Firmware y FreeRTOS

El núcleo del reloj es un **ESP32-C3** (con prospección a ESP32-S3). El código del dispositivo (firmware) está escrito en **C nativo**, utilizando el framework oficial de Espressif (ESP-IDF) sobre el sistema operativo en tiempo real **FreeRTOS**.

En lugar de tener un bucle infinito monolítico (un solo `while(1)`), el firmware de SupaClock divide sus lógicas en pequeñas tareas (multitasking). 
* **Prioridad y Tareas**:
  * **Adquisición (Prioridad Alta)**: Interroga los sensores I2C de forma estructurada para no perder muestras cruciales y lee el ADC (ECG).
  * **Procesamiento Edge (Prioridad Media)**: Corre los algoritmos de filtrado local, conteo de pasos y el modelo de clasificación Machine Learning (TinyML).
  * **Interfaz Gráfica (Prioridad Media-Baja)**: Refresca el display y gestiona las acciones de los botones sobre LVGL.
  * **Comunicaciones BLE (Prioridad Baja)**: Empaqueta los resultados en crudo (RAW) o procesados y los envía al celular.

* **Sincronización y Memoria (IPC)**: 
  Para evitar que dos tareas lean un recurso al mismo tiempo, originando caídas (por ejemplo, si la tarea del display y la tarea del TinyML chocan intentando transmitir algo al mismo tiempo), se utilizan **Mutexes (Semáforos de exclusión mutua)**.
  Para pasar datos de una tarea a otra, se usan **Queues (Colas de mensajes)**. Respecto al **uso de memoria RAM/SRAM**, el objetivo de este Wearable es mantener el bajo consumo; por tanto, los datos que son muy densos (como frecuencias del acelerómetro) no se almacenan permanentemente en el reloj, sino que el buffer "TinyML" extrae las características en el instante, descarta el histórico local, y envía solo los resultados procesados al teléfono a 1 Hz, asegurando que el chip no se desborde, ni gaste batería en envíos pesados. La única excepción es el bloque puntual donde se toma el Electrocardiograma: ahí los datos analógicos a 100 Hz se vuelcan inmediatamente vía Bluetooth.

## 2. Abstracciones de Hardware y Conceptos de Bus (Directorio `lib/`)

La arquitectura del código se separa en módulos (drivers). Cada sensor tiene su pequeña propia librería construida sobre los buses principales, manteniendo el directorio `src/` despejado.
 
* **Bus I2C (Inter-Integrated Circuit)**: 
  SupaClock ahorra pines en el chip utilizando un bus compartido I2C. A nivel de firmware, con la librería `i2c_bus`, el ESP32 envía una señal de reloj por un cable (SCL) y los datos por el otro (SDA). Dependiendo de a qué dirección "llame" el ESP32, un sensor en particular responde:
  * MAX30102 (Oxímetro/PPG) a la dirección `0x57`.
  * MAX30205 (Sensor Corporal de Temp) a la `0x48`.
  * BMI160 (Unidad Inercial 6 ejes) a la `0x68`.
  * MAX17048 (Monitor del %, Fuel Gauge) a la `0x36`.

* **Bus SPI y DMA (Direct Memory Access)**:
  Para actualizar la pantalla LCD y dibujar la interfaz gráfica (`lib/display_ui`) a color sin "congelar" el microcontrolador, se usa la interfaz SPI. El código para esto se encuentra en `lib/st7789_driver`. Un concepto vital aquí es el **DMA**. En lugar de que la CPU detenga todas las mediciones cardíacas para mandar píxel por píxel hacia la pantalla, LVGL (librería gráfica) prepara un "dibujo" en la RAM y se lo entrega al hardware del DMA. El DMA "bombea" internamente esos bytes a fondo por el hardware SPI hacia la placa LCD, dándole libertad al microcontrolador de continuar ejecutando su algoritmo de Machine Learning a máxima eficiencia.

* **ADC (Conversor Análogo/Digital)**: 
  Utilizado de forma especializada (`lib/ecg_ad8232`) para leer la señal continua (analógica) proveniente de los terminales secos de las manos para levantar los voltajes reales del corazón que produce la placa analógica pre-amplificadora.

## 3. Rol de la Aplicación Móvil en Flutter

La interfaz del teléfono móvil sirve principalmente como el **Gateway Móvil (Nodo de Enlace)** entre el sensor en la muñeca (borde) y la nube. Está documentada dentro de la carpeta `app/`.
El código en **Dart/Flutter** utiliza librerías reactivas orientadas a stream. 
* Cuando entramos a la app, esta hace un *scanning* en el chip de Bluetooth del usuario y busca al servicio GATT del SupaClock (Vía BLE - Bluetooth de Baja Energía).
* **Flujos a 1 Hz vs 100 Hz**: La aplicación se suscribe a características pasivas. Recibe con una frecuencia muy lenta de 1 Hz los "resúmenes" del reloj: cuánto porcentaje de batería (Fuel Gauge) tiene, ritmo cardíaco superficial y contador de pasos, actualizando los widgets sin congelarse.
* Solo cuando el usuario en la GUI navega para hacerse un ECG, se invoca a un modo transaccional pesado: la app se prepara para recibir una avalancha de puntos RAW cardíacos a más de 100 Hz durante 30 segundos, que los va renderizando en un Canvas del entorno gráfico Flutter para finalmente enpaquetarlos como un JSON masivo.

## 4. Ecosistema de Nube (Backend con Firebase)

En lugar de contar con un servidor propio que requiera mantenimiento activo de base de datos relacionales, el equipo está utilizando una estructura Serverless gestionada en el directorio `firebase/` a través del ecosistema de Google.

* **Cloud Firestore**: Es una base de datos de documentos donde cada usuario asocia su dispositivo, y se agregan "Documentos" anidados (Colecciones) con los históricos consolidados (por ejemplo, perfil general de salud en el tiempo o promedios diarios).
* **Firebase Authentication**: Se encarga de aislar la data biométrica de los usuarios a través del inicio de sesión dentro del app en Flutter.
* **Procesamiento Asíncrono en la Nube (Cloud Functions)**:
  Los microcontroladores como el ESP32, aunque potentes, derrocharían mucha vida de la batería si debiesen filtrar y hallar desviaciones estándar milimétricas con integrales en la señal cruda del Electrodardiograma.
  El código sube ese JSON masivo directo a Firebase, lo que dispara una *Cloud Function* especializada (alojada en los servidores de Google y típicamente ejecutada en Python). Esta función aplica virtualmente los algoritmos pesados de biomedicina (como el Filtro Derivativo de Pan-Tompkins para atenuar interferencia electromagnética de red y ubicar con precisión los picos R a R), retorna resultados como la Frecuencia Cardíaca final, los inyecta en la Firestore y finalmente la app del usuario los extrae para su visualización como resultado definitivo.

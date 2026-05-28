# Guión de Apoyo para la Presentación: Machine Learning y Cierre

Este documento contiene un libreto guía y puntos clave a destacar en cada una de las diapositivas del bloque de **Machine Learning**, **Pruebas** y **Próximos Pasos** de la presentación del Avance 3 (`presentacion3.tex`).

---

## Diapositiva: Transición / Portada de Sección

### **Sección: Machine Learning Edge**

* **Idea Central:** Introducir que el foco en Inteligencia Artificial en esta entrega ha madurado gracias al salto de hardware y a la recolección de datos.
* **Guión Hablado:**
  > *"Ahora pasaremos a detallar la implementación de Machine Learning en el borde para la clasificación de actividad física (HAR) y la detección de caídas. Durante la Entrega 2, esto era un hito en etapa de diseño preliminar. En este avance, hemos consolidado el pipeline completo directamente en el hardware final."*
  >

---

## Diapositiva: Evolución del Procesamiento Edge (C3 vs. S3)

* **Idea Central:** Justificar la migración tecnológica basándose en los cuellos de botella de hardware resueltos.
* **Guión Hablado:**
  > *"La primera pregunta fundamental es: ¿por qué migramos a la ESP32-S3 para el Machine Learning? En la ESP32-C3 nos enfrentábamos a restricciones severas. Su arquitectura single-core sin FPU ni instrucciones vectoriales hacía que una sola inferencia de red neuronal tomara entre 50 y 200 milisegundos. Esto provocaba latencia visible en la pantalla (LVGL) e interrumpía la pila Bluetooth NimBLE, causando desconexiones de telemetría. Además, su heap libre tras inicializar el sistema era de apenas 99 kilobytes, restringiendo el modelo a tamaños imprácticos de menos de 30 kilobytes.*
  >
  > *Con la Seeed XIAO ESP32-S3, abrimos una ventana de oportunidades: su procesador es dual-core, cuenta con instrucciones vectoriales optimizadas mediante la librería ESP-NN de Espressif y dispone de 8 megabytes de PSRAM externa. Esto nos permite ejecutar la inferencia de manera asíncrona en el Core 1 en tan solo 5 a 20 milisegundos sin perturbar el Core 0 (donde corren la interfaz gráfica y el Bluetooth), y alocar una Tensor Arena más holgada de 128 kilobytes en la PSRAM."*
  >
* **Detalles Técnicos a Señalar:**
  * Señalar el contraste de latencias (5-20 ms en S3 vs 50-200 ms en C3).
  * Enfatizar la separación de núcleos (Core 0 para BLE/GUI, Core 1 para ML).

---

## Diapositiva: Arquitectura del Modelo de ML (CNN 1D)

* **Idea Central:** Explicar la red convolucional de 4 clases y el significado físico de la ventana temporal.
* **Guión Hablado:**
  > *"La arquitectura del modelo que estamos integrando es una red convolucional unidimensional, o CNN 1D. El bus de datos de entrada es un tensor estructurado con una dimensión de 200 muestras temporales por 6 canales de sensores inerciales. Como operamos el modelo a 50 Hz, una ventana de 200 muestras representa exactamente 4.0 segundos de señal física continua. Esto es de vital importancia, ya que permite al modelo capturar ciclos periódicos completos de caminata y trote, además de capturar las tres fases inerciales características de una caída: la caída libre, el impacto severo y la inmovilidad posterior.*
  >
  > *La red se compone de 3 capas convolucionales con filtros progresivos de 32, 64 y 128 que extraen patrones de movimiento en alta dimensionalidad. Seguido de esto, implementamos Global Average Pooling (GAP) para colapsar la dimensión temporal y reducir drásticamente el número de conexiones redundantes, previniendo el sobreajuste. Finalmente, una capa densa con regularización por Dropout entrega las probabilidades de salida para las 4 clases de actividad del SupaClock: reposo, caminata, trote y caída."*
  >
* **Detalles Técnicos a Señalar:**
  * Apuntar al diagrama TikZ en pantalla para mostrar la reducción de dimensiones de izquierda a derecha.
  * Resaltar el significado físico de los **4.0 segundos** de ventana y por qué el **traslape del 50% (deslizamiento cada 2.0 segundos)** da fluidez visual sin lag de procesamiento.

---

## Diapositiva: Ventajas de la CNN 1D frente a otras arquitecturas

* **Idea Central:** Comparar el modelo propuesto contra alternativas comunes y detallar el bus de datos inercial.
* **Guión Hablado:**
  > *"Es importante justificar por qué seleccionamos una CNN 1D en lugar de otras arquitecturas de Machine Learning:*
  >
  > * *Frente a las redes densas multicapa o MLP, la CNN 1D preserva la vecindad temporal y es extremadamente eficiente en parámetros gracias a la compartición de pesos con kernels deslizantes. Esto nos permite tener un modelo compacto de solo 58 kilobytes cuantizado.*
  > * *Frente a las redes recurrentes como LSTMs o RNNs, la CNN 1D evita el altísimo consumo de RAM que exige retener estados ocultos y permite evaluar la ventana completa en paralelo, aprovechando la aceleración por hardware SIMD de la ESP32-S3.*
  > * *Frente a algoritmos puramente heurísticos de umbral inercial, la red aprende patrones multidimensionales complejos, reduciendo falsos positivos por sacudidas casuales.*
  >
  > *En la esquina inferior derecha podemos ver las dimensiones del bus de entrada al modelo: un buffer circular dinámico de 2.4 kilobytes en la SRAM que, tras normalizarse a float32 de-cuantizado ocupa 4.8 kilobytes, y finalmente se cuantiza a tan solo 1.2 kilobytes en formato INT8 para alimentar eficientemente la primera convolución del intérprete."*
  >
* **Detalles Técnicos a Señalar:**
  * Explicar que la cuantización INT8 reduce el tamaño de entrada de 4.8 KB a 1.2 KB.
  * Resaltar el paralelismo inercial gracias a la librería ESP-NN.

---

## Diapositiva: Pipeline de Datos e Integración en Firmware

* **Idea Central:** Mostrar cómo interactúa el hardware del reloj con el software del modelo de manera integrada.
* **Guión Hablado:**
  > *"Aquí podemos observar el pipeline de datos completo implementado en el firmware. El IMU BMI160 lee continuamente aceleración y giroscopio a una frecuencia física de 100 Hz. Sin embargo, para cumplir con la frecuencia de diseño del modelo y limpiar ruidos transitorios, implementamos un downsampling en caliente: promediamos cada dos muestras consecutivas para alimentar el ring buffer estático de 2.4 kilobytes en SRAM a una tasa de 50 Hz.*
  >
  > *Una vez acumuladas las 100 muestras de salto (cada 2 segundos), los valores int16 se normalizan al rango flotante de -1 a 1 dividiéndolos por 32768.0, y el intérprete de TensorFlow Lite Micro ejecuta la inferencia de forma asíncrona sobre la Tensor Arena de 128 kilobytes alocada en la PSRAM externa. La gran ventaja de este diseño es que la caída está integrada como la cuarta clase directa del modelo, delegando al clasificador la capacidad de reconocer perfiles complejos y eliminando heurísticas rígidas propensas a fallos."*
  >
* **Detalles Técnicos a Señalar:**
  * Señalar el flujo en el diagrama TikZ: IMU (100 Hz) $\to$ Promedio/2 (50 Hz) $\to$ Ring Buffer (SRAM) $\to$ Normalización $\to$ TFLite Micro (PSRAM) $\to$ Clasificación.
  * Resaltar que la Tensor Arena reside en PSRAM mediante `MALLOC_CAP_SPIRAM` para no asfixiar la SRAM interna.

---

## Diapositiva: Evolución Futura del Modelo de ML

* **Idea Central:** Exponer la hoja de ruta y la escalabilidad del sistema inercial de forma realista.
* **Guión Hablado:**
  > *"Como visión de futuro para robustecer el modelo, planteamos dos ejes de desarrollo:*
  >
  > * *Primero, a nivel de fusión sensorial, consideramos viable incorporar la señal del fotodetector del sensor óptico de pulso (PPG MAX30102) como una entrada de apoyo al clasificador. Esto permitirá al modelo distinguir caídas reales de movimientos bruscos mediante el análisis de las variaciones del ritmo cardíaco bajo estrés cinético, disminuyendo drásticamente los falsos positivos.*
  > * *Segundo, el aprendizaje activo y la personalización: pretendemos habilitar calibraciones locales 'en caliente' para ajustar los umbrales de activación según el peso y patrón de marcha individual de cada usuario.*
  >
  > *Por el lado de la recolección, continuaremos capturando datos reales a través de nuestra aplicación Flutter y aplicando técnicas de optimización como el pruning de pesos para reducir aún más la huella del modelo compilado."*
  >

---

## Diapositiva: Plan de validación de la unidad integrada

* **Idea Central:** Demostrar cómo se probará el wearable como un producto final completo y funcional, no solo en protoboard.
* **Guión Hablado:**
  > *"Habiendo cerrado la arquitectura física de la carcasa, la PCB y la lógica de machine learning, nuestro plan de validación se divide en tres frentes:*
  >
  > * *A nivel mecánico, evaluaremos el encaje repetido de la carcasa, la alineación física del display y botones, la comodidad del reloj en muñeca y, sobre todo, el contacto estable y con presión caracterizada del sensor térmico y óptico contra la piel.*
  > * *A nivel eléctrico, mediremos rails de voltaje, consumo de corriente en los modos de bajo consumo y la calidad de transmisión de la antena BLE con la caja cerrada.*
  > * *Y a nivel funcional, validaremos la precisión biométrica del ritmo cardíaco contra un oxímetro de referencia clínica, el trazado ECG a través de los pernos de acero inoxidable y pogo pins, y la tasa de acierto del modelo de Machine Learning en uso real.*
  >
  > *El criterio de avance fundamental es que la unidad integrada debe reproducir los resultados obtenidos previamente en la protoboard, manteniendo una degradación de señal controlada y medible."*
  >

---

## Diapositiva: Próximos pasos

* **Idea Central:** Enumerar las tareas inmediatas para fabricar el prototipo cerrado.
* **Guión Hablado:**
  > *"Nuestros siguientes pasos inmediatos se dividen en dos áreas:*
  >
  > * *Para el prototipo funcional, cerraremos las reglas de diseño (DRC/ERC) de la PCB carrier v1 en KiCad para mandar a fabricar las placas. Realizaremos el bring-up ordenado por subsistema con la ESP32-S3 montada.*
  > * *Para el Machine Learning y el producto final, continuaremos tomando muestras de calibración, evaluaremos migrar en la siguiente versión a un diseño de dos placas tipo sándwich para comprimir el tamaño horizontal (reduciendo el ancho del reloj a costa de un aumento controlado en altura) y migraremos progresivamente los sensores a encapsulados SMD nativos para independizarnos de módulos comerciales.*
  >
  > *Nuestra meta final para la siguiente iteración es tener el prototipo wearable cerrado y autónomo, transmitiendo telemetría continua por BLE y mostrando las métricas y la clasificación local en pantalla."*
  >

---

## Diapositiva: Demostración / Discusión

* **Idea Central:** Abrir la mesa a preguntas, demostrando que el diseño físico ya está preparado en los repositorios.
* **Guión Hablado:**
  > *"Con esto cerramos nuestra presentación del Avance 3, habiendo completado la migración a la ESP32-S3, el diseño de la PCB integradora y el pipeline de Inteligencia Artificial en el borde. Quedamos atentos a sus preguntas, observaciones y sugerencias para esta fase de bring-up físico. Muchas gracias."*
  >

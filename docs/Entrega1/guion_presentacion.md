# Guión de Presentación: Avance 10% SupaClock
**Orador:** Integrante del Grupo 10  
**Tiempo Objetivo Máximo:** 10:00 Minutos (+ 5 de demo)

---

### [00:00] Slide 1: Título 
"Buenas tardes. Somos el Grupo 10 y hoy venimos a presentarles nuestra planificación y 10% de avance en la construcción de **SupaClock**, nuestra plataforma wearable dedicada al monitoreo de la salud."

---

### [00:30] Slide 2: Problemática y Filosofía de Diseño
"SupaClock nace para suplir la necesidad de herramientas de monitoreo ambulatorio médico. Sin embargo, nuestra filosofía de ingeniería es tajante: **SupaClock no es ni pretende ser un smartwatch de gama alta comercial**. 

Tal como respaldamos en nuestra investigación (con estudios como los de Kushlev y Pielot en interacción humano-computadora), los wearables comerciales imponen una tremenda carga cognitiva: la avalancha de notificaciones e hiperconectividad constante eleva enormemente la inatención y fatiga mental. Basándonos en esta premisa clínica, y buscando un impacto real frente al sedentarismo (como demuestran los estudios de Shin sobre trackers de actividad), diseñamos un dispositivo 'Cero Distracciones'. Es una extensión médica pasiva en la muñeca dedicada casi exclusivamente a medir parámetros de vida (SpO2, Pulso, Temperatura, ECG e inercia), así como reportarlos al usuario junto con la hora, sin invadirlo con ruidos externos, redes sociales o vibraciones de consumo masivo."

---

### [01:40] Slide 3: Evaluación Peto vs. Reloj
"Originalmente, para obtener las mejores señales anatómicas del Electrocardiograma evaluamos construir un *Peto Torácico*. Su ventaja eléctrica es que las placas de medición van directas al pecho; pero su falla ergonómica es brutal: los pacientes rechazan usar arneses invasivos de forma rutinaria y los cables largos inducen tremendo ruido.

Por ende, **optamos por el factor de forma de reloj biométrico**. Esto exige una ingeniería de empaquetamiento y administración de batería mucho más ruda, pero el acceso orgánico a los capilares de la muñeca para sensores ópticos nos garantiza que el paciente se sienta cómodo utilizándolo 24/7."

---

### [03:00] Slide 4: Arquitectura General
"Este es el esquema que construimos. El corazón es nuestro ESP32-C3; este micro actúa como un coordinador de todo el sistema. Extrae todas las métricas de los sensores y las envía al smartphone Android por BLE (Bluetooth Low Energy). El celular actúa como un puente, y sin saturar al reloj ejecutando cálculos, sube la información biológica directo a Firebase Firestore para su almacenamiento en la Nube."

---

### [04:00] Slide 5: Demanda de Energía
"Garantizar la autonomía de este monitoreo no es trivial. Utilizamos un BMS TP4056 acompañado por un sistema de protección DW01A para evitar picos de voltaje en la celda LiPo de 3.7V. Paralelo a esto, usamos un Fuel Gauge MAX17048 para conocer el porcentaje de la batería vía I2C, todo regulado por un LDO que asegura 3.3V para el micro."

---

### [05:00] Slide 6: Ruteo Sistémico e Interfaces
"Respecto al tráfico de datos en la placa, tomamos decisiones de empaquetamiento estrictas:
Encadenamos los sensores bajo un **bus I2C**. Esto es crucial: componentes como el sensor óptico MAX30102 (que emite y recibe pulsos de luz en la arteria para leer ritmo cardíaco y oxígeno), el termómetro y el acelerómetro comparten la misma vía de solo dos cables. Esto previene que nuestro ESP32-C3 se quede sin interfaces disponibles.
Paralelamente, para tener gráficas fluidas sin ahogar al microcontrolador, la pantalla LCD ST7789 viaja aislada recibiendo alto ancho de banda en un **bus SPI con DMA**. Es decir, enviamos memoria física directo a la pantalla vía hardware, sin interrumpir al procesador. Las señales críticas del ECG, en tanto, operan puramente de forma análoga al ADC, el cual también usa la DMA."

---

### [06:00] Slide 7: Arquitectura de Software y Gráficos
"Nuestro firmware exige una precisión temporal estricta sobre un kernel FreeRTOS.
Separamos drásticamente la recopilación sensorial del hilo visual. Para dibujar usamos la librería gráfica **LVGL**, que nos permite diseñar tableros médicos muy ligeros; esta librería renderiza ocultamente en memoria RAM y luego gatilla que el bus SPI la empuje a la pantalla. Como I2C, sensores y pantalla corren concurrentemente, controlamos su ecosistema mediante semáforos (Mutex), logrando garantizar que jamás nos perdamos una métrica aunque el LCD se esté actualizando."

---

### [07:00] Slide 8: Machine Learning en el Borde (TinyML)
"Al tener un wearable de bajo consumo energético, necesitamos inteligencia médica predictiva local: detectar una caída, o si el paciente está caminando o en reposo, lo cual logramos muestreando la inercia del acelerómetro en muy alta frecuencia.
El diseño especifica que esta inferencia corre sobre el mismo reloj. Para esto, utilizaremos la plataforma **Edge Impulse** para entrenar e integrar **una Red Neuronal Convolucional (CNN 1D)**. Empezaremos implementando este modelo optimizado en nuestra arquitectura actual (ESP32-C3); sin embargo, el diseño físico que armamos nos permite una escalabilidad de Plug & Play hacia la versión superior ESP32-S3 (dual-core) para obtener mayor capacidad de procesamiento en caso de que la complejidad del modelo lo exija."

---

### [08:00] Slide 9: Integración Nube (Backend)
"A nivel sistémico global, el backend absorbe los verdaderos picos de carga pesada. Nuestra base de datos NoSQL dispara algoritmos automatizados en la nube mediante Cloud Functions. Allí cruzaremos operaciones computacionalmente demandantes como análisis con el Algoritmo de Pan-Tompkins para escanear deficiencias en el electro de fondo, sin drenar los ciclos de batería finita del reloj en la mano."

---

### [09:00] Slide 10 y 11: Avance de 10% y Próximos pasos
"**Al día de hoy:** Hemos superado la integración básica del entorno con el hardware; compilamos exitosamente las librerías base, orquestamos FreeRTOS con los buffers de la pantalla LVGL, y pudimos enlazar los sensores pasivos por comandos en el mundo real.

**Hacia el avance del 25%:** Apuntamos a finalizar la validación de componentes hardware en laboratorio y arrancar de forma firme con el algoritmo de detección de pasos. Además de perfilar el inicio gráfico de las métricas vitales."

---

### [10:00] Demo en Vivo
"Para cerrar este hito, demostraremos nuestros primeros ecosistemas integrados vivos. Validaremos ante ustedes el display ST7789 energizado con aceleración hardware en FreeRTOS; y como prueba en caliente, comunicaremos, extraeremos y visualizaremos el registro digital de nuestra temperatura anatómica dictada en I2C vía el sensor clínico MAX30205. (Pasar control a integrante que hace la demo técnica)."

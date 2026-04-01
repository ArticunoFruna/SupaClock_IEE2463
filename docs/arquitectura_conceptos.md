# Guía Completa: Arquitectura y Conceptos del Proyecto SupaClock

Esta guía está diseñada para llevar a un programador con conocimientos básicos (por ejemplo, alguien que ha utilizado la plataforma Arduino y la función `loop()`) hacia el entendimiento de una arquitectura profesional utilizando un Sistema Operativo en Tiempo Real (RTOS), desarrollo móvil moderno y bases de datos en la nube.

---

## 1. De Arduino a ESP-IDF (FreeRTOS)

En un programa clásico de Arduino, el código funciona de manera **secuencial**: hay un `setup()` que inicializa los pines y un `loop()` infinito que se repite una y otra vez. El problema de esta estrategia (llamada "Super-Loop") es que si utilizas un `delay()` o si la lectura de un sensor tarda mucho, **todo el sistema se congela**. No puedes actualizar la pantalla y medir la frecuencia cardíaca al mismo tiempo.

Para solucionar esto, SupaClock no usa el entorno básico de Arduino, sino que usa **ESP-IDF** (el entorno oficial de Espressif en lenguaje C puro) operando sobre **FreeRTOS** (Free Real-Time Operating System).

### ¿Qué es FreeRTOS y sus Tareas?
FreeRTOS es un mini-sistema operativo para microcontroladores. Nos permite dividir nuestro código en **Tareas (Tasks)**, como si tuviéramos múltiples `loop()` ejecutándose al mismo tiempo (Multitasking o Concurrencia). 
El ESP32-C3 solo tiene un "procesador" (un solo núcleo), pero FreeRTOS cambia tan rápido de una tarea a otra (en milisegundos) que parece que todo ocurre a la vez.

En SupaClock se definen tareas con **prioridades**:
1. **Tarea de Adquisición (Prioridad Alta):** Constantemente lee los sensores. Como medir el pulso o la temperatura es crítico, si esta tarea necesita funcionar, FreeRTOS pausa a las demás de inmediato y le presta el procesador.
2. **Procesamiento Edge (Prioridad Media):** Aplica matemáticas a lo que leyó la tarea anterior (ej. contar pasos, deducir la postura con *TinyML*).
3. **Interfaz Gráfica (Prioridad Media-Baja):** Dibuja los números en la pantalla LCD (LVGL) y lee cuando oprimes un botón.
4. **Comunicaciones BLE (Prioridad Baja):** Se encarga de empaquetar y enviar inalámbricamente los resultados por Bluetooth.

### Protegiendo la Memoria: Queues y Mutexes
Al tener cosas pasando "al mismo tiempo", hay riesgos:
* **El problema:** ¿Qué pasa si la Tarea 1 intenta guardar datos en una variable, y justo en ese milisegundo la Tarea 2 intenta leer esa misma variable? Podría leer basura.
* **Solución 1 (Queues o Colas):** Son buzones de mensajes de FreeRTOS. La Tarea de Adquisición lee 50 valores del acelerómetro y los mete en un buzón de forma segura. La Tarea de Procesamiento los saca del buzón uno a uno para analizarlos.
* **Solución 2 (Mutex):** "Mutex" significa Exclusión Mutua. Funciona como la **llave del baño**. Si tenemos tres sensores conectados al mismo cable físico (I2C) y la Tarea 1 quiere leer un sensor, toma la llave (Mutex). Si durante ese instante la Tarea 3 quiere leer otro sensor del mismo cable, debe cruzar los brazos y esperar a que la Tarea 1 suelte la llave. Así evitamos colisiones catastróficas.

---

## 2. Conceptos de Hardware y Protocolos (Explicado desde cero)

Tu microcontrolador ESP32-C3 tiene un número muy limitado de pines físicos. Si intentas conectar 4 sensores avanzados asignando cables y pines separados para cada dato, te quedarías sin pines casi de inmediato. Por ello, se utilizan "Protocolos de Bus", que permiten compartir cables entre muchos chips.

### Bus I2C (Inter-Integrated Circuit)
Se podría traducir como el "Bus de 2 cables". Es un protocolo donde todos los sensores biométricos de SupaClock comparten **solo 2 cables**:
1. **SCL (Reloj):** El cable donde el ESP32 marca el ritmo o velocidad de la conversación.
2. **SDA (Datos):** El cable por donde viajan los "1s y 0s" (los mensajes).

**¿Cómo funciona si todos comparten el mismo cable?**
Funciona como si el ESP32 fuera el profesor de una sala de clases, y cada sensor es un alumno con un "nombre" numérico único (Llamado **Dirección Hexadecimal**). 
Si el ESP32 grita por el cable SDA: *"¡Alumno 0x57, entrégame tu tarea!"*, solo el Oxímetro (MAX30102) le responde, mientras el Termómetro (MAX30205, dirección `0x48`) ignora el mensaje. 
Esto permite conectar el Oxímetro, el Termómetro, el Giroscopio y el Medidor de Batería, usando solamente 2 pines del ESP32.

### Bus SPI (Serial Peripheral Interface)
I2C es genial para sensores, pero es muy lento para mover gráficos hacia una pantalla a color. Para la pantalla LCD ST7789, usamos SPI.
SPI usa más cables que I2C, pero es una vía rápida dedicada que opera como una autopista de varias pistas:
* **SCK:** El ritmo de reloj (mucho más rápido que I2C).
* **MOSI:** El canal de ida (Master Out, Slave In). El ESP32 le escupe píxeles de colores a la pantalla por aquí.
* **CS (Chip Select):** Un cable extra que actúa como un "switch". Cuando el ESP32 lo pone en LOW, le dice a la pantalla *"Hey, pon atención que lo que va a pasar por el cable MOSI es una foto para ti"*. 

### El DMA (Direct Memory Access)
Imagina que eres un arquitecto construyendo un muro muy largo. Podrías tomar ladrillo por ladrillo y llevarlo caminando (es decir, el procesador del ESP32 lee un píxel de su memoria y lo manda a la pantalla, uno a uno). Si lo haces así, tú (la CPU del ESP32) no puedes hacer nada más; estás esclavizado llevando píxeles.

El **DMA** es como contratar a un ayudante por hardware físico. El código del ESP32 (usando una librería gráfica llamada LVGL) "dibuja" en su memoria interna una imagen completa de la pantalla. Luego, el ESP32 le dice al DMA: *"Hey DMA, toma todos estos 10.000 píxeles y bombéalos hacia la pantalla por la vía SPI lo más rápido que puedas. Yo me voy a seguir midiendo la actividad cardíaca."*
El resultado: la pantalla se refresca de la nada, fluidamente, sin costarle recursos valiosos de cálculo matemático a la CPU.

### ADC (Conversor Análogo a Digital)
El mundo real no son `1` ni `0`; la corriente eléctrica de tu corazón es analógica (como una ola infinita y suave). El chip encargado del ECG (AD8232) extrae tus latidos tocando los tornillos metálicos, y saca un pequeñito voltaje que sube y baja.
El **ADC** del ESP32 es la compuerta que lee esa olita de voltaje y, miles de veces por segundo, la convierte a números enteros (ej. de 0 a 4095) para que el código en C la pueda entender y matemáticas.

---

## 3. El Mundo Móvil: App y Flutter (Explicado desde cero)

### ¿Qué es Flutter y por qué se usa?
Tradicionalmente, si querías crear una App para que se conecte a un de hardware, necesitas saber programar en **Kotlin o Java** (para teléfonos Android) y otro código completamente distinto en **Swift** (para iPhones). Esto dobla el trabajo.
**Flutter** es una herramienta moderna creada por Google utilizando el lenguaje de programación **Dart**. Nos permite diseñar una aplicación escribiendo el código de las pantallas **una sola vez**, y Google se encarga de compilar ese código para que funcione de forma idéntica en Android e iOS.
En Flutter todo es un "Widget". Un botón es un widget que va dentro de una Lista, que es un widget, que va dentro de una Ventana (widget). Modificar pantallas móviles consiste en agrupar visualmente estas cajas de construcción abstractas.

### ¿Cómo se comunican el Reloj y el Celular? (Bluetooth BLE)
Se utiliza un protocolo llamado **Bluetooth Low Energy (BLE)**, el cual es muy diferente al Bluetooth clásico de los audífonos (que chupan muchísima batería al estar siempre activos transmitiendo audio).

El BLE opera mediante la lectura de "pizarras":
* **El Servidor (SupaClock):** El ESP32 funciona como una pizarra dividida en secciones lógicas (llamadas **Servicios**). En la sección 'Batería' escribe un '80%'. En la sección 'Frecuencia Cardíaca' escribe '90 ppm'. Estas cajitas pequeñas donde escribe los números exactos se llaman **Características GATT**.
* **El Cliente (La App del Celular):** El teléfono encuentra el reloj, se conecta, y no pide la información cada 5 minutos, sino que **se subscribe** a la característica de 'Frecuencia Cardíaca'.
* **Notificaciones:** El reloj actualiza el texto en su "pizarra" solo 1 vez por segundo (1 Hz). Cuando el número cambia a '91 ppm', pasivamente la antena le "notifica" (Notify) a la App del celular para que mueva sus gráficos. Es un diseño altamente ahorrativo en batería.

---

## 4. El Backend y la Nube: Firebase (Explicado desde cero)

### ¿Qué es el "Backend" y qué es "Serverless"?
Al construir una app completa, necesitas un puente permanente que guarde en la nube los registros del reloj para que no se pierdan cuando el celular se apague. Históricamente tendrías que arrendar una torre de computadora real en Amazon (un "Servidor"), instalar Linux, configurar una base de datos MySQL, gestionar la seguridad, y rezar para que si se corta su luz, puedas reiniciarlo.

**Firebase** es un ecosistema de Google enfocado en la arquitectura "Serverless" (Sin Servidor propio). En este formato, tú no configuras una computadora virtual, ni sabes en qué parte física del mundo corre tu código, sino que Google te da las herramientas mágicas listas para usar: "Aquí tienes un inicio de sesión que no se cae nunca, aquí almacenas datos como un diccionario gigante, y me pagas solo por los datos consumidos, olvídate del mantenimiento". 

### A. Cloud Firestore (Base de Datos NoSQL)
Existen bases de datos tipo SQL (relacionales), parecidas a una planilla de Excel inmensa donde todo debe encajar en columnas rígidas ('ID', 'Nombre', 'Hobby').
**Firestore** rompe esto siendo **NoSQL**; organiza los datos mediante "Documentos".
Imagina organizar tu escritorio:
* Abres una carpeta que dice **`Usuarios`**. (Esto Firebase le llama una "Colección").
* En la carpeta encuentras miles de hojas de papel con un nombre, como **`tomas_avendaño_123`**. (Esto se llama un "Documento").
* En el documento de Tomás, hay puro texto formateado estilo diccionario (JSON): "Edad: 24, Pasos de Hoy: 4000".
Su utilidad radica en la flexibilidad inmensa, pudiendo organizar datos biológicos por meses y años de una manera casi idéntica a carpetas y archivos locales, sin obligar al programa a ser tan rígido.

### B. Firebase Authentication
¿Cómo garantizamos la seguridad de que el usuario Pedro no vea el registro de ritmo cardíaco del usuario Juan en la nube? 
Firebase Authentication otorga de caja un login automatizado. La app de Flutter le tira a la cajita mágica de Google un correo y una contraseña; Google verifica y se encarga de todo lo difícil, y si la contraseña es correcta, devuelve un "Ticket Secreto (Token)". La App muestra este Token cada vez que le pide un dato a la base de Datos Firestore. Así blindamos toda la información biomédica sin codificar sistemas robustos de criptografía por nosotros mismos.

### C. Cloud Functions (Y el Algoritmo del ECG)
Un ESP32 (el cerebro del reloj) mide unos 2 centímetros; es lo bastante poderoso para dibujar tu pantalla y medir si el acelerómetro saltó, peeeeeero... 
Si quisiéramos analizar matemáticamente durante largo rato la derivativa médica de tu Electrocardiograma (usando el complejo algoritmo de Pan-Tompkins para atenuar tu respiración y hallar la variabilidad de los latidos pura), el microcontrolador llegaría al tope, se congelaría por el esfuerzo matemático flotante o, derechamente, agotaría su micropila de batería muy velozmente.

**La solución es la Nube:**
El reloj captura el Vector del ECG puro en datos brutos (RAW), los envía por la baja energía del Bluetooth a la App de Flutter, y la App los despacha masivamente hacia Google Firebase.
Apenas llegan a la nube de Firebase, ocurre la magia de las **Cloud Functions**: Estas son programas o "Scripts" en la nube (generalmente escritos con código **Python**) que Google dispara automáticamente como si fuera el percutor de una pistola. Una de estas funciones ocultas "despierta", recibe este gigantesco lote de datos cardíacos sucios, y ejecuta todo el filtro complejo usando la potencia infinita de los mega procesadores de Google Data Centers. Genera el resultado médico limpio, lo incrusta de vuelta en la base de datos Firestore de ese usuario, y se "apaga". Finalmente la App en tu celular escucha esto, y muestra tu Electrocardiograma perfecto sin que el reloj haya sufrido estrés alguno.

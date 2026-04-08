# Guía de Introducción al Desarrollo de SupaClock

Si eres nuevo en el proyecto y quieres empezar a programar o trabajar con el hardware, ¡bienvenido! El proyecto SupaClock es muy completo y abarca diversas áreas de la ingeniería, desde soldar componentes hasta ejecutar algoritmos de inteligencia artificial en la nube. 

No te asustes, no necesitas saber todo para aportar. Hemos dividido esta guía en las diferentes **Áreas de Conocimiento**. Elige el área en la que quieres trabajar y sigue la ruta de aprendizaje.

---

## 1. Área de Firmware (Programación del ESP32)
*Si tu rol es programar el "cerebro" del reloj, leer sensores y manejar las tareas diarias del dispositivo, esta es tu área.*

### A. El Entorno: PlatformIO y VS Code
Olvídate del IDE clásico de Arduino (la pantalla blanca con botones turquesa). 
Para proyectos profesionales usamos **Visual Studio Code (VS Code)** con una extensión llamada **PlatformIO**. 
* **Qué aprender:** Cómo instalar PlatformIO, cómo funciona el archivo `platformio.ini` (donde se declaran qué librerías descargará internet automáticamente) y cómo compilar/subir código al ESP32.

### B. El Framework: ESP-IDF y FreeRTOS
En lugar de usar las librerías genéricas de Arduino, usamos **ESP-IDF** (El kit oficial del fabricante del chip). ESP-IDF nos da acceso al 100% de la potencia del microcontrolador.
* **Lenguaje:** **C nativo** (y un poco de C++). Necesitas repasar cómo funcionan los *Suma de Punteros*, paso por valor/referencia y Estructuras (`struct`).
* **FreeRTOS:** A diferencia de Arduino, aquí no hay función `loop()`. Todo el código está dentro de **"Tasks"** (Tareas infinitas). 
* **Qué investigar en YouTube/Google:**
  1. *FreeRTOS xTaskCreate* (Cómo crear tareas que corran en paralelo).
  2. *FreeRTOS Queues* (Cómo pasar información de una tarea a otra).
  3. *FreeRTOS Mutex* (Cómo evitar que dos tareas choquen).

---

## 2. Área de Interfaz Gráfica (UI del Reloj)
*Si tu rol es dibujar las carátulas del reloj, los gráficos de pulso y hacer que los menús sean bonitos y rápidos.*

### A. La Librería Gráfica: LVGL
Las pantallas LCD de los relojes no entienden de rectángulos bonitos; solo entienden de un mar de píxeles individuales (X, Y, Color). Para no volverse loco pintando píxel por píxel matemáticamente, usamos **LVGL (Light and Versatile Graphics Library)**.
* **Qué es:** Una librería en C espectacular que te permite crear "Widgets" (Cajas, Etiquetas de texto, Barras de progreso, Arcos o Botones).
* **Qué aprender:** 
  1. *LVGL Styles (Estilos):* Cómo cambiar fondos, tipografías y colores.
  2. *LVGL Layouts (Flexbox/Grid):* Cómo alinear textos al medio de la pantalla fácilmente.
  3. *LVGL Callbacks:* El código que quieres que se ejecute si detecta que un botón físico del ESP32 se apretó e interactúa de forma directa con la pantalla.

---

## 3. Área de Sensores y Hardware (La Placa y la Electrónica)
*Si tu rol es diseñar el circuito, arreglar fallos eléctricos y soldar (o diseñar la PCB).*

### A. Datasheets (Hojas de Datos)
Tus mejores amigos tienen extensión `.pdf`. En la carpeta `docs` tenemos los de cada sensor (MAX30102, BMI160, etc.).
* **Qué aprender a leer:** Al programador de firmware no le importa cuánta corriente aguanta el chip de soldadura, pero a ti sí. Debes entender los voltajes máximos absolutos (Absolute Maximum Ratings), consumo energético (Current Quiescent) y pines críticos.

### B. Protocolos Físicos de Comunicación (I2C y SPI)
* **I2C:** Se pronuncia "I-cuadrado-C". Aprende a identificar quién es la línea de Datos (SDA) y quién el Reloj (SCL). Entender cómo los sensores comparten las mismas vías usando "Direcciones Hexadecimales" sin quemarse mutuamente.
* **Pull-Up Resistors (Resistencias Pull-Up):** Vital para entender por qué algunos sensores I2C o botones no funcionan si no conectas una resistencia a positivo.
* **Hardware de Potencia:** Comprender cómo una batería de LiPo (Que baja de 4.2V a 3.0V dramáticamente) afecta la electrónica sensible de 3.3V y por qué instalamos un regulador LDO (Low Dropout) y un Chip de Carga constante (BMS TP4056).

---

## 4. Área de Desarrollo Móvil (La App del Celular)
*Si tu rol es hacer el programa que verá el usuario en su teléfono.*

### A. Dart y Flutter
Flutter es la magia de Google para crear una App bellísima para Android y iPhones de una sola sentada.
* **Concepto Clave - Todo es un Widget:** Si quieres texto flotando amarillo en un recuadro verde, colocas un widget "Padding" dentro de un Widget "Container" (Verde), y dentro pones un Widget "Text" (Amarillo).
* **Gestión de Estado (State Management):** Descubrirás que presionar un botón no actualiza la vista inmediatamente si no avisas a la App con código mágico como `setState()` o proveedores modernos como *Provider* o *Riverpod*.
* **Qué investigar en YouTube:** *Curso básico Flutter 2024: De cero (Stateless y Stateful Widgets).*

### B. Bluetooth Low Energy (BLE) en Flutter
El celular debe pescar los latidos voladores emitidos por el reloj de forma activa pero usando poca batería celular.
* **Las reglas del BLE:** Debes estudiar qué es un Servicio GATT, qué es una Característica y qué es un Descriptor.
* **Conexión:** Entender por qué en Dart la lectura Bluetooth es **Asíncrona** (`async` / `await` o `Streams`). Es decir, el celular le pide el ritmo cardíaco al reloj, el código se pausa (await), llega el dato y solo entonces el código se reanuda pintando en el gráfico.

---

## 5. Área de Backend y Algoritmos Biológicos (Firebase y Python)
*Si tu rol es hacer de "Médico Analista" en la nube.*

### A. Firebase y Firestore
* **Qué es la Consola de Google Firebase:** Aprenderás a navegar un ecosistema web donde creas la Base de Datos NoSQL **Firestore** (Colecciones llenas de Documentos tipo JSON: "Hojas" con datos guardados). 
* Entender cómo la App de Flutter se puede suscribir fácilmente (en sólo unas 5 o 6 líneas mágicas) apuntando una bala desde la App del móvil directamente hacia este repositorio masivo donde está todo el historial del paciente, en tiempo casi real; si alguien edita el dato en el Servidor Google Firestore con click derecho, la pantalla del Celular lo baja instantáneamente.

### B. Cloud Functions y Algoritmos (Python)
Aquí programarás la **inteligencia pesada**.
* **Scripts Serveless:** Aprenderás a decirle a Firebase "Vigila la base de datos, y si ves que la app subió un electrocardiograma crudo, dispara mi código automáticamente".
* **Python vs Señales (Numpy, SciPy):** Entender librerías pesadas de cálculo vectorial matemático (Imposibles para un pequeño ESP32). Aquí programarás funciones médicas estandarizadas (Por ejemplo, el algoritmo Pan-Tompkins para detectar ondas R y sacar promedios médicos reales, filtrar ruido de 50Hz usando los típicos "Pasabanda", etc.) y devolverlos a la variable correcta.

---

### 🔥 ¿Por dónde y cómo empiezo hoy mismo (Paso Cero)?
1. **Instala las herramientas según tu rol**: VS Code + PlatformIO (Si vas a Firmware) o Android Studio + Flutter SDK (Si vas a la App).
2. **Clona ("Descarga") el código**: Aprende comandos básicos de **Git** (`git pull`, `git add`, `git commit -m "Mi avance"`, `git push`) para bajar el repositorio en el que el equipo trabaja y subir tus reparaciones a Github para que todos las vean.
3. Habla con el compañero que es "Lead" en ese sector y dile: *Abre el archivo C (o Dart) de tu zona y explícame cómo creaste el primero de tus 10 métodos.* 

¡Mucha suerte trabajando en SupaClock!

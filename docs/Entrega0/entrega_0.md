                PONTIFICIA UNIVERSIDAD CATÓLICA DE CHILE
                ESCUELA DE INGENIERÍA                                                                  Grupo 10
                Departamento de Ingenierı́a Eléctrica
                IEE2913 — Diseño Eléctrico (Capstone)




      Informe de Planificación: Arquitectura del Hardware/Software

Proyecto: SupaClock: Wearable Biométrico Modular Integrantes: Tomás
Avendaño, Benjamı́n Sepúlveda, Pablo Uribe Fecha: 25 de marzo de 2026

Índice

1.  Contexto y Objetivos 4

2.  Posibles soluciones ante la problemática 4 2.1. Peto deportivo
    inteligente . . . . . . . . . . . . . . . . . . . . . . . . . . . .
    . . . . . . . . 4 2.2. Dispositivo wearable tipo smartband . . . . .
    . . . . . . . . . . . . . . . . . . . . . . . . . 5

3.  Propuesta de diseño 5

4.  Análisis de requerimientos 6 4.1. Dispositivo compacto y cómodo . .
    . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 6 4.2.
    Energización y autonomı́a . . . . . . . . . . . . . . . . . . . . . .
    . . . . . . . . . . . . . . 7 4.3. Mediciones requeridas . . . . . .
    . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 7
    4.4. Procesamiento algorı́tmico y Machine Learning . . . . . . . . .
    . . . . . . . . . . . . . . . 7 4.5. Algoritmos de Procesamiento de
    Señales (ECG) . . . . . . . . . . . . . . . . . . . . . . . . 8 4.6.
    Interfaz gráfica y navegación . . . . . . . . . . . . . . . . . . .
    . . . . . . . . . . . . . . . 8

5.  Elección de componentes y justificación 9

6.  Diagrama de bloques 17 6.1. Diagrama de bloques de alto nivel . . .
    . . . . . . . . . . . . . . . . . . . . . . . . . . . . 17 6.2.
    Diagrama de potencia . . . . . . . . . . . . . . . . . . . . . . . .
    . . . . . . . . . . . . . . 19 6.3. Diagrama de señales y buses de
    comunicación . . . . . . . . . . . . . . . . . . . . . . . . . 20
    6.3.1. Mapeo de pines y plan de migración (C3 a S3) . . . . . . . .
    . . . . . . . . . . . . 20 6.4. Arquitectura de Firmware (FreeRTOS)
    . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 21 6.4.1.
    Frecuencias de Muestreo y Optimización de Memoria . . . . . . . . .
    . . . . . . . . 22 6.5. Arquitectura de Ecosistema (App y Servidor)
    . . . . . . . . . . . . . . . . . . . . . . . . . 22 6.5.1.
    Algoritmos de Procesamiento de Señales (ECG en Servidor) . . . . . .
    . . . . . . . 23

7.  Bosquejo de la solución 24

8.  Metodologı́a de trabajo 24 8.1. Áreas y roles . . . . . . . . . . . .
    . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 25

IEE2913 Diseño Eléctrico 1  8.1.1. Área 1: Hardware . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . . . . . . . . 25 8.1.2. Área 2:
Software y Procesamiento . . . . . . . . . . . . . . . . . . . . . . . .
. . . 25 8.1.3. Área 3: Interfaz y Sistema . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . 25 8.2. Trabajo colaborativo . . . . .
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26
8.3. Asignación de tareas . . . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . 26

9.  Objetivos a cumplir 27 9.1. Hitos . . . . . . . . . . . . . . . . .
    . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 27
    9.1.1. Hito 5 %: Definición y planificación inicial . . . . . . . .
    . . . . . . . . . . . . . . . 27 9.1.2. Hito 25 %: Validación de
    subsistemas iniciales . . . . . . . . . . . . . . . . . . . . . 27
    9.1.3. Hito 60 %: Integración funcional del sistema . . . . . . . .
    . . . . . . . . . . . . . . 28 9.1.4. Hito 90 %: Validación avanzada
    y optimización . . . . . . . . . . . . . . . . . . . . 28 9.1.5.
    Hito 100 %: Entrega final . . . . . . . . . . . . . . . . . . . . .
    . . . . . . . . . . . 28

10.Presupuesto estimado 29

11.Referencias 30

IEE2913 Diseño Eléctrico 2 Índice de figuras

1.   Peto deportivo de referencia . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .        4

2.   Smartband comercial de referencia . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .           5

3.   Diseño preliminar de las interfaces gráficas del dispositivo. . . . . . . . . . . . . . . . . . .       9

4.   Diagrama de bloques de alto nivel del sistema SupaClock, incluyendo el dispositivo, la
         aplicación móvil y el backend en la nube. . . . . . . . . . . . . . . . . . . . . . . . . . . .        18

5.   Diagrama de flujo de potencia y gestión de baterı́a del sistema SupaClock. . . . . . . . . .            19

6.   Diagrama de arquitectura de señales, buses lógicos y direcciones I2C. . . . . . . . . . . . .          20

7.   Diagrama de bloques del firmware y gestión de tareas en FreeRTOS. . . . . . . . . . . . .               22

8.   Arquitectura de ecosistema: flujo de datos desde el dispositivo Edge hacia la aplicación
         móvil y el backend en la nube. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .        23

9.   Bosquejo preliminar del prototipo SupaClock con vistas cenital, frontal, lateral y posterior.            24

10. Carta Gantt del proyecto SupaClock con la planificación temporal de actividades por hito.               29

Índice de cuadros

1.   Comparación de Opciones de Microcontroladores (MCU) . . . . . . . . . . . . . . . . . . .               10

2.   Comparación de Sensores Inerciales (IMU) para Detección de Actividad . . . . . . . . . .               11

3.   Comparación de Sensores Ópticos (PPG) para HR y SpO2 . . . . . . . . . . . . . . . . . .               12

4.   Comparación de Sensores de Temperatura para Aplicaciones Médicas/Wearables . . . . .                   13

5.   Comparación de Circuitos Integrados para Carga y Protección (BMS) . . . . . . . . . . .                14

6.   Comparación de Monitores de Baterı́a (Fuel Gauges) . . . . . . . . . . . . . . . . . . . . .            15

7.   Comparación de Reguladores de Voltaje LDO (3.3V) para Fase 2 . . . . . . . . . . . . . .                16

8.   Comparación de Tecnologı́as de Pantalla para Interfaz de Usuario . . . . . . . . . . . . . .            17

9.   Mapeo definitivo de señales para placas SuperMini. . . . . . . . . . . . . . . . . . . . . . .          21

10. Bill of Materials (BOM) con precios reales de adquisición (IVA incl.) para componentes
        crı́ticos. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .   30

IEE2913 Diseño Eléctrico 3 1. Contexto y Objetivos

Según la Encuesta Nacional de Actividad Fı́sica \[1\], apenas un 49.7 %
de los hombres y un 40.3 % de las mujeres mayores de 18 años en el paı́s
mantienen un nivel de actividad fı́sica adecuado. Esta cifra resulta
alarmante al considerar que el 42 % de la población adulta presenta
sobrepeso \[2\], un factor de riesgo determinante para el desarrollo de
patologı́as crónicas a largo plazo. Frente a este escenario, surge la
necesidad de fomentar hábitos de vida más saludables. Con el fin de
contribuir a esta problemática, el presente proyecto propone el diseño y
desarrollo de un dispositivo wearable de bajo costo que permita a los
usuarios monitorizar sus variables biométricas y registrar su evolución
fı́sica de manera accesible.

2.  Posibles soluciones ante la problemática

Para cumplir con los requerimientos solicitados por el cliente,
planteamos dos posibles soluciones. Cada una de ellas con ventajas y
desventajas en distintos ámbitos.

2.1. Peto deportivo inteligente

Con el fin de proponer una solución que sea poco visible y sea más fácil
de utilizar al momento de realizar deporte, planteamos la posibilidad de
diseñar un peto deportivo que integrara los distintos sensores y tuviera
una pantalla en el pecho. La Fig.1 ilustra un peto deportivo comercial
que se utilizó como referencia de este concepto: una prenda ceñida al
torso que podrı́a alojar sensores distribuidos sobre el pecho,
aprovechando la cercanı́a al corazón y la estabilidad del contacto con la
piel.

Figura 1: Peto deportivo de referencia. Imagen obtenida de Pro:Direct
Sport. Disponible en: https:
//images.prodirectsport.com/ProductImages/Main/V3_1_Main_0289806.jpg

Las principales ventajas de este diseño eran principalmente la facilidad
al momento de integrar los sensores, debido a que los circuitos no
tendrı́an que ser tan pequeños. Además, algunas mediciones serı́an más
sencillas de realizar, como las del sensor de ECG. Sin embargo, existı́an
complicaciones técnicas que nos limitaron a seguir pensando esta
opción. La primera de ellas es que quizás serı́a muy complejo adaptar un
circuito a un material más flexible y cómodo para el usuario,
considerando que si se utilizan cables la posibilidad de aumentar el
ruido entre los sensores y el microcontrolador eran muy altas. También,
investigando en algunos de los posibles sensores que podı́amos utilizar,
algunos fueron diseñados para realizar mediciones en la zona de la
muñeca. Por último, analizando desde un punto de vista higiénico,
notamos que serı́a dificil permitir el lavado de la prenda sin generar
daños a la circuiterı́a.

IEE2913 Diseño Eléctrico 4 2.2. Dispositivo wearable tipo smartband

Una de los requisitos de diseño implicaba la necesidad de utilizar una
pantalla que mostrara los datos sensados, por lo que consideramos que
una decisión de diseño fundamental para nuestro producto es que el
usuario tuviera fácil acceso y visualización de los datos que el
dispositivo mide. Es por ello que analizamos la opción de diseñar un
dispositivo que se utilizara en la muñeca y pueda ser fácilmente
manipulado por el usuario. La Fig.2 presenta una smartband comercial que
sirvió como referencia para esta alternativa, mostrando el factor de
forma compacto, la pantalla integrada y la correa ajustable que
caracterizan a este tipo de dispositivos.

Figura 2: Smartband comercial de referencia (Prixton AT410). Disponible
en: https://prixton.com/ product/smartband-at410/

El diseño del producto como smartband tiene como ventaja que, al existir
una gran variedad de dispositivos de la misma ı́ndole en el mercado,
existe mucha documentación relacionada al diseño de estos dispositivos,
incluyendo sensores adaptados para realizar mediciones en la zona de la
muñeca dado que el grosor de la piel en esta zona es menor y permite un
mejor monitoreo de las señales corporales como la saturación de oxı́geno
en sangre. Por otra parte, este dispositivo serı́a menos invasivo y más
adaptable a distintos perfiles de usuarios, considerando que nuestro
público objetivo puede variar significativamente su contextura fı́sica.
Más allá de las ventajas mencionadas anteriormente, existen distintos
desafı́os que deben ser tomados en cuenta para diesñar este tipo de
dispositivos. La primera consideración es que el producto debe ser
pequeño para no generar incomodidades al usuario, esto es una limitación
a nivel de diseño y de ingenierı́a, pues se debe considerar que la
baterı́a que tendrá el dispositivo será pequeña y no podemos considerar
componentes que consuman mucha corriente para maximizar la duración de
la baterı́a, considerando que debemos incluir un modelo de Machine
Learning dentro o fuera del dispositivo, lo cual es una tarea exi- gente
que puede demandar mucha memoria del microcontrolador si se realiza
interna o mucho consumo energético si se envı́an muchos paquetes de forma
inalámbrica a un servidor para su posterior análisis. Siguiendo la lı́nea
de la comodidad, se vuelve fundamental la necesidad de miniaturizar el
dispositivo, principalmente los componentes utilizados para el
funcionamiento óptimo de los sensores, utilizando ideal- mente
componentes SMD, lo cual puede ser difı́cil considerando que el
laboratorio de trabajo no tiene instrumentos ni herramientas óptimos
para realizar PCB que utilicen estos componentes.

3.  Propuesta de diseño

En base al análisis de las opciones anteriores, se optó por el
desarrollo de un wearable tipo smartwatch debido a la amplia
disponibilidad de componentes adaptados para biometrı́a periférica y la
facilidad de

IEE2913 Diseño Eléctrico 5 interacción directa con el usuario, ası́ como
no tener que lidiar con los largos recorridos de las señales de estar
situadas muy lejos entre sı́ fı́sicamente. El producto a desarrollar es
SupaClock, un wearable enfocado en el monitoreo biométrico y de
actividad fı́sica. El entregable principal del proyecto consistirá en un
dispositivo compacto, cómodo y completa- mente funcional. La primera
iteración contendrá todos los subsistemas y sensores sobre una PCB
básica. Luego de verificar el funcionamiento general, se migrará a una
PCB personalizada con los circuitos de los sensores, alimentación,
microcontrolador y periféricos, ası́ como una carcasa hecha a medida. El
sistema será orquestado inicialmente por el microcontrolador ESP32 C3
\[3\], dependiendo de su desempeño en eta- pas iniciales de prueba, se
considera migrar hacia el ESP32 S3 \[4\], que tiene varias mejoras
significativas, a costa de un mayor consumo energético. Para gestionar
el lado de software, se dispondrá del sistema operativo FreeRTOS \[5\].
Como objetivo extendido (bonus), una vez validada la arquitectura
principal, se diseñará una versión "Pro". Esta consistirá en una placa
PCB miniaturizada de alta densidad, acercándose a un factor de forma de
smartwatch comercial. De manera complementaria, el ecosistema se
expandirá con una aplicación Android que recibirá los datos vı́a
Bluetooth Low Energy (BLE) y los almacenará de forma persistente en una
base de datos en la nube (como Firebase), permitiendo un análisis
histórico y avanzado de las métricas del usuario. Para satisfacer los
requerimientos técnicos del proyecto, la propuesta de diseño integra
múltiples subsiste- mas. En primer lugar, la adquisición de señales
biológicas se realiza mediante el sensor MAX30102 \[6\] para
fotopletismografı́a (SpO2/HR) y el MAX30205 \[7\] para la temperatura
corporal, exigiéndose un diseño mecánico que garantice un contacto
dérmico directo, constante y cómodo en la muñeca. Complementaria- mente,
se implementa un sistema de electrocardiografı́a monocanal basado en el
circuito AD8232 \[8\], el cual utiliza un arreglo de tres electrodos de
contacto seco (pernos M3) distribuidos en el dispositivo; esto permite
medir la Derivación I del corazón mediante una interacción sencilla,
requiriendo únicamente que el usuario haga contacto con la mano opuesta
sobre un perno situado en la carcasa. En cuanto al monitoreo de la
actividad, se integra un acelerómetro y giroscopio (BMI160) \[9\] para
la captura de telemetrı́a inercial en 6 ejes, datos que alimentarán un
modelo de Machine Learning capaz de distinguir estados fı́sicos como
caminar, correr, reposo, o detectar situaciones de emergencia/caı́da.
Finalmente, la retroalimentación al usuario se resuelve mediante un
despliegue gráfico local, incorporando una pantalla LCD a color \[10\]
para renderizar una interfaz fluida, atractiva y prolija, capaz de
mostrar métricas numéricas y formas de onda de manera completamente
autónoma y sin depender del teléfono móvil.

4.  Análisis de requerimientos

4.1. Dispositivo compacto y cómodo

Al escoger un wearable, debemos diseñar un dispositivo que no
imposibilite el uso de la muñeca, por lo que no puede superar los 80 mm
de largo ni 50 mm de ancho. A su vez, la altura del dispositivo puede
generar incomodidad, por lo que debemos realizar un diseño que no supere
los 30 mm de altura. Por otra parte, el peso es un factor importante,
por lo que nuestro dispositivo no puede pesar más de 150 g. Dada la
naturaleza de algunos sensores, es necesario que algunos de estos deban
tener contacto directo con la piel del usuario, por lo que es
fundamental que estos se encuentren correctamente protegidos y aislados.
Considerando los implementos del laboratorio, realizaremos una carcasa
impresa en 3D que albergue todos los componentes en su interior, y que
permita una interacción segura entre los sensores y el usuario.

IEE2913 Diseño Eléctrico 6 4.2. Energización y autonomı́a

El dispositivo no puede estar energizado a una fuente de poder para su
uso cotidiano, por lo que es fundamental el uso de baterı́as para
alimentar al mismo. Para garantizar un uso óptimo y seguro de la misma,
se debe incluir un BMS en el diseño. A su vez, es necesario incluir un
circuito battery fuel gauge, el integrado MAX17043 se encarga de
informar mediante I2C el nivel de baterı́a al usuario. Para ser un
dispositivo a la altura de la competencia, es necesario que la baterı́a
tenga una duración mı́nima de 12 horas, por lo que es necesario optimizar
los consumos de energı́a del microcontrolador y los sensores del
dispositivo.

4.3. Mediciones requeridas

Para cumplir ı́ntegramente con las especificaciones del proyecto, el
hardware del dispositivo debe ser capaz de adquirir, acondicionar y
digitalizar cinco variables biométricas y cinemáticas fundamentales,
garantizando la entrega de señales limpias para su posterior
procesamiento: Temperatura corporal superficial: Capturada de manera
periódica mediante el sensor de grado clı́nico MAX30205, el cual requiere
un diseño mecánico que asegure contacto térmico directo con la piel del
usuario. Frecuencia cardı́aca y Saturación de oxı́geno (SpO2): Ambas
métricas serán estimadas me- diante fotopletismografı́a (PPG) de
reflectancia, utilizando el módulo óptico MAX30102 ubicado en la base
del dispositivo. Aceleración: La captura del movimiento lineal espacial
(necesaria tanto para el podómetro como para el modelo de clasificación)
se realizará mediante la Unidad de Medición Inercial (IMU) BMI160.
Electrocardiograma (ECG): Se requiere la adquisición de la actividad
eléctrica del corazón. Esta función estará destinada de manera exclusiva
a mediciones puntuales en estado de reposo. Para ello, se utilizará el
módulo de instrumentación analógica AD8232 acoplado a un arreglo de tres
electrodos de contacto seco (pernos M3), requiriendo que el usuario
cierre el circuito bioeléctrico mediante contacto bimanual. Estos datos
serán procesados inicialmente en el microcontrolador, para luego
enviarse vı́a bluetooth al teléfono, el cual subirá los datos al servidor
que guardará el perfil del usuario, junto con hacer un análisis más
profundo de las métricas diarias.

4.4. Procesamiento algorı́tmico y Machine Learning

El dispositivo no solo actúa como un recolector de datos, sino que debe
procesarlos localmente, cumpliendo con los siguientes requerimientos
algorı́tmicos: Contador de pasos algorı́tmico: Dadas las restricciones del
proyecto de no utilizar módulos comerciales pre-programados, se
desarrollará un algoritmo propietario qen C++. Este operará cal- culando
la magnitud del vector tridimensional de aceleración (\|a\| = a2x +
a2y + a2z ). A esta señal cruda se le aplicará un filtro pasabajos (como
una media móvil) para atenuar vibraciones de alta frecuencia, seguido de
un algoritmo de detección de picos con un umbral (threshold ) adaptativo
para contabilizar el paso. Este contador contará con la funcionalidad de
ser reseteado a cero a voluntad del usuario navegando por los menús de
la interfaz gráfica local del smartwatch. Clasificación de actividades
(4 estados): Se requiere implementar un modelo de clasificación capaz de
predecir cuatro estados fı́sicos y situaciones de riesgo: reposo,
caminar, correr y detección de caı́da/emergencia. Este último estado es
crı́tico para otorgar un valor añadido a la seguridad del usuario.

IEE2913 Diseño Eléctrico 7  Justificación de la arquitectura Edge AI: Se
ha decidido implementar el modelo de Inteligencia Artificial
directamente en el microcontrolador (Edge Computing/TinyML) en lugar de
procesarlo en la nube. Esta decisión se justifica por tres factores
clave: minimiza la latencia (aspecto crı́tico para reportar una caı́da en
tiempo real), reduce drásticamente el consumo energético al evitar la
transmisión constante de telemetrı́a cruda por la antena Bluetooth, y
garantiza la privacidad de los movimientos del usuario. Para el flujo de
trabajo, se evaluará el uso de la plataforma Edge Impulse \[11\] y
TensorFlow Lite for Microcontrollers \[12\], herramientas que permiten
entrenar la red neuronal y compilarla en una librerı́a en C++ altamente
optimizada para las instrucciones vectoriales del ESP32-S3 o código
simple cuantizado para ESP32-C3.

4.5. Algoritmos de Procesamiento de Señales (ECG)

Para la estimación precisa de la frecuencia cardı́aca a partir de la
señal analógica del módulo AD8232, se implementará el algoritmo estándar
de Pan-Tompkins \[13\]. Para no sobrecargar el microcontrolador y
conservar baterı́a, este procesamiento se ejecutará de manera ası́ncrona
en la nube una vez recibida la trama de datos: 1. Filtrado Digital
Pasabanda: Se aplicará un filtro digital para atenuar el ruido de alta
frecuencia (interferencia de la red eléctrica) y eliminar la deriva de
la lı́nea base causada por la respiración del usuario. 2. Procesamiento
Morfológico: La señal filtrada pasará por un filtro derivativo y una
función de cuadratura para resaltar exclusivamente la pendiente aguda
del complejo QRS del corazón. 3. Detección de Picos R: Se utilizará una
integración de ventana móvil con umbrales adaptativos (adaptive
thresholding) para identificar con precisión el tiempo exacto de cada
latido. 4. Cálculo de Métricas: A partir de la distancia temporal entre
picos consecutivos (intervalos R-R), el sistema calculará la Frecuencia
Cardı́aca promedio (BPM) y devolverá el valor a la aplicación móvil para
su visualización.

4.6. Interfaz gráfica y navegación

Para asegurar una interacción intuitiva y cumplir con los requerimientos
de visualización local sin de- pender exclusivamente del teléfono móvil,
el dispositivo integrará una pantalla LCD rectangular a color de 1.69
pulgadas. El sistema de navegación fı́sico constará de tres botones
pulsadores (Arriba, Abajo y Seleccionar/Acción) ubicados en el costado
derecho de la carcasa, garantizando una operación intuitiva y fácil,
incluso durante la actividad fı́sica. La interfaz gráfica de usuario
(GUI) se ha diseñado maximizando el contraste mediante un fondo negro e
ı́conos de colores vibrantes para una rápida lectura. El sistema se
estructurará lógicamente en cuatro vistas principales, las cuales se
pueden apreciar en la Fig.3:

IEE2913 Diseño Eléctrico 8  Figura 3: Diseño preliminar de las
interfaces gráficas del dispositivo.

     1. Pantalla Principal (Dashboard): Actuará como la carátula central (watchface). Concentra la
        información de uso continuo exigida por los requerimientos: la hora actual, el contador de pasos, el
        nivel porcentual de baterı́a, la frecuencia cardı́aca y la actividad fı́sica predicha por el modelo de IA
        en ese instante.
     2. Biometrı́a General: Un panel dedicado a mostrar las lecturas de los sensores periféricos, des-
        plegando la frecuencia cardı́aca, el porcentaje de saturación de oxı́geno (SpO2) y la temperatura
        corporal superficial, además de indicar el estado operativo de los sensores.
     3. Modo ECG: Interfaz interactiva orientada exclusivamente a la toma del electrocardiograma en
        estado de reposo. Guiará al usuario mediante instrucciones visuales para que cierre el circuito bima-
        nual tocando los electrodos. Adicionalmente, mostrará una cuenta regresiva del progreso y dibujará
        (se está evaluando la posibilidad) la forma de onda cardı́aca en tiempo rea.
     4. Menú del Sistema: Interfaz de configuración general que albergará opciones avanzadas. Aquı́ se
        incluirá la función de reiniciar a cero el contador de pasos, activar el modo de emparejamiento Blue-
        tooth (BLE) para la sincronización de datos (Data Logger ) con la aplicación móvil/PC y gestionar
        el apagado del equipo.

5.   Elección de componentes y justificación

El componente más importante de nuestro wearable es el microcontrolador,
ya que actúa como el cerebro del sistema: gobierna todos los
periféricos, ejecuta los algoritmos de procesamiento de señales, corre
el modelo de Machine Learning y gestiona las comunicaciones
inalámbricas. Una elección inadecuada en esta etapa comprometerı́a la
autonomı́a energética, la capacidad de procesamiento y la viabilidad de
futuras expansiones del producto. A continuación se presenta una tabla
comparativa de las distintas opciones analizadas como MCU del wearable.
Es importante aclarar que los tamaños utilizados para la columna de
dimensiones están referidos

IEE2913 Diseño Eléctrico 9 a módulos que tienen integrados los
microcontroladores mencionados.

MCU Arch y Freq Math Pro- Connect Power Size/ In- PinOut cess (DS-
Consum- tegration GPIO P/ML) ption ESP32-C3 RISC-V 32-bit Por softwa-
WiFi 4 y Medio. 22.5 mm x 13 SUPER MI- de un núcleo re. No posee
Bluetooth 5 Requiere 18 mm NI (Seleccio- (160 MHz) FPU (hardwa- (LE)
diseño cui- nado) \[3\] re), los cálcu- dadoso de los de ML y Deep
Sleep. filtros reque- rirán más ci- clos de reloj. ESP32-S3 Xtensa 32-
Sobresaliente. WiFi 4 y Alto. Con- Muy 27 (Selecciona- bit Dual-Core
Posee ins- Bluetooth 5 sumo peak bueno, pe- do) \[4\] (240 MHz)
trucciones (LE) de 340 mA ro requiere vectoriales transmi- un área de
para acelerar tiendo por PCB lige- redes neurona- Wifi 2.4 ramente les.
GHz, 1 mayor. Mbps y 21 dBm . NRF52840 ARM Cortex- Excelente. Solo Blue-
Ultra Ba- Excelente, 11 \[14\] M4F (64 Posee FPU tooth 5 jo. Perfecto
existen MHz) por hardware, (LE) y para weara- módulos ideal para el
otros. Sin bles de alta SMD dimi- filtrado del WiFi. autonomı́a. nutos.
ECG y ML de bajo consumo. STM32WB55 Dual-Core: Excelente. Bluetooth
Ultra Ba- Muy 22 \[15\] ARM Cortex- Posee FPU 5 (LE) y jo. El pro-
bueno. M4 (64 MHz) por hardware 802.15.4 cesador de Empaque- para
aplica- y soporte nati- (Zig- radio inde- tados muy ción y Cortex- vo
optimizado bee/Th- pendiente pequeños, M0+ (32 para ML read). Sin
permite pero re- MHz) dedi- WiFi. ahorrar quiere un cado a radio. mucha
ba- diseño de terı́a du- antena RF rante las si no se transmisio- compra
en nes. formato módulo.

                  Cuadro 1: Comparación de Opciones de Microcontroladores (MCU)

La decisión de utilizar la familia de microcontroladores de Espressif
(ESP32-C3 y ESP32-S3) se fun- damenta en su facilidad de desarrollo y la
vasta cantidad de documentación y herramientas disponibles que el equipo
ya domina. Esto permite acelerar significativamente la fase de
prototipado. Adi- cionalmente, la compatibilidad en el ecosistema de
Espressif ofrece una tremenda flexibilidad: el proyecto

IEE2913 Diseño Eléctrico 10 iniciará utilizando el ESP32-C3 debido a su
bajo costo y menor consumo energético, con la posibilidad de migrar
fácilmente al ESP32-S3 sin mayores cambios en el código base, en caso de
que el modelo de Machine Learning y los algoritmos de procesamiento
exijan una mayor potencia de cómputo.

        Sensor (Fa-       Interfaz      Resolución        Consumo         Ventajas / Des-
        bricante)                                          (Modo Acti-     ventajas
                                                           vo)
        BMI160            I2C / SPI     16 bits (Acel      ∼900 µA         Ventaja: Excelente
        (Bosch) -                       + Giro)                            eficiencia y buffer
        Seleccionado                                                       FIFO grande (1024
        [9]                                                                bytes). Ideal para
                                                                           Edge ML.
        MPU6050           I2C           16 bits (Acel      ∼3900 µA (3.9   Desventaja: Con-
        (InvenSense)                    + Giro)            mA)             sumo de energı́a ex-
        [16]                                                               cesivamente alto
                                                                           para un wearable.
                                                                           Tecnologı́a antigua
                                                                           (Legacy).
        LSM6DS3           I2C / SPI     16 bits (Acel      ∼1250 µA        Ventaja: Muy bajo
        (STMicro)                       + Giro)                            consumo y conta-
        [17]                                                               dor de pasos por
                                                                           hardware integrado
                                                                           (aunque el proyecto
                                                                           exige software pro-
                                                                           pio).

          Cuadro 2: Comparación de Sensores Inerciales (IMU) para Detección de Actividad

Para la detección de movimiento continuo (podómetro y clasificación de
Machine Learning), la Tabla 2 evidencia que el BMI160 es la opción
idónea al combinar un consumo por debajo de 1 mA y un amplio buffer FIFO
(que desacopla el trabajo de lectura continua del microcontrolador),
descartando alternativas de tecnologı́a más antigua y alto perfil
energético como el MPU6050.

IEE2913 Diseño Eléctrico 11  Sensor (Fa- Interfaz Resolución Consumo
Ventajas / Des- bricante) ADC Promedio ventajas MAX30102 I2C 18 bits
∼600 µA Ventaja: Integra (Maxim) - LEDs y fotodetec- Seleccionado tor.
Alta resolución \[6\] y cristal protector que facilita la lim- pieza en
contacto con la piel. MAX30100 I2C 16 bits ∼1200 µA Desventaja: Menor
(Maxim) \[18\] resolución, mayor consumo y conocido por problemas de
compatibilidad de voltajes en módulos comerciales. MAX86141 SPI 19 bits
Ultra bajo Desventaja: Re- (Maxim) \[19\] (\<100 µA) quiere LEDs ex-
ternos y diseño de hardware sumamen- te complejo. Exce- de el alcance de
un prototipo inicial.

                  Cuadro 3: Comparación de Sensores Ópticos (PPG) para HR y SpO2

Por el lado de la métrica cardı́aca y de oxigenación, en la Tabla 3 se
analiza el abanico de fotopletismógra- fos comerciales. El MAX30102 se
consagra como nuestra elección principal por su excelente relación de
facilidad de uso e integración, ya que compila ópticas de alta
resolución en un encapsulado que incluye vidrio protector ideal para el
contacto epitelial constante en la muñeca, superando las limitaciones de
versiones antiguas como el MAX30100.

IEE2913 Diseño Eléctrico 12  Sensor (Fa- Interfaz Precisión Consumo
Ventajas / Des- bricante) (Rango Activo ventajas Clı́nico) MAX30205 I2C
±0.1◦ C (37◦ C ∼600 µA Ventaja: Precisión (Maxim) - a 39◦ C) de grado
clı́nico es- Seleccionado pecı́fica para piel \[7\] humana. Fácil inte-
gración en bus I2C. MLX90614 I2C / ±0.2◦ C ∼1.5 mA Desventaja: Me-
(Melexis) PWM dición infrarroja sin \[20\] contacto. Es muy voluminoso
(empa- que TO-39) e invia- ble para un diseño ultracompacto. DS18B20
1-Wire ±0.5◦ C ∼1 mA Desventaja: Pre- (Maxim) \[21\] cisión insuficiente
para uso biomédico y el protocolo 1- Wire consume más recursos de
software que el I2C.

      Cuadro 4: Comparación de Sensores de Temperatura para Aplicaciones Médicas/Wearables

La medición de fiebre o estrés térmico requiere alta fidelidad; como
describe la Tabla 4, descartamos medidores de propósito general
(DS18B20) o módulos infrarrojos muy abultados (MLX90614, propio de
termómetros de pistola). En contraste, el MAX30205 proporciona precisión
con certificación clı́nica (±0.1◦ C en rango humano) mediante simple
contacto superficial con la piel.

IEE2913 Diseño Eléctrico 13  IC de Carga Topologı́a Protección Corriente
Ventajas / Des- (Fabricante) Integrada de Carga ventajas TP4056 +
Cargador Li- Requiere Hasta 1A Ventaja: Costo ex- DW01A - neal LDO IC
externo (Ajustable) tremadamente bajo, Seleccionado (DW01A) circuito
comprobado \[22\] y fácil de soldar a mano. Ideal para el prototipo.
BQ24075 Cargador Sı́ (Sobreten- Hasta 1.5A Desventaja: Cos- (Texas Ins-
Lineal con sión y Tempe- toso y empaquetado truments) Power-Path ratura)
QFN difı́cil de en- \[23\] samblar. Ventaja: Permite usar el dis-
positivo mientras se carga sin estresar la baterı́a. MCP73831 Cargador
Li- No (Solo ges- Hasta 500 Desventaja: Aun- (Microchip) neal tión
térmica) mA que es diminuto \[24\] (SOT-23), carece de protección contra
sobredescarga, obli- gando a añadir más circuiterı́a.

            Cuadro 5: Comparación de Circuitos Integrados para Carga y Protección (BMS)

Respecto a la fuente de poder, se seleccionó una baterı́a de polı́mero de
litio (LiPo) de 3.7V. Según el análisis y perfilado de consumo de los
diferentes periféricos operando bajo FreeRTOS, hemos estimado un
requerimiento energético en torno a los 350 mAh para asegurar una
autonomı́a funcional de aproximadamente un dı́a completo, manteniendo al
mismo tiempo un factor de forma delgado y liviano fundamental para un
dispositivo tipo pulsera. Para la gestión térmica y energética de dicha
celda, la Tabla 5 resume el análisis del subsistema de carga (BMS). La
combinación de los integrados genéricos TP4056 y el circuito de
protección DW01A es ideal para el primer prototipo. Este módulo se
adquiere fácilmente soldado en placa base con puerto Type-C integrado
por un valor bajo, superando las barreras de soldadura que ofrecen QFNs
como el BQ24075.

IEE2913 Diseño Eléctrico 14  Componente Método de Hardware Consumo
Ventajas / Des- (Fabricante) Medición Externo Promedio ventajas MAX17043
ModelGauge Ninguno ∼50 µA Ventaja: Estima (Maxim) - (Basado en el estado
de carga Seleccionado voltaje) (SOC) sin resis- \[25\] tencia de sensado
(shunt), ahorrando valioso espacio en la PCB. BQ27441-G1 Impedance
Resistencia ∼93 µA Desventaja: Ma- (Texas Ins- Track (Cou- Shunt externa
yor precisión, pero truments) lomb Coun- requiere ruteo crı́ti- \[26\]
ting) co de la resistencia shunt y configura- ción compleja del perfil
de baterı́a. Lectura Divisor de vol- 2 Resistencias Variable Desventaja:
El directa taje resistivo (Corriente ADC del ESP32 no por ADC de fuga)
es lineal. Desper- (ESP32) dicia baterı́a por el divisor de tensión y es
muy impreciso.

                     Cuadro 6: Comparación de Monitores de Baterı́a (Fuel Gauges)

Representar el State of Charge (nivel de baterı́a en la pantalla de la
muñeca) sin dañar la autonomı́a general requiere un integrado
especializado. Tal como indica la Tabla 6, la decisión decantó por el
MAX17043 ya que utiliza el algoritmo patentado ModelGauge sobre el
voltaje crudo (sin utilizar derivadores shunt externos o puentes
resistivos analógicos), ahorrando considerable consumo estático en la
placa.

IEE2913 Diseño Eléctrico 15  Regulador Corriente Voltaje de Corriente
Ventajas / Des- LDO Máxima Caı́da (Dro- Quiescente ventajas pout) (IQ )
ME6211 / ∼500 mA Ultra bajo ∼55 µA Ventaja: Excelente AP2112K - (∼100 mV
a balance entre ta- Seleccionado 100 mA) maño (SOT-23-5), \[27\]
eficiencia cuando la baterı́a baja de 3.6V y capacidad de co- rriente
para el WiFi. AMS1117- 1A Muy alto ∼5 mA (5000 Desventaja: Pési- 3.3
\[28\] (∼1.1 V) µA) ma elección para baterı́as. Su alto dropout cortarı́a
el ESP32 incluso cuan- do la baterı́a esté al 50 %. Drena energı́a en
reposo. TPS7A02 200 mA Bajo (∼140 Nano-poder Desventaja: Co- (Texas Ins-
mV) (25 nA) rriente máxima in- truments) suficiente para los \[29\]
picos de transmisión WiFi del ESP32-C3 (que superan los 300 mA).

              Cuadro 7: Comparación de Reguladores de Voltaje LDO (3.3V) para Fase 2

Traducir los voltajes inestables de la baterı́a de litio (4.2V a 3.0V) a
los estables 3.3V que demanda el microcontrolador debe hacerse con
máxima eficiencia térmica y con el mı́nimo dropout posible (Voltaje de
caı́da extra). La Tabla 7 deja en evidencia que reguladores comunes (como
el AMS1117) son pésimos para baterı́as móviles; por ello, elegimos el
ME6211, el cual puede entregar ráfagas de alta corriente sin bloquearse
cuando la celda baje de 3.6V.

IEE2913 Diseño Eléctrico 16  Tecnologı́a (Con- Interfaz Color y Re-
Consumo Es- Ventajas / Desventa- trolador) solución timado jas IPS LCD
SPI RGB Medio (∼20-40 Ventaja: Excelente 1.3"(ST7789) - (240x240) mA con
retro- densidad de pı́xeles, Seleccionado \[10\] iluminación) colores
vibrantes pa- ra gráficas (ECG) y la interfaz SPI maneja la tasa de
refresco sin cue- llos de botella. OLED I2C / SPI Monocromo Bajo-Medio
Desventaja: Menor 0.96"(SSD1306) (128x64) (∼10-20 mA) resolución y
tamaño. \[30\] Difı́cil visualizar 4 señales simultáneas y la predicción
de Machine Learning de forma legi- ble. Memory LCD SPI Monocromo Ultra
bajo (∼10 Desventaja: Muy cos- (Sharp) \[31\] (Alto contras- µA) tosa y
sin retroilumina- te) ción (ilegible en la oscu- ridad). Excelente para
autonomı́a extrema, pe- ro no justifica el costo en este alcance.

              Cuadro 8: Comparación de Tecnologı́as de Pantalla para Interfaz de Usuario

Finalmente, dotar al sistema de una cara visual local y dinámica decanta
en la elección del controlador de pantalla ST7789 (Tabla 8). Al
descartar opciones monocromáticas limitadas o extremadamente cos- tosas,
la pantalla IPS permite un pintado en alta resolución que es ideal para
dibujar los vectores del Electrocardiograma (ECG) en vivo, junto a una
paleta de colores vibrantes para un entorno de usuario premium.

6.  Diagrama de bloques

6.1. Diagrama de bloques de alto nivel

Para la implementación de la propuesta, se planteó el diagrama de
bloques de la Fig.4, el cual presenta una vista integral de todos los
subsistemas del proyecto SupaClock y sus interacciones. El diagrama
abarca desde la adquisición de señales biométricas en el dispositivo
hasta el almacenamiento y análisis de datos en la nube, pasando por la
aplicación móvil que actúa como enlace entre ambos dominios. Los colores
de las cajas y las conexiones permiten identificar rápidamente cada
dominio funcional, tal como se detalla en la leyenda incluida en la
parte inferior del diagrama.

IEE2913 Diseño Eléctrico 17  Interfaz Local

                                                                                            Botones (x3)
                                                                                            GPIO + ISR



                                                                                            Pantalla LCD
                                                                                            ST7789 (SPI)

                                            I2C
                                                                                                                                                      Backend (Firebase)
                                Sensores y Adquisición

Interacción Usuario SPI Cloud Firestore MAX30102 MAX30205
(Perfiles/Hist.)

                                                                                                                                                 4G
      Muñeca           (SpO2/HR)                 (Temperatura)                                                    Aplicación Móvil




                                                                                                                                            i/
      (Temp,




                                                                                                                                         i-F
     PPG, IMU)




                                                                                                                                        W
                                                                                            ESP32-C3/S3      BLE    App Android
                                                                                              FreeRTOS                                                           BPM
                          BMI160                     AD8232            ADC                                          (Gateway BLE)
                                                                                               TinyML




                                                                                                                                        W
                        (IMU 6-ejes)                 (ECG)




                                                                                                                                         i-F

Mano opuesta

                                                                                                                                             i /

(ECG bimanual) Procesamiento (MCU) Cloud Functions

                                                                                                                                                 4G
                                                                                                                             Futuro                    (Pan-Tompkins)

                           USB-C                 BMS TP4056                 Baterı́a LiPo
                        (5V Entrada)              + DW01A                     (3.7V)                               Raspberry Pi 3B+
                                                                                                                     (Edge Server)


                                                  LDO 3.3V
                                                  (ME6211)           3.3V



                                                  Fuel Gauge
                                                 (MAX17048)

                                            Subsistema de Energı́a
                                                                                  Interacción usuario
                                                                                  Subsistema de energı́a
                                                                                  Sensores biométricos
                                                                                  Procesamiento (MCU)
                                                                                  Interfaz local
                                                                                  Aplicación móvil
                                                                                  Backend en la nube
                                                                                  Proyección futura
                                                                                  Bus digital (I2C/SPI)
                                                                                  Lı́nea de potencia
                                                                                  Señal analógica (ADC)
                                                                                  Enlace BLE
                                                                                  Proyección futura

Figura 4: Diagrama de bloques de alto nivel del sistema SupaClock,
incluyendo el dispositivo, la aplicación móvil y el backend en la nube.

1.  Bloque de Interacción (Usuario): Representa el vı́nculo fı́sico entre
    el wearable y el individuo. Diferencia la zona de la muñeca (para la
    captura constante de temperatura, inercia y fotopletis- mografı́a y
    ECG) de la extremidad opuesta (necesaria para cerrar el circuito
    bioeléctrico del ECG bimanual). A su vez, ilustra la
    retroalimentación visual y táctil mediante la pantalla y los
    botones.
2.  Bloque de Energı́a: Está compuesto por una celda de polı́mero de litio
    (LiPo), un módulo BMS TP4056 que garantiza una recarga segura por
    USB-C, y un regulador lineal (LDO) que estabiliza el voltaje a una
    pista principal de 3.3V, alimentando a toda la electrónica de forma
    ininterrumpida.
3.  Bloque de Sensores y Adquisición: Constituye la etapa de
    instrumentación periférica. Agrupa al sensor MAX17048 (para
    telemetrı́a de baterı́a), MAX30102 (SpO2/HR), MAX30205 (Temperatura) y
    BMI160 (IMU), los cuales multiplexan su información digital a través
    de un bus I2C unificado. De forma paralela, el módulo AD8232 entrega
    una señal analógica continua para el electrocardiograma.
4.  Bloque de Procesamiento (MCU): El núcleo del dispositivo, operado
    por el microcontrolador ESP32-C3 bajo el sistema en tiempo real
    FreeRTOS. Se encarga de gobernar los periféricos, ejecutar el
    filtrado digital local y correr el modelo de Machine Learning
    (TinyML) para la inferencia y clasificación de actividades del
    usuario.
5.  Bloque de Interfaz Local: Permite la operatividad local sin depender
    del teléfono móvil. Está compuesto por un panel LCD IPS (ST7789)
    manejado mediante un bus de alta velocidad SPI (vı́a acceso directo a
    memoria, DMA) y un set de botones configurados como interrupciones
    por hardware (GPIO).
6.  Bloque de Aplicación Móvil: La aplicación Android actúa como Gateway
    entre el dispositivo

IEE2913 Diseño Eléctrico 18  y la nube. Recibe datos vı́a BLE, los
visualiza en tiempo real y los reenvı́a al backend para su almacenamiento
y análisis. 7. Bloque de Backend (Firebase): Cloud Firestore almacena
perfiles y métricas históricas, mientras que Cloud Functions ejecuta
algoritmos de procesamiento pesado (como Pan-Tompkins para ECG). Como
proyección futura, se contempla una Raspberry Pi 3B+ como servidor
local.

6.2. Diagrama de potencia

El subsistema de energización y distribución de potencia ha sido
diseñado para garantizar la seguridad operativa de la baterı́a y proveer
un voltaje altamente estable a la electrónica sensible. El flujo de
energı́a se estructura en tres etapas principales, tal como se ilustra en
la Fig.5:

           Figura 5: Diagrama de flujo de potencia y gestión de baterı́a del sistema SupaClock.

       Fuentes de Entrada y Gestión (BMS): El sistema posee un funcionamiento autónomo ali-
       mentado por una baterı́a LiPo de 3.7V. Para el proceso de recarga, se utiliza un puerto USB tipo
       C que suministra 5V a los pines de entrada (IN+/IN-) del módulo de gestión de baterı́a (BMS)
       TP4056. Este módulo actúa como intermediario, encargándose de cargar la celda de forma segura
       (pines B+/B-) y aislar el circuito para protegerlo contra sobredescargas y cortocircuitos mediante
       el integrado DW01A.
       Monitoreo Independiente (Fuel Gauge): Para informar el nivel de carga al usuario, se ha
       integrado el sensor MAX17048. Este componente realiza una lectura de voltaje directa desde los
       terminales de la celda LiPo, operando en paralelo al BMS.
       Regulación y Dominio Lógico (3.3V): La salida de energı́a protegida (OUT+/OUT-) prove-
       niente del BMS se direcciona hacia un regulador lineal (LDO). Este componente estabiliza la tensión
       fluctuante de la baterı́a y establece una pista principal constante de 3.3V. Este dominio lógico uni-
       ficado alimenta simultáneamente a todos los circuitos integrados del sistema: el microcontrolador
       central ESP32-C3 (S3 en posteriores versiones), la pantalla IPS ST7789, el acelerómetro BMI160,
       los biosensores MAX30102 y MAX30205, y el acondicionador de señal analógico del ECG (AD8232).

IEE2913 Diseño Eléctrico 19 6.3. Diagrama de señales y buses de
comunicación

La arquitectura de comunicación del SupaClock ha sido diseñada para
minimizar el uso de pines del microcontrolador, agrupar sensores de la
misma naturaleza y facilitar la futura migración del ESP32-C3 al
ESP32-S3 de ser necesario (más que nada por temás de potencia a la hora
de probar el Machine Learning). El flujo de datos se ilustra en la
Fig.6:

              Figura 6: Diagrama de arquitectura de señales, buses lógicos y direcciones I2C.

El sistema divide las señales en cuatro dominios principales: Bus I2C
(Sensores): Actúa como el bus principal de telemetrı́a operando a 400
kHz. Agrupa el sensor de oximetrı́a (MAX30102 - 0x57), el monitor de
baterı́a (MAX17048 - 0x36), el sensor de temperatura (MAX30205 - 0x48) y
la unidad inercial (BMI160 - 0x68). El uso de este bus compartido es
crı́tico para ahorrar puertos de I/O. Bus SPI (Pantalla): Un bus dedicado
de alta velocidad. Está asignado al controlador ST7789 de la pantalla
LCD. Solo requiere señales de salida (MOSI, Reloj, CS, DC, Reset), ya
que no es necesario leer información desde la pantalla, ahorrando el pin
MISO. Canal Analógico (ECG): La salida analógica filtrada proveniente
del módulo AD8232 requiere de un conversor analógo-digital, el cual
funcionará mediante la DMA. Este se conectará a un pin ADC dedicado.
Entradas Digitales (Botones e Interrupciones): Tres lı́neas GPIO
configuradas como entradas con resistencias pull-up internas para los
botones fı́sicos, y un pin adicional conectado al pin INT1 del BMI160,
permitiendo despertar al microcontrolador mediante este mismo de ser
necesario a futuro.

6.3.1. Mapeo de pines y plan de migración (C3 a S3)

El proyecto contempla el desarrollo de los primeros prototipos con la
placa ESP32-C3 SuperMini (que posee 13 GPIOs expuestos) y la migración
final hacia el ESP32-S3 SuperMini (que posee 16 GPIOs), esto es
altamente probable debido a los requerimientos de Machine Learning, pero
debido al bajo costo monetario de los módulos, preferimos probar primero
con el de menor consumo. Tras analizar los esquemáticos

IEE2913 Diseño Eléctrico 20 de ambas placas, se confirma que la cantidad
de pines es suficiente. Aprovechando la matriz GPIO, se ha definido un
mapa de pines (Tabla 9) que intenta mantener la compatibilidad fı́sica
entre ambas arquitecturas:

       Periférico / Señal          Pin ESP32-C3        Pin ESP32-S3       Tipo de Señal
       Bus I2C (SDA)                    GPIO 8              GPIO 33         Digital Bidireccional
       Bus I2C (SCL)                    GPIO 9              GPIO 34         Digital Reloj
       Pantalla SPI (MOSI)              GPIO 6              GPIO 6          Digital Salida
       Pantalla SPI (SCK)               GPIO 4              GPIO 4          Digital Reloj
       Pantalla SPI (CS)                GPIO 5              GPIO 5          Digital Salida
       Pantalla SPI (DC)                GPIO 3              GPIO 3          Digital Salida
       Pantalla SPI (RST)               GPIO 7              GPIO 7          Digital Salida
       ECG (AD8232 Out)               GPIO 0 (A0)           GPIO 1          Analógica (ADC)
       Interrupción IMU (INT1)         GPIO 1              GPIO 2          Entrada Digital
       Botón 1 (Seleccionar)           GPIO 10             GPIO 14         Entrada Digital (Pull-up)
       Botón 2 (Arriba)                GPIO 20             GPIO 15         Entrada Digital (Pull-up)
       Botón 3 (Abajo)                 GPIO 21             GPIO 16         Entrada Digital (Pull-up)

                       Cuadro 9: Mapeo definitivo de señales para placas SuperMini.

6.4. Arquitectura de Firmware (FreeRTOS)

Para gestionar la concurrencia de múltiples sensores y garantizar
tiempos de ejecución deterministas, el microcontrolador ESP32 operará
bajo el sistema operativo en tiempo real FreeRTOS. La arquitectura
interna del dispositivo se estructurará de la siguiente manera: Gestión
de Tareas (Multitasking): El sistema se dividirá en tareas
independientes asignadas a distintos niveles de prioridad para optimizar
el uso del procesador: • Tarea de Adquisición (Prioridad Alta):
Encargada de muestrear el ADC (para el ECG) me- diante acceso directo a
memoria (DMA) y leer los buses I2C a frecuencias precisas. • Tarea de
Procesamiento Edge (Prioridad Media): Ejecutará el modelo de TinyML para
la clasificación de actividad y el algoritmo del contador de pasos de
forma local. • Tarea de Interfaz Gráfica (Prioridad Media-Baja):
Actualizará la pantalla LCD vı́a SPI y gestionará las interrupciones de
los botones fı́sicos para la navegación fluida. • Tarea de Comunicación
BLE (Prioridad Baja): Empaquetará los datos y manejará la conexión con
el teléfono móvil de forma ası́ncrona, evitando bloquear el sistema
principal. Sincronización y Comunicación (IPC): Para evitar colisiones
en los buses de datos (como el bus I2C compartido por cuatro sensores
biométricos), se implementarán semáforos de exclusión mutua (Mutexes).
La transferencia de datos entre la tarea de adquisición y la de
procesamiento se realizará mediante colas de mensajes (Queues),
asegurando que no se pierdan muestras crı́ticas. Gestión Energética
Activa: El planificador de FreeRTOS se configurará en conjunto con las
rutinas de bajo consumo del ESP32 (Light Sleep). El sistema dormirá la
mayor parte del tiempo y será despertado únicamente por interrupciones
de hardware o temporizadores internos.

IEE2913 Diseño Eléctrico 21 6.4.1. Frecuencias de Muestreo y
Optimización de Memoria

Para optimizar el uso de la memoria SRAM del microcontrolador y el ancho
de banda de transmisión Bluetooth, se diferenciarán las tasas de
muestreo interno (hardware) de las tasas de registro lógico: Sensores
Dinámicos (IMU y PPG): Muestreados internamente a frecuencias elevadas
(50 a 100 Hz) para alimentar el modelo de Machine Learning y algoritmos
de biometrı́a. Los datos crudos no serán almacenados; el microcontrolador
extraerá las caracterı́sticas en tiempo real y solo registrará en el
buffer de transmisión los resultados procesados a 1 Hz. Sensores de
Dinámica Lenta (Temperatura): El sensor operará a 1 Hz, dada la alta
inercia térmica del cuerpo humano. Telemetrı́a Cruda (ECG): Durante la
medición puntual, el ADC operará a 100 Hz mı́nimo. Estos datos sı́ serán
almacenados de forma cruda (raw ) y transmitidos inmediatamente por BLE,
delegando su procesamiento a la nube. La Fig.7 presenta el diagrama de
bloques del firmware, mostrando la arquitectura de tareas concurrentes
bajo FreeRTOS, los periféricos de hardware conectados al
microcontrolador y los mecanismos de comu- nicación entre procesos
(IPC). Las cajas grises representan los periféricos de hardware, las
cajas azules las tareas de software, las cajas amarillas las colas de
mensajes (Queues) y el cı́rculo rojo el semáforo de exclusión mutua
(Mutex ) que protege el bus I2C compartido. Las flechas continuas
indican flujo de datos y las discontinuas representan interrupciones
ası́ncronas.

Microcontrolador ESP32 (FreeRTOS) Tarea Adquisición ADC / DMA Prioridad:
Alta (ECG 100Hz) (Muestreo de sensores) Queue: Datos

         Bus I2C              Tarea Procesamiento                        Tarea Com. BLE
       (IMU, PPG,       M                                                                       BLE    Smartphone
                                Prioridad: Media                          Prioridad: Baja
          Temp)                                                                                       (App Gateway)
                                 (TinyML, Pasos)                       (Empaquetado y envı́o)
                                                                Queue: Tx BLE


     Botones fı́sicos   ISR   Tarea Interfaz Gráfica
    (Interrupciones)                                           SPI      Pantalla LCD
                               Prioridad: Media-Baja
                                                                          (ST7789)
                                (Actualización SPI)



                                     Periférico de hardware
                                     Tarea FreeRTOS
                                     Cola de mensajes (Queue)
                                     Mutex (exclusión mutua)
                                     Flujo de datos
                                     Interrupción ası́ncrona (ISR)
                                     Enlace BLE inalámbrico


                Figura 7: Diagrama de bloques del firmware y gestión de tareas en FreeRTOS.

6.5. Arquitectura de Ecosistema (App y Servidor)

El dispositivo actuará como un nodo de adquisición en el borde (Edge),
delegando el almacenamiento histórico y el análisis profundo a una
arquitectura externa compuesta por una Aplicación Móvil y un backend en
la nube.

IEE2913 Diseño Eléctrico 22  Estrategia de Transmisión (BLE): Para
optimizar el consumo energético, la transmisión Blue- tooth Low Energy
(BLE) operará en dos modalidades. La telemetrı́a continua (pasos,
temperatura, SpO2, actividad) se enviará a la aplicación móvil en
paquetes consolidados de forma periódica. Para la medición del
electrocardiograma, se abrirá un canal de alto rendimiento que
transmitirá los datos a 100 Hz en tiempo real durante los 30 segundos de
captura. Aplicación Móvil (Android): Funcionará como el enlace principal
o Gateway. Se encargará de emparejarse con el dispositivo, visualizar el
resumen en tiempo real, calibrar métricas del usuario y subir los
paquetes de datos al servidor. Backend Principal (Firebase): Durante las
primeras fases de desarrollo, se utilizará el ecosis- tema de Firebase.
Cloud Firestore almacenará los perfiles y métricas históricas, mientras
que el procesamiento pesado se ejecutará mediante Cloud Functions
utilizando Python. Esto asegura alta disponibilidad y un despliegue
rápido. Proyección a Servidor Propio (Edge Server): Como objetivo
extendido, y sujeto a la holgura en la Carta Gantt, se contempla la
migración del backend a un servidor propio gestionado mediante una
Raspberry Pi 3B+. Esta arquitectura On-Premise (utilizando una API
propia, bases de datos de series de tiempo e interfaces como Grafana)
eliminarı́a la dependencia de servicios externos y maximizarı́a la
privacidad de los datos biométricos. La Fig.8 ilustra esta arquitectura
de ecosistema. En el diagrama, las flechas azules representan la
telemetrı́a continua enviada a baja frecuencia (1 Hz), mientras que las
flechas rojas indican el canal de alta velocidad para la transmisión de
datos crudos de ECG (100 Hz). Las cajas verdes corresponden a
dispositivos fı́sicos, las azules a la aplicación de software y las
naranjas al ecosistema en la nube. El nodo punteado en púrpura
representa la proyección futura del servidor local.

                                                                                             Cloud Firestore
                                                                                                (Perfiles)
                      BLE (1 Hz)
                  Telemetrı́a Continua     Aplicación

SupaClock Móvil Wi-Fi / 4G Ecosistema Actualiza BPM (Nodo Edge) (Gateway
Firebase BLE (100 Hz) Android) ECG Crudo Cloud Functions (Python)
Proyección local Algoritmo ECG Dispositivo fı́sico Aplicación de software
Servicio en la nube Proyección futura BLE: Telemetrı́a (1 Hz) Raspberry
BLE: ECG crudo (100 Hz) Pi 3B+ Enlace futuro (Edge Server)

Figura 8: Arquitectura de ecosistema: flujo de datos desde el
dispositivo Edge hacia la aplicación móvil y el backend en la nube.

6.5.1. Algoritmos de Procesamiento de Señales (ECG en Servidor)

Para la estimación de la frecuencia cardı́aca a partir de la señal del
AD8232, se implementará el algoritmo de Pan-Tompkins de manera ası́ncrona
en el servidor (ej. funciones nativas en Python): 1. Filtrado Digital
Pasabanda: Atenuación del ruido de alta frecuencia (interferencia
electro- magnética) y eliminación de la deriva de la lı́nea base
(baseline wander ). 2. Procesamiento Morfológico: Aplicación de un
filtro derivativo y función de cuadratura para resaltar la pendiente
aguda del complejo QRS.

IEE2913 Diseño Eléctrico 23  3. Detección de Picos R: Integración de
ventana móvil combinada con umbrales adaptativos (adap- tive
thresholding) para identificar el instante exacto de cada latido. 4.
Cálculo de Métricas: Evaluación de la distancia temporal entre picos
consecutivos (intervalos R- R) para calcular los latidos por minuto
(BPM) y evaluar la Variabilidad de la Frecuencia Cardı́aca (HRV).

7.   Bosquejo de la solución

La Fig.9 presenta el bosquejo preliminar del prototipo SupaClock, el
cual ilustra las principales vistas y dimensiones del dispositivo. Desde
la vista cenital se aprecia la interfaz del usuario con la pantalla
principal y los ı́conos de navegación. En la vista frontal se observan
los tres botones fı́sicos ubicados en el costado derecho, los cuales
cumplirán las funciones de navegación (arriba/abajo) y
selección/acción. La vista lateral derecha muestra la ubicación de los
electrodos de contacto seco para la medición de ECG, posicionados en la
zona más cercana a la mano del usuario. Finalmente, la vista posterior
exhibe el puerto USB-C de carga y la correa ajustable. Las cotas
indicadas corresponden a las dimensiones máximas de diseño: largo ≤ 80
mm, ancho ≤ 50 mm y altura ≤ 30 mm.

Figura 9: Bosquejo preliminar del prototipo SupaClock con vistas
cenital, frontal, lateral y posterior.

8.   Metodologı́a de trabajo

Para el proyecto se colocarán los avances que cada persona va realizando
en una tarea en particular, para esto se utilizará la plataforma Notion
y Git (github), en las cuales se organiza la creación y ejecución de
tareas, registran los avances de los proyectos de código
respectivamente. También realizaremos trabajo presencial en conjunto al
menos cuatro módulos a la semana se realizarán jornadas remotas
orientadas principalmente para los bloques de programación y escritura
de código.

IEE2913 Diseño Eléctrico 24 8.1. Áreas y roles

El desarrollo del proyecto se divide en tres áreas principales:
Hardware, Software y Procesamiento, e Interfaz y Sistema. Esta división
permite un trabajo modular, paralelo y eficiente, facilitando la
integración final del sistema.

8.1.1. Área 1: Hardware

La primera área es Hardware, esta contempla los sensores, el diseño de
la PCB, el manejo fı́sico de la energı́a y la integración de estos
bloques. Dentro de sus tareas se encuentran la selección de componentes,
diseño esquemático, diseño de PCB, integración fı́sica, diseño del
sistema de alimentación y pruebas eléctricas. A cargo de esta área se
encuentra Benjamı́n Sepúlveda, encargado de registrar los avances
asociados a esta rama. Los roles asociados a esta área son: Diseñador de
Hardware: encargado del diseño completo del sistema, tanto a nivel
fı́sico como eléctrico. Energı́a: responsable del manejo de la baterı́a,
sistema de carga y optimización del consumo energético. Electrónica:
encargado de la conexión entre sensores y microcontrolador, además del
debug de hardware.

8.1.2. Área 2: Software y Procesamiento

La segunda área es Software y Procesamiento, la cual cubre firmware,
algoritmos y machine learning. Dentro de sus tareas se encuentra la
lectura de sensores y su comunicación, filtrado de señales de ECG,
implementación del contador de pasos y gestión del sistema operativo
FreeRTOS. A cargo de esta área se encuentra Pablo Uribe, encargado de la
gestión de los procesos asociados. Los roles asociados a esta área son:
Firmware: encargado de la arquitectura del código, implementación de
drivers y comunicación con los sensores. Procesamiento y Señales:
responsable del filtrado de señales (ECG), cálculo de métricas
fisiológi- cas y desarrollo del contador de pasos. Machine Learning:
encargado de implementar y optimizar el modelo de clasificación de
actividades en el dispositivo (TinyML), considerando restricciones de
consumo y latencia.

8.1.3. Área 3: Interfaz y Sistema

La tercera área es Interfaz y Sistema, la cual contempla la interacción
con el usuario, visualización de datos y comunicación externa del
dispositivo. Dentro de sus tareas se encuentra el diseño de la interfaz
gráfica (GUI), navegación del sistema, despliegue de métricas en
pantalla, implementación de comunicación inalámbrica (BLE/WiFi) y la
integración completa del sistema.

IEE2913 Diseño Eléctrico 25 A cargo de esta área se encuentra Tomás
Avendaño, responsable de coordinar la integración final del dispositivo.
Los roles asociados a esta área son: Desarrollador UI/UX: encargado del
diseño e implementación de la interfaz gráfica, asegurando una
experiencia de usuario clara e intuitiva. Comunicaciones: responsable de
la transmisión de datos mediante BLE/WiFi hacia dispositivos externos o
plataformas externas. Integrador de Sistema: encargado de la integración
de hardware, software e interfaz, además de realizar pruebas completas
del sistema.

8.2. Trabajo colaborativo

El desarrollo del proyecto se basa en una metodologı́a de trabajo
colaborativo, donde cada área cuenta con un encargado responsable de su
correcta ejecución. Sin embargo, esto no implica que el trabajo recaiga
exclusivamente en dicha persona, sino que su rol principal es coordinar,
organizar y supervisar las tareas dentro de su área. Cada encargado
deberá distribuir los roles definidos previamente entre los integrantes
del equipo, asig- nando responsabilidades especı́ficas de acuerdo a las
necesidades del proyecto y a las habilidades de cada miembro. De esta
forma, se busca asegurar una carga de trabajo equilibrada, fomentar la
especialización en distintas áreas y mejorar la eficiencia del
desarrollo. Adicionalmente, se promoverá la colaboración entre áreas,
especialmente en etapas de integración, donde será necesario coordinar
esfuerzos entre hardware, software e interfaz para asegurar el correcto
funciona- miento del sistema completo. Este enfoque permite reducir
errores de integración y facilita la detección temprana de problemas.

8.3. Asignación de tareas

La asignación de tareas se realizará de manera dinámica y progresiva, en
función del avance del proyecto y los hitos establecidos en la
planificación. Cada área definirá sus propias tareas especı́ficas, las
cuales serán desglosadas en actividades más pequeñas y manejables. Para
la gestión de estas tareas se utilizarán herramientas colaborativas como
Notion y GitHub, las cuales permitirán llevar un seguimiento continuo
del progreso, asignar responsables, establecer plazos y docu- mentar los
avances realizados. Cada tarea contará con un responsable principal,
quien será el encargado de su ejecución, sin perjuicio de recibir apoyo
de otros integrantes cuando sea necesario. Asimismo, se establecerán
instancias periódicas de revisión, donde cada área reportará sus
avances, difi- cultades y próximos pasos. Esto permitirá mantener una
visión global del estado del proyecto, facilitar la toma de decisiones y
asegurar el cumplimiento de los objetivos planteados. Finalmente, la
asignación de tareas será flexible, permitiendo reasignaciones en caso
de sobrecarga de trabajo, dificultades técnicas o cambios en los
requerimientos del proyecto, garantizando ası́ la continuidad y
adaptabilidad del desarrollo.

IEE2913 Diseño Eléctrico 26 9. Objetivos a cumplir

El desarrollo del proyecto SupaClock tiene como objetivo principal el
diseño e implementación de un dispositivo wearable funcional, capaz de
adquirir, procesar y visualizar variables biométricas en tiempo real,
cumpliendo con los requerimientos de portabilidad, autonomı́a energética
y robustez del sistema. Para alcanzar este objetivo general, se plantean
los siguientes objetivos especı́ficos: Diseñar e implementar el sistema
de adquisición de señales biométricas, incluyendo sensores de frecuencia
cardı́aca, SpO2, temperatura, aceleración y ECG. Desarrollar la
arquitectura de procesamiento basada en un microcontrolador ESP32, capaz
de ejecu- tar algoritmos de filtrado, análisis de señales y
clasificación de actividad mediante Machine Learning. Implementar una
interfaz gráfica que permita visualizar en tiempo real las métricas
obtenidas, junto con un sistema de navegación intuitivo. Diseñar un
sistema de alimentación autónomo basado en baterı́a LiPo, incluyendo
gestión de carga, regulación de voltaje y monitoreo de baterı́a. Integrar
todos los subsistemas en una PCB funcional, asegurando la correcta
comunicación entre módulos y la estabilidad del sistema. Validar
experimentalmente el funcionamiento del dispositivo mediante pruebas de
sensores, consumo energético y desempeño del sistema completo.

9.1. Hitos

El desarrollo del proyecto se organiza en distintos hitos de avance, los
cuales permiten evaluar el progreso del sistema de manera incremental.

9.1.1. Hito 5 %: Definición y planificación inicial

En esta etapa se establece la base conceptual del proyecto. Se espera
contar con: Prueba preliminar de componentes por separado. Crear la base
de datos en Firebase. Crear el servidor en Firebase. Crear el
repositorio en GitHub.

9.1.2. Hito 25 %: Validación de subsistemas iniciales

En este punto se busca validar el funcionamiento básico de los
principales módulos del sistema: Implementación del bloque de energı́a
(BMS y regulación). Integración inicial de sensores. Pruebas
preliminares de adquisición de datos. Primera integración con
microcontrolador de los sensores en conjunto. Validación básica de
algoritmos.

IEE2913 Diseño Eléctrico 27 9.1.3. Hito 60 %: Integración funcional del
sistema

En esta etapa el sistema comienza a operar de manera integrada: Diseño e
implementación de la PCB. Integración de sensores en la PCB. Desarrollo
de la interfaz gráfica en pantalla. Implementación de algoritmos de
procesamiento (ECG, pasos). Integración de comunicación inalámbrica
(Bluetooth). Pruebas funcionales del sistema completo.

9.1.4. Hito 90 %: Validación avanzada y optimización

En esta fase el sistema se encuentra prácticamente completo y se enfoca
en su validación: Pruebas biométricas completas en condiciones reales.
Optimización del consumo energético. Ajuste de algoritmos de Machine
Learning. Validación de estabilidad del sistema. Integración final de
hardware, software e interfaz.

9.1.5. Hito 100 %: Entrega final

Corresponde a la finalización del proyecto: Dispositivo completamente
funcional e integrado. Validación final del sistema. Documentación
completa del proyecto. Presentación final del prototipo. La Fig.10
presenta la carta Gantt del proyecto, la cual desglosa las actividades
planificadas para cada hito en un horizonte temporal que abarca el
semestre académico completo. Las barras de color representan la duración
estimada de cada tarea, permitiendo visualizar el paralelismo entre
actividades de hardware, software e interfaz, ası́ como identificar las
dependencias crı́ticas entre subsistemas.

IEE2913 Diseño Eléctrico 28  Figura 10: Carta Gantt del proyecto
SupaClock con la planificación temporal de actividades por hito.

10. Presupuesto estimado

La Tabla 10 presenta el listado de materiales (Bill of Materials, BOM)
con el costo de los componentes principales del prototipo. Los precios
corresponden a las transacciones reales realizadas a través de la
plataforma AliExpress durante marzo de 2026, los cuales ya incluyen el
19 % correspondiente al IVA de importación, junto a estimaciones
conservadoras para los demás componentes genéricos. Cabe destacar que la
fabricación de la placa de desarrollo prototipo es otorgada
gratuitamente por el laboratorio; no obstante, para el objetivo
extendido (Bonus "Versión Pro"), se contempla la manufactura externa
(JLCPCB o PCBWay) estimando un valor de \$30.000 CLP con envı́o incluido.

IEE2913 Diseño Eléctrico 29  Componente Cantidad Referencia Costo (CLP)
ESP32-C3 SuperMini 3 MCU principal \$8.211 ESP32-S3 SuperMini 1 MCU
migración \$3.500 Módulo de expansión ESP32-C3 1 Soporte desarrollo
\$2.235 MAX30102 (Módulo PPG) 2 SpO2 / HR \$3.853 MAX30205 (Sensor
Temp.) 1 Temperatura clı́nica \$5.949 AD8232 (Módulo ECG) 1
Electrocardiograma \$3.500 BMI160 (Módulo IMU) 2 Acelerómetro + Giro
\$3.204 MAX17048 (Fuel Gauge) 1 Monitor de baterı́a \$3.000 Módulo
Cargador TP4056 Type-C 1 Carga y protección \$1.142 ME6211 / AP2112K
(LDO 3.3V) 1 Regulador de voltaje \$300 Pantalla IPS LCD 1.69"ST7789 1
Display principal \$3.195 Pantalla TFT LCD 1.3" 1 Display alternativo
\$2.059 Conectores 24PIN FPC 10 Interfaz pantallas \$2.161 Baterı́a LiPo
3.7V 400 mAh 1 Alimentación \$3.000 Pulsadores táctiles SMD 50 Botones
navegación \$2.248 Conector USB-C hembra 1 Puerto de carga \$500 Pernos
M3 10 Contactos ECG \$2.264 Componentes pasivos (R, C) --- Varios
\$1.000 Filamento PLA (carcasa 3D) --- Impresión 3D \$2.000 PCB
prototipo 1 FabLab Gratis Subtotal (Prototipo) --- --- \$53.321 PCB SMD
(JLCPCB/PCBWay) 1 Versión Pro (Bonus) \$30.000 Total con Bonus --- ---
\$83.321

Cuadro 10: Bill of Materials (BOM) con precios reales de adquisición
(IVA incl.) para componentes crı́ticos.

11. Referencias

Referencias

\[1\] CIPER Chile. Resultados de la encuesta nacional de actividad
fı́sica y deporte: una oportunidad para fortalecer el bienestar integral
en chile, 2025. Disponible en: https://www.ciperchile.cl/
2025/05/24/resultados-de-la-encuesta-nacional-de-actividad-fisica-y-deporte/.
\[2\] UC Christus. 5 razones por las que chile es el paı́s con más
obesidad de lati- noamérica, 2025. Disponible en:
https://www.ucchristus.cl/blog-salud-uc/articulos/2025/
5-razones-por-las-que-chile-es-el-pais-con-mas-obesidad-de-latinoamerica.
\[3\] Espressif Systems. ESP32-C3 Series Datasheet, 2024. Disponible en:
https://www.espressif.com/
sites/default/files/documentation/esp32-c3_datasheet_en.pdf.

IEE2913 Diseño Eléctrico 30  \[4\] Espressif Systems. ESP32-S3 Series
Datasheet, 2024. Disponible en: https://www.espressif.com/
sites/default/files/documentation/esp32-s3_datasheet_en.pdf. \[5\]
Amazon Web Services. FreeRTOS Reference Manual, 2024. Disponible en:
https://www.freertos. org/Documentation/RTOS_book.html. \[6\] Analog
Devices (Maxim Integrated). MAX30102: High-Sensitivity Pulse Oximeter
and Heart- Rate Sensor for Wearable Health, 2018. Disponible en:
https://www.analog.com/media/en/
technical-documentation/data-sheets/MAX30102.pdf. \[7\] Analog Devices
(Maxim Integrated). MAX30205: Human Body Temperature Sensor, 2016.
Disponi- ble en:
https://www.analog.com/media/en/technical-documentation/data-sheets/MAX30205.
pdf. \[8\] Analog Devices. AD8232: Single-Lead, Heart Rate Monitor Front
End, 2012. Disponible en: https:
//www.analog.com/media/en/technical-documentation/data-sheets/ad8232.pdf.
\[9\] Bosch Sensortec. BMI160: Small, Low Power Inertial Measurement
Unit, 2020. Dispo- nible en:
https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/
bst-bmi160-ds000.pdf. \[10\] Sitronix Technology. ST7789V: 240RGB x 320
Dot 262K Color with Frame Memory Single- Chip TFT Controller/Driver,
2014. Disponible en: https://www.newhavendisplay.com/appnotes/
datasheets/LCDs/ST7789V.pdf. \[11\] Edge Impulse Inc. Edge Impulse
Documentation, 2024. Disponible en: https://docs.edgeimpulse. com/.
\[12\] Google. TensorFlow Lite for Microcontrollers, 2024. Disponible
en: https://www.tensorflow.org/ lite/microcontrollers. \[13\] Jiapu Pan
and Willis J. Tompkins. A real-time qrs detection algorithm. IEEE
Transactions on Biomedical Engineering, BME-32(3):230--236, 1985. doi:
10.1109/TBME.1985.325532. \[14\] Nordic Semiconductor. nRF52840 Product
Specification, 2021. Disponible en: https://docs.
nordicsemi.com/bundle/nRF52840_PS_v1.8/page/keyfeatures_html5.html.
\[15\] STMicroelectronics. STM32WB55xx Datasheet, 2022. Disponible en:
https://www.st.com/ resource/en/datasheet/stm32wb55cg.pdf. \[16\]
InvenSense (TDK). MPU-6050: Six-Axis MEMS MotionTracking Device, 2013.
Disponible en: https:
//invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf.
\[17\] STMicroelectronics. LSM6DS3: iNEMO inertial module, 2017.
Disponible en: https://www.st. com/resource/en/datasheet/lsm6ds3.pdf.
\[18\] Analog Devices (Maxim Integrated). MAX30100: Pulse Oximeter and
Heart-Rate Sen- sor IC for Wearable Health, 2014. Disponible en:
https://www.analog.com/media/en/
technical-documentation/data-sheets/MAX30100.pdf. \[19\] Analog Devices
(Maxim Integrated). MAX86141: Optical Data Acquisition System, 2019.
Disponible en:
https://www.analog.com/media/en/technical-documentation/data-sheets/
MAX86140-MAX86141.pdf. \[20\] Melexis. MLX90614 Family: Single and Dual
Zone Infra Red Thermometer in TO- 39, 2019. Disponible en:
https://www.melexis.com/-/media/files/documents/datasheets/
mlx90614-datasheet-melexis.pdf.

IEE2913 Diseño Eléctrico 31 \[21\] Analog Devices (Maxim Integrated).
DS18B20: Programmable Resolution 1-Wire Digital Ther- mometer, 2019.
Disponible en: https://www.analog.com/media/en/technical-documentation/
data-sheets/DS18B20.pdf. \[22\] NanJing Top Power ASIC Corp. TP4056: 1A
Standalone Linear Li-Ion Battery Charger with Thermal Regulation, 2008.
Disponible en: https://dlnmh9ip6v2uc.cloudfront.net/datasheets/
Prototyping/TP4056.pdf. \[23\] Texas Instruments. bq2407x Standalone
1-Cell 1.5-A Linear Battery Charger with Power Path Management, 2019.
Disponible en: https://www.ti.com/lit/ds/symlink/bq24075.pdf. \[24\]
Microchip Technology. MCP73831/2: Miniature Single-Cell, Fully
Integrated Li-Ion, Li-Polymer Charge Management Controllers, 2014.
Disponible en: https://ww1.microchip.com/downloads/
en/DeviceDoc/20001984g.pdf. \[25\] Analog Devices (Maxim Integrated).
MAX17043/MAX17044: Compact, Low-Cost 1S/2S Fuel Gauges with ModelGauge,
2014. Disponible en: https://www.analog.com/media/en/
technical-documentation/data-sheets/MAX17043-MAX17044.pdf. \[26\] Texas
Instruments. bq27441-G1 System-Side Impedance Track Fuel Gauge, 2018.
Disponible en: https://www.ti.com/lit/ds/slusbq8b/slusbq8b.pdf. \[27\]
Micro One Electronic Inc. ME6211: High Speed LDO Regulators, 2019.
Disponible en: https://
datasheet.lcsc.com/lcsc/1811131510_MICRONE-Nanjing-Micro-One-Elec-ME6211C33M5G-N\_
C82942.pdf. \[28\] Advanced Monolithic Systems. AMS1117: 1A LOW DROPOUT
VOLTAGE REGULATOR, 2006. Disponible en:
http://www.advanced-monolithic.com/pdf/ds1117.pdf. \[29\] Texas
Instruments. TPS7A02 200-mA, Nanopower IQ (25 nA), Low-Dropout
Regulator, 2022. Dis- ponible en:
https://www.ti.com/lit/ds/symlink/tps7a02.pdf. \[30\] Solomon Systech.
SSD1306: 128 x 64 Dot Matrix OLED/PLED Segment/Common Driver with
Controller, 2008. Disponible en:
https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf. \[31\] Sharp
Microelectronics. LS013B7DH03: Memory LCD Data Sheet, 2014. Disponible
en: https: //www.sharpsma.com/documents/20181/0/LS013B7DH03_Spec.pdf.

IEE2913 Diseño Eléctrico 32 

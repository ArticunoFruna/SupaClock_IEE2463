# Guía Práctica de Modelado: SupaClock V2 en Fusion 360 (Método A + D)

Esta guía te guiará paso a paso para reconstruir de forma **profesional y paramétrica** la carcasa y los botones de SupaClock V2 en **Autodesk Fusion 360**, utilizando conjuntamente los planos técnicos generados y los perfiles vectoriales `.dxf` listos para usar en `mechanical/dxf_sketches/`.

---

## Recursos Generados en tu Carpeta `mechanical/`
1. **Plano de Ingeniería Industrial:** [supaclock_v2_blueprint.pdf](file:///home/articunot/Documents/PlatformIO/Projects/SupaClock/mechanical/supaclock_v2_blueprint.pdf) (Contiene 6 hojas con vistas ortográficas acotadas y secciones transversales).
2. **Perfiles Croquis 2D en DXF:** Ubicados en `mechanical/dxf_sketches/`:
   * `bottom_outer.dxf`: Contorno exterior del case en Z=0 (antes de chaflán, 98x79 mm, R12).
   * `top_outer.dxf`: Contorno exterior en Z=22 (tapered, 94x75 mm, R10).
   * `inner_cavity.dxf`: Contorno de la pared interna (94x75 mm, R10).
   * `bottom_cuts.dxf`: Ventanas de sensores, electrodos y centros de standoffs en el piso.
   * `top_cuts.dxf`: Ventana del display y pilares en el techo.
   * `lug.dxf`: Croquis 2D lateral de un lug para la correa de reloj.
   * `button_cap.dxf`: Sección transversal del botón lista para herramienta *Revolve* (Revolución).

---

## Flujo de Modelado en Fusion 360 (Paso a Paso)

### Paso 1: Configurar Parámetros del Proyecto (Método A - Paramétrico)
Antes de dibujar nada, define las variables globales para que puedas alterar las medidas en el futuro con un clic:
1. En la barra superior, ve a **Modify > Change Parameters** (Modificar > Cambiar parámetros).
2. Haz clic en el botón `+` e introduce los siguientes **User Parameters**:
   * `W = 98.0 mm` (Ancho total)
   * `L = 79.0 mm` (Largo total)
   * `H = 22.0 mm` (Altura total)
   * `taper = 2.0 mm` (Inclinación de las paredes)
   * `wall = 2.0 mm` (Grosor uniforme de pared)
   * `seam = 4.0 mm` (Plano de corte entre bottom y top case)

---

### Paso 2: Crear el Sólido Base Tapered (Afilado)
Utilizaremos los contornos DXF para crear la forma exterior estilizada:
1. **Perfil Base (Z=0):**
   * Haz clic en **Insert > Insert DXF** (Insertar > Insertar DXF).
   * Selecciona el plano **XY (horizontal)** y el archivo `bottom_outer.dxf`. Acepta.
2. **Plano de Altura:**
   * Ve a **Construct > Offset Plane** (Construir > Plano de desfase).
   * Selecciona el plano XY, y desplázalo hacia arriba a una distancia de `H` (22 mm).
3. **Perfil Superior (Z=22):**
   * Haz clic en **Insert > Insert DXF**.
   * Selecciona el plano de desfase que acabas de crear en Z=22, y elige el archivo `top_outer.dxf`.
4. **Loft (Solevantado):**
   * Ve a **Create > Loft** (Crear > Solevantar).
   * Selecciona el perfil del croquis inferior (Z=0) y el superior (Z=22).
   * Fusion creará un sólido de transición cónico perfecto con las esquinas redondeadas y el taper de 2mm.
5. **Chaflanes Horizontales:**
   * Ve a **Modify > Fillet** (o presiona la tecla `F`).
   * Selecciona la arista perimetral inferior (Z=0) y la superior (Z=22).
   * Aplica un filete/chaflán de **1.5 mm** para dar el acabado premium que se observa en los renders.

---

### Paso 3: Vaciado y Separación de Piezas
Ahora huecaremos el envelope y lo separaremos en Bottom Case y Top Case:
1. **Crear el Plano de Junta (Seam):**
   * Ve a **Construct > Offset Plane**, selecciona el plano XY (Z=0) e introduce la altura `seam` (4 mm).
2. **Vaciado de Paredes:**
   * Ve a **Modify > Shell** (Vaciado).
   * Selecciona la cara inferior del sólido (Z=0) y aplica un grosor de pared de `wall` (2 mm). Esto dejará la carcasa hueca y con paredes uniformes.
3. **Cortar la Carcasa:**
   * Ve a **Modify > Split Body** (Dividir cuerpo).
   * Selecciona el cuerpo de la carcasa como *Body to Split*.
   * Selecciona el plano de desfase Z=4 (plano de junta) como *Splitting Tool*.
   * Presiona OK. Ahora tendrás dos cuerpos independientes en tu línea de tiempo: **Bottom Case** (abajo, 4mm) y **Top Case** (arriba, 18mm).

---

### Paso 4: Detalles del Bottom Case (Agujeros e Implantes)
1. **Croquis de Detalles:**
   * Selecciona la cara interior plana del Bottom Case (Z = 2 mm).
   * Ve a **Insert > Insert DXF** y selecciona `bottom_cuts.dxf`.
2. **Ventanas de Sensores:**
   * Usa la herramienta **Extrude** (tecla `E`). Selecciona los rectángulos correspondientes a la ventana del sensor MAX30102 y MAX30205.
   * Extruye hacia abajo con la opción **Cut** (Cortar) atravesando el piso (Z=0).
3. **Agujeros de Electrodos:**
   * Selecciona los tres círculos de electrodos de Ø6mm y extruye hacia abajo en modo **Cut**.
4. **Standoffs (Pilares de soporte PCB):**
   * Crea un boceto en el piso interior (Z=2).
   * Dibuja 4 círculos concéntricos de Ø5.5 mm en los mismos centros provistos por los círculos de Ø2.7 mm en el DXF.
   * Extruye estos cilindros de Ø5.5 mm hacia **arriba** por `2.0 mm` (llegando a Z=4) con la operación **Join** (Unir) al Bottom Case.
   * Vuelve a activar el croquis DXF y extruye en modo **Cut** los círculos internos de Ø2.7 mm hacia abajo para dejar el canal roscado de los tornillos de fijación M3.

---

### Paso 5: Detalles del Top Case
1. **Ventana de Display:**
   * Selecciona la cara del techo interior del Top Case (Z = 20 mm).
   * Ve a **Insert > Insert DXF** y selecciona `top_cuts.dxf`.
   * Selecciona el rectángulo de la ventana de display (28x34 mm) y extruye hacia arriba en modo **Cut** atravesando el techo de 2mm (hasta Z=22).
2. **Pilares de Techo:**
   * Usando las referencias de centros de pilares provistas por `top_cuts.dxf`, dibuja círculos concéntricos de Ø5.5 mm.
   * Extrúyelos hacia **abajo** en modo **Join** hasta apoyar en el PCB (Z=5.6 en top-case-local). Haz un agujero concéntrico de Ø2.7 mm (rosca M3 self-tap).

---

### Paso 6: Agregar los Lugs de la Correa (Tapa Superior)
1. **Importar el Perfil Lateral del Lug:**
   * Crea un plano de desfase a la distancia lateral donde va el lug (`X = 35.5 mm`).
   * Selecciona ese plano e inserta `lug.dxf`.
   * El croquis contiene la silueta tipo "stadium" del lug.
2. **Extrusión y Agujero:**
   * Extruye la silueta por un grosor de `lug_thickness` (5.0 mm) hacia adentro del reloj.
   * Realiza una perforación transversal con **Modify > Hole** (Agujero) de Ø1.8 mm usando el centro del spring bar del DXF como referencia.
3. **Espejo (Mirror):**
   * Utiliza la herramienta **Create > Mirror** (Simetría) de tipo *Features* para duplicar el lug al otro extremo en el eje X, y luego haz simetría en el plano medio Y para tener los 4 lugs perfectos.

---

### Paso 7: Modelar los Botones en un Componente Aparte (Método D)
Para modelar los tapones físicos del botón:
1. Crea un nuevo componente (`New Component`) y llámalo "Button Cap".
2. Selecciona un plano de boceto vertical (plano XZ o YZ).
3. Ve a **Insert > Insert DXF** y selecciona `button_cap.dxf`.
4. Verás el perfil exacto de la mitad del botón en corte (flange Ø6, stem Ø3.5, y pestaña de retención cónica Ø4.3).
5. Ve a **Create > Revolve** (Revolución).
6. Selecciona el perfil cerrado del botón y elige la arista recta horizontal inferior (eje de simetría) como el **Axis**.
7. Fusion 360 generará instantáneamente un cuerpo cilíndrico de revolución perfecto del botón en 3D, sin necesidad de realizar extrusiones o chaflanes complicados.

---

¡Felicidades! Al utilizar esta metodología combinada, logras un diseño **100% editable e industrial** en menos de la mitad del tiempo tradicional.

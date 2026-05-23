# SupaClock V2 — Hoja de cotas para reconstruccion en Fusion 360

Documento de referencia con todas las dimensiones criticas del case V2
(esquinas redondeadas + chamfer + lugs + taper) para reproducir el diseño
parametrico en Fusion 360 desde cero.

Todas las medidas en **milimetros**. Tolerancia recomendada: ±0.2mm para
impresion FDM, ±0.05mm para resina.

---

## 1. Sistema de coordenadas

```
            Z (vertical, hacia arriba)
            |
            |
            +------ X (largo, eje izq-der del reloj puesto)
           /
          /
         Y (corto, eje proximal-distal en la muneca)

Origen: esquina INFERIOR-IZQUIERDA-FRENTE del envelope exterior
        (Z=0 toca la piel, Z=H_total es la cara del display)
```

- **Bottom case**: Z = 0 a Z = 4 (la cara Z=0 toca la piel)
- **Seam (union)**: plano horizontal en Z = 4
- **Top case**: Z = 4 a Z = 22 (en frame top-case-local: Z=0 a Z=18)

Conversion PCB-local <-> case-local:
```
X_case = X_pcb + pcb_off_x = X_pcb + 6.5
Y_case = Y_pcb + pcb_off_y = Y_pcb + 6.0
```

---

## 2. Envelope exterior

Es UNA SOLA forma 3D que abarca todo el case (bottom + top). Bottom y top
son slices horizontales de esta forma.

| Parametro | Valor | Notas |
|---|---|---|
| Ancho (X) | 98.0 mm | base. Top esta INSET por `taper` |
| Largo (Y) | 79.0 mm | base. Top esta INSET por `taper` |
| Alto (Z) | 22.0 mm | total ensamblado |
| r_vert | 12.0 mm | radio de las 4 esquinas verticales |
| r_chamfer | 1.5 mm | chamfer en Z=0 (inferior) y Z=22 (superior) |
| taper | 2.0 mm | reduccion lineal del radio del cilindro de esquina entre Z=0 y Z=22 |

### Construccion sugerida en Fusion

1. **Sketch en XY plane** (Z=0): rectangulo 98×79 con `Sketch > Rectangle`,
   luego `Sketch > Fillet` con radio 12 en las 4 esquinas. → contorno
   inferior.

2. **Sketch en XY plane offset Z=22**: rectangulo (98−2×taper)×(79−2×taper)
   = 94×75, con fillet de radio (12 − taper) = 10. → contorno superior.

3. **Loft** entre los dos sketches → solido tapered.

4. **Fillet 3D** con radio 1.5 sobre las aristas top y bottom (las
   horizontales superiores e inferiores) para el chamfer.

5. Resultado: solido envelope. Ahora se hueca por dentro (paso siguiente).

---

## 3. Cavidad interior (pared uniforme 2mm)

La cavidad sigue la misma forma que el envelope pero offset 2mm hacia
adentro. En Fusion: usa `Modify > Shell` con grosor 2mm e indica las
caras top + bottom como "open" (para abrir las 2 caras donde se acoplan
bottom y top).

ALTERNATIVA: shell solo open en Z=4 (seam). Luego corta el solido en
Z=4 con un plano para separar las dos mitades.

---

## 4. Bottom case — features

Bottom case = parte del envelope entre Z=0 y Z=4, hueco arriba.

| Feature | Posicion (X,Y) | Z | Dimensiones | Notas |
|---|---|---|---|---|
| Piso | toda la base | 0 — 2 | 2mm grosor | conserva las esquinas redondeadas |
| Cavidad | toda | 2 — 4+ | abierta arriba | sigue el offset 2mm desde el envelope |
| Standoff MH1 | (10.0, 69.5) | 2 — 4 | Ø4mm OD, Ø1.8mm ID | self-tap M2 |
| Standoff MH2 | (88.0, 69.5) | 2 — 4 | Ø4mm OD, Ø1.8mm ID | self-tap M2 |
| Standoff MH3 | (10.0, 9.5) | 2 — 4 | Ø4mm OD, Ø1.8mm ID | self-tap M2 |
| Standoff MH4 | (88.0, 9.5) | 2 — 4 | Ø4mm OD, Ø1.8mm ID | self-tap M2 |
| Cutout MAX30102 | centro (52.0, 39.195) | 0 — 2 | 17 × 22 mm | pasa por TODO el piso |
| Cutout MAX30205 | centro (51.5, 23.0) | 0 — 2 | 14 × 10 mm | pasa por TODO el piso |
| Electrode hole E1 (RA) | centro (18.5, 37.5) | 0 — 2 | Ø6mm | electrodo seco M3 |
| Electrode hole E2 | centro (72.0, 38.0) | 0 — 2 | Ø6mm | electrodo seco M3 |
| Electrode hole E3 | centro (50.0, 10.0) | 0 — 2 | Ø6mm | electrodo seco M3 (RA functional) |

---

## 5. Top case — features

Top case = parte del envelope entre Z=4 (abs) y Z=22 (abs).
En el frame LOCAL del top case (Z=0 = seam):

| Feature | Posicion (X,Y) | Z (local top) | Dimensiones | Notas |
|---|---|---|---|---|
| Cavidad | toda | 0 — 16 | abierta abajo | open base = seam, ceiling Z=16 a Z=18 |
| Techo | toda | 16 — 18 | 2mm grosor | con ventana del display |
| Pilar P1 | (10.0, 69.5) | 1.6 — 16 | Ø4mm OD, Ø1.8mm ID | apoya sobre cara superior del PCB |
| Pilar P2 | (88.0, 69.5) | 1.6 — 16 | Ø4mm OD, Ø1.8mm ID | idem |
| Pilar P3 | (10.0, 9.5) | 1.6 — 16 | Ø4mm OD, Ø1.8mm ID | idem |
| Pilar P4 | (88.0, 9.5) | 1.6 — 16 | Ø4mm OD, Ø1.8mm ID | idem |
| **Ventana display** | centro (60.28, 41.0) | 15 — 18 | **28 × 34 mm** | atraviesa el techo |
| Boton SW1 | (96, 21.375, btn_z=3.5) | 3.5 ± 2 | Ø4mm | atraviesa pared +X, eje a lo largo de X |
| Boton SW2 | (96, 33.875, btn_z=3.5) | 3.5 ± 2 | Ø4mm | atraviesa pared +X, eje a lo largo de X |
| **USB-C** | (96, 54, 12.6) | 12.6 ± 2 | **10 × 4 mm** rect | atraviesa pared +X, eje a lo largo de X |
| Jack 3.5mm | (20.025, 0, 4.6) | 4.6 ± 3.25 | Ø6.5mm | atraviesa pared −Y, eje a lo largo de Y |

Las coordenadas de boton/USB/jack son: (X del centro del agujero EN la pared
INTERIOR, Y/Z del centro del agujero). El agujero atraviesa los 2mm de pared.

---

## 6. Lugs (4 unidades)

Forma desde arriba: **stadium** (rectangulo + medio circulo en la punta).
Forma desde el costado: rectangulo recto.

| Parametro | Valor |
|---|---|
| Cantidad | 4 (2 en lado -Y, 2 en lado +Y) |
| Espesor (X) | 5.0 mm |
| Sobresale del case (Y) | 7.0 mm |
| Altura (Z, en frame top-case-local) | 10.0 mm (de Z=5 a Z=15) |
| Forma de la punta | medio cilindro Ø5mm (radio 2.5mm) |
| Overlap con el case (Y) | 1.0 mm (para union limpia con la pared) |
| Spring bar Ø | **1.8 mm** (estandar 1.5mm + holgura) |
| Spring bar pasante? | Si, atraviesa todo el lug a lo largo de X |
| Ancho de correa libre | **22 mm** (gap entre caras interiores de los lugs) |
| Separacion center-to-center | 27 mm (= 22 + 5) |

### Posicion exacta de cada lug (centro del cilindro vertical de la punta)

```
Lugs lado -Y (delante):
  L1:  X = 35.5,  Y = -4.5   (Y_outer_ctr = 0 - (7 - 5/2) = -4.5)
  L2:  X = 62.5,  Y = -4.5

Lugs lado +Y (atras):
  L3:  X = 35.5,  Y = 83.5   (Y_outer_ctr = 79 + (7 - 5/2) = 83.5)
  L4:  X = 62.5,  Y = 83.5

Centros de los spring bar holes: mismos (X,Y) que arriba, a Z = 10 en
frame top-case-local (= Z = 14 absoluto).
```

### Como replicar en Fusion

1. Crea un sketch en el plano XZ a la altura Y donde quieres el lug.
2. Dibuja el perfil "stadium" del lug visto desde arriba.
3. Extrude el sketch en Z por 10mm.
4. Drill el spring bar hole con `Modify > Hole` a Ø1.8.
5. Combina con el cuerpo del case via `Combine > Join`.
6. Aplica `Pattern > Rectangular` o `Mirror` para las 4 instancias.

---

## 7. Button caps (piezas sueltas)

Se imprimen como pieza separada. Se insertan desde adentro del case hacia
afuera durante el ensamble.

| Parametro | Valor |
|---|---|
| Flange exterior Ø | 6.0 mm |
| Flange exterior altura | 1.5 mm |
| Stem Ø | 3.5 mm |
| Stem altura | 7.9 mm (= 2 pared + 5.7 air gap + 0.2 pretravel) |
| Retention lip altura | 0.8 mm |
| Retention lip Ø (extremo interno) | 4.3 mm |

Construccion en Fusion: revolve un perfil sobre el eje Z. Perfil:
```
   |
   |  flange D=6mm
   |/----+
   | h=1.5
   |\----+
   |
   |  stem D=3.5mm
   |  h=7.9-0.8=7.1
   |
   |  lip cone D=3.5 -> 4.3
   |  h=0.8
   |
```

---

## 8. Standard parts (compra fuera)

| Parte | Especificacion | Cantidad | Donde |
|---|---|---|---|
| Tornillo M2 | M2 × 8 mm self-tap o thread-forming | 4 | ferreteria / RS |
| Spring bar | Ø1.5 mm × largo 22 mm | 4 | tienda de relojes ($1 USD c/u) |
| Correa | 22 mm (cualquier estilo) | 1 | tienda de relojes |
| Perno M3 (electrodo) | M3 × 8 mm cabeza redondeada acero inoxidable | 3 | ferreteria |
| Tuerca M3 (electrodo) | M3 estandar | 3 | ferreteria |
| Jack 3.5mm | TRS panel-mount Ø6mm thread | 1 | AliExpress / Mouser |
| Placa de aluminio (térmico MAX30205) | 14 × 10 × 2.4 mm | 1 | corte custom |

---

## 9. Workflow recomendado en Fusion 360

1. **Crea un nuevo documento** con unidades en milimetros.

2. **Define los parametros nombrados** en `Modify > Change Parameters`:
   ```
   W = 98
   L = 79
   H = 22
   r_vert = 12
   r_chamfer = 1.5
   taper = 2
   grosor_pared = 2
   altura_base = 2
   pcb_off_x = 6.5
   pcb_off_y = 6
   ```
   Esto te permite cambiar el case despues solo editando estos numeros.

3. **Modela el envelope exterior** (paso 2 de esta hoja):
   - Sketch base + sketch top + loft + fillets 3D
   - Resultado: 1 solido tapered con esquinas redondeadas y chamfers

4. **Hace shell** del envelope con grosor 2mm, abriendo solo en Z=4 (seam).
   Esto crea la cavidad interior con paredes uniformes.

5. **Corta en Z=4** con un plano horizontal para separar bottom y top.
   Cada uno queda en un component separado.

6. **Agrega los standoffs / pilares** como cilindros union'd a cada mitad.

7. **Cuts** (sensor cutouts, electrode holes, display window, USB-C, jack,
   buttons): usa sketch + extrude cut con las dimensiones de las tablas 4 y 5.

8. **Lugs**: sketch + extrude + hole, union con el top case.

9. **Export STL** desde cada component para imprimir.

### Tip para los caps de botones

Pueden ir como un sub-component aparte en el mismo archivo. Usalo
`File > Save as Mesh > STL` solo del component "cap" cuando quieras
imprimirlo.

---

## 10. Tolerancias y notas de impresion

- **Pared minima recomendada**: 2.0 mm (uniformement aplicada)
- **Capa para FDM**: 0.2 mm height para cuerpo, 0.15 para los caps
- **Infill**: 30% cuerpo principal, 80% caps de botones (resistencia)
- **Soportes**: solo bajo los lugs si los orientas horizontales. Si los
  imprimes verticales (case parado sobre la cara Z=0), no necesitas
  soportes en los lugs ni en los pilares.
- **Orientacion bottom case**: cara Z=2 (interior, plana) hacia abajo en
  la base de impresion. Asi el lado de la piel queda con mejor acabado.
- **Orientacion top case**: cara seam (Z=4 absoluto = Z=0 top-local)
  hacia abajo en la base. El display window queda como overhang en la
  parte superior — necesitaras soporte ahi (~3mm de overhang).

---

## 11. Tabla resumen para validacion rapida

Si replicas el case en Fusion y quieres verificar que esta correcto, mide:

| Verificacion | Valor esperado |
|---|---|
| Ancho exterior maximo (Z=0) | 98.0 |
| Ancho exterior maximo (Z=22) | 94.0 (= 98 - 2*taper) |
| Distancia entre lugs (inner-to-inner, mismo lado) | 22.0 |
| Distancia entre standoffs MH1-MH2 | 78.0 (= 88 - 10) |
| Distancia entre standoffs MH1-MH3 | 60.0 (= 69.5 - 9.5) |
| Distancia desde piso (Z=0) al centro del display | 17 (= 4 + 1.6 + 11.4 aprox) |
| Distancia desde piso al centro USB-C | 16.6 (= 4 + 1.6 + 11) |
| Volumen aproximado del case completo (sin componentes) | ~110 cm³ |

---

*Generado a partir de los archivos SCAD funcionales V2:*
- `supaclock_v2_bottom_case.scad`
- `supaclock_v2_top_case.scad`
- `supaclock_v2_button_caps.scad`

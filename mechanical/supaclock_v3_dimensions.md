# SupaClock V3 — Hoja de cotas

Análoga a `supaclock_v2_dimensions.md`. Cambios clave vs V2:

- Altura total reducida de **25 mm → 17 mm** (XIAO SMD soldado directo, sin pin sockets).
- Bottom case **al ras** (sin pocket de pila). Standoffs reducidos a la mitad (1 mm vs 2 mm).
- Pila Sony WF-1000XM5 case (prismática ~30 × 20 × 5 mm) **colgada del techo** con cinta 3M VHB doble faz (0.5 mm) sobre la zona del display.
- Top case con sistema **plug-and-play** para faceplate intercambiable (snap-fit ó magnetic).
- Botones laterales V2 (stem horizontal) **inalterados**: switch nuevo 3×7×3.5 mm side-press pegado plano sobre la PCB con super-glue gel, plunger apuntando a +X, cables soldados a pads SMD originales.
- Heat-set inserts M3 para tornillería desmontable (igual que V2: Ø4.2 × 5 mm).

Todas las medidas en milímetros.

---

## 1. Sistema de coordenadas

Idéntico a V2:

```
            Z (vertical, hacia arriba)
            |
            |
            +------ X (largo, eje izq-der del reloj puesto)
           /
          /
         Y (corto, eje proximal-distal en la muñeca)

Origen: esquina INFERIOR-IZQUIERDA-FRENTE del envelope exterior
```

- **Bottom case**: Z = 0 a Z = 3 (cara Z=0 toca la piel)
- **Seam (unión)**: plano horizontal en Z = 3
- **Top case**: Z = 3 a Z = 17 (en frame top-case-local: Z = 0 a 14)
- **Faceplate** (cosmético, intercambiable): Z = 17 a Z = 19 (sumando 2 mm de techo decorativo)

Conversión PCB-local ↔ case-local:
```
X_case = X_pcb + pcb_off_x = X_pcb + 6.5
Y_case = Y_pcb + pcb_off_y = Y_pcb + 6.0
```

---

## 2. Envelope exterior

| Parámetro | Valor | Notas |
|---|---|---|
| Ancho (X) | 98.0 mm | base. Top está INSET por `taper` |
| Largo (Y) | 79.0 mm | base. Top está INSET por `taper` |
| **Alto (Z) core** | **17.0 mm** | core sin faceplate (vs V2 25.0) |
| Alto con faceplate snap | 19.0 mm | +2 mm de skin decorativo |
| Alto con faceplate magnetic | 19.0 mm | idem |
| r_vert | 12.0 mm | radio de las 4 esquinas verticales |
| r_chamfer | 1.5 mm | chamfer en Z=0 y Z=15.5 |
| taper | 2.0 mm | reducción lineal radio entre Z=0 y Z=15.5 |

---

## 3. Bottom case — features (al ras)

| Feature | Posición (X, Y) | Z | Dimensiones | Notas |
|---|---|---|---|---|
| Piso | toda la base | 0 — 2 | 2 mm grosor | esquinas redondeadas |
| Cavidad | toda | 2 — 3 | abierta arriba | offset 2 mm desde envelope |
| Standoff MH1 | (10.0, 69.5) | 2 — 3 | Ø7 OD, Ø3.2 ID | **1 mm** (mitad de V2) |
| Standoff MH2 | (88.0, 69.5) | 2 — 3 | Ø7 OD, Ø3.2 ID | idem |
| Standoff MH3 | (10.0, 9.5) | 2 — 3 | Ø7 OD, Ø3.2 ID | idem |
| Standoff MH4 | (88.0, 9.5) | 2 — 3 | Ø7 OD, Ø3.2 ID | idem |
| Cutout MAX30102 | centro (52.0, 39.195) | 0 — 2 | **17 × 22 mm (X×Y)** | sensor PPG, rotado 90° vs V2 (lado largo en Y) para calzar orientación real del sensor en PCB v3 |
| Cutout MAX30205 | centro (51.5, 23.0) | 0 — 2 | 14 × 10 mm | sensor temp |
| Electrode RA | centro (18.5, 37.5) | 0 — 2 | Ø6 mm | M3 a piel |
| Electrode LA | centro (72.0, 38.0) | 0 — 2 | Ø6 mm | M3 a piel |
| Electrode RL | centro (50.0, 10.0) | 0 — 2 | Ø6 mm | M3 a piel |

**Sin pocket de pila** → cara contra la piel completamente plana.

---

## 4. Top case core — features

Frame local: Z=0 = seam, Z=14 = cara exterior del techo (cubierta por el faceplate).

| Feature | Posición (X, Y) | Z (local top) | Dimensiones | Notas |
|---|---|---|---|---|
| Cavidad | toda | 0 — 12 | abierta abajo | techo a Z=12, interior alcanza 12 |
| Techo | toda | 12 — 14 | 2 mm grosor | con ventana display |
| Pilar P1-P4 (4) | (10/88, 9.5/69.5) | 0 — 12 | Ø7 OD, Ø4.2 ID × 5 mm | heat insert M3 (insert depth 5) |
| **Ventana display** | centro (50.78, 44.5) | 10 — 14 | **28 × 34 mm** | atraviesa techo + faceplate alineado |
| Botón SW1 | (96, 21.375, btn_z=3.5) | 3.5 ± 2.1 | Ø4.2 mm | atraviesa pared +X, eje X (V2 idéntico) |
| Botón SW2 | (96, 33.875, btn_z=3.5) | 3.5 ± 2.1 | Ø4.2 mm | idem (V2 idéntico) |
| **USB-C** | (96, 54, usb_z=3.6) | 3.6 ± 3.5 | 13 × 7 mm rect | pared +X. **z_above_pcb reducido a 2** vs V2 (14) por XIAO SMD soldado |
| **Jack 3.5 ECG** | (0, 24.586, jack_z=6.1) | 6.1 ± 3.25 | Ø6.5 thread pasante + Ø12 rebaje (prof 1 mm en la pared) | pared **-X**. Mismo `jack_y_pcb` que V2; `z_above_pcb` bajado de 12.6 → 4.5. Rebaje **NO atraviesa** la pared (deja 1 mm interior con solo el thread). Conecta a J13 PCB v3 vía cables manuales |
| **Marca VHB pila** | centro (50.78, 44.5) | 12 (cara interior techo) | 30 × 20 mm rectángulo | indentación 0.3 mm de profundidad para guiar la cinta VHB |

> **Tabs L para la pila — DESHABILITADOS en V3.1.** La zona donde irían los tabs (X=35–67, Y=34–55) coincide con la huella del display + adapter PCB y puede colisionar. Hasta confirmar medidas reales del PCB+adapter, sólo dejamos la marca VHB. El código `battery_tab()` y `battery_tabs_all()` se preserva en el SCAD comentado para reactivar cuando se confirme el clearance.

### 4.1 Retention faceplate — V3.3 diagonal limpia

**Esquema diagonal SW-NE**: solo 2 imanes redondos en la diagonal SW-NE. Las otras 2 esquinas (SE-NW) **quedan limpias, sin agujeros**. Sin pines guía.

#### Modo MAGNETIC

| Parámetro | Valor |
|---|---|
| Imanes | **2 pares de N42 Ø7 × 1.5 mm** (4 total, diámetro = cabeza M4) |
| Posición imanes | esquinas **SW y NE** (diagonal), a 6 mm del borde |
| Hueco imán | Ø7.2 × 1.6 mm (holgura 0.2/0.1) |
| Otras 2 esquinas (SE, NW) | **sin features** (lisas) |
| Fuerza atracción por par | ~0.7 kg |
| Fuerza total ensamble | ~1.4 kg |
| Ciclos | infinitos |
| Alineación angular | los 2 imanes en diagonal restringen rotación; el ajuste perimetral del faceplate al borde del core complementa |
| Polaridad | N hacia arriba en core, S hacia abajo en face |
| Fijación imán | super-glue gel dentro del hueco |
| Tapa cosmética | 0.4 mm de PLA debajo del imán en core (techo 2 mm, hueco 1.6) |

#### Modo SNAP

| Parámetro | Valor PLA eSun PLA+ |
|---|---|
| Cantilevers | **4 idénticos** (faceplate macho, core hembra) |
| Cantilever largo × espesor | 5 × 1.4 mm |
| Hook engagement | 0.4 mm |
| Fillet base | 0.5 mm |
| Insertion / extraction | ~5 N / ~9 N |
| Ciclos | 30–40 |

---

## 5. Faceplate (skin intercambiable)

Pieza separada, snap o magnetic. Z=17 a Z=19.

| Feature | Posición | Dimensiones | Notas |
|---|---|---|---|
| Slab base | toda | 98 × 79 × 1.5 mm | sigue contorno del top exterior (radio 10 mm = r_vert − taper) |
| Ventana display | centro (50.78, 44.5) | 28 × 34 mm (alineada con techo core) | atraviesa todo el faceplate |
| Bezel display | anillo alrededor de la ventana | 32 × 38 mm exterior, 28 × 34 interior | recess 0.3 mm para "vidrio" cosmético |
| Marcas horarias / decoración | según `style` | grabadas 0.4 mm de profundidad | mode-dependiente |
| Retention macho (snap) | 4 cantilevers internos | 5 mm largo × 1.4 mm espesor | cuando `retention=snap` |
| Cavidades imanes (magnetic) | 4 huecos esquinas | Ø5.2 × 2.1 mm | cuando `retention=magnetic` |
| Pin guía | 2 cilindros Ø2 mm | sale de la cara inferior 3 mm | siempre presente |

### 5.1 Variantes de estilo

- **`style="minimal"`**: PLA Black, sin decoración, sólo bezel display.
- **`style="classic"`**: bezel display + 12 marcas horarias (Ø1 mm cada una) a 16 mm del centro, grabadas 0.4 mm. Color sugerido: PLA Silver/Grey.
- **`style="sport"`**: bezel display + numerales 5/15/25/35/45/55 en relieve negativo a 18 mm del centro. Color sugerido: PLA Orange.

---

## 6. Pila colgada del techo

Pila Sony WF-1000XM5 case (~30 × 20 × 5 mm, LiPo prismática) **con conector JST PH 2.0 mm** previamente soldado.

| Item | Valor |
|---|---|
| Posición de la pila (cara interior techo) | centrada en (50.78, 44.5) en case-local |
| Footprint | 30 × 20 mm |
| Altura pila | 5 mm |
| Adhesivo | cinta 3M VHB 5952 (0.5 mm espesor) doble faz |
| Marca en el techo | indentación rectangular 0.3 mm de profundidad para guiar el VHB |
| Salida cable JST | lado **-X** de la pila → recorrido ~25 mm hasta MAX17048 en (~17, 50) PCB-local |
| Gap pila ↔ display | ~1.5 mm (pila a Z=7..12, display a Z=1.6..5.6 en top-local) |

**Importante**: NO pegar con cyanoacrylate sobre la pila (riesgo térmico). VHB de doble faz es la opción correcta para baterías LiPo.

---

## 7. Stack vertical detallado

```
Z = 17.0  ── cara exterior FACEPLATE ──
Z = 15.0     base FACEPLATE (1.5 mm slab + 0.5 retention)
Z = 15.0  ── cara exterior TECHO core ──
Z = 13.0     espesor techo
Z = 13.0  ── cara interior techo ──
Z = 12.5     VHB 3M (0.5 mm)
Z = 12.5  ── cara superior pila ──
Z = 7.5      pila (5 mm)
Z = 7.5   ── cara inferior pila ──
Z = 6.0      gap pila ↔ display (1.5 mm)
Z = 6.0   ── cara superior display ──
Z = 4.6      display + adapter (1.6 + 2.4)
Z = 4.6   ── cara superior PCB ──
Z = 3.0      PCB FR-4 (1.6 mm)
Z = 3.0   ── cara inferior PCB / seam ──
Z = 2.0      standoff (1 mm, mitad de V2)
Z = 2.0   ── cara interior bottom ──
Z = 0.0      piso bottom (2 mm)
Z = 0.0   ── cara contra PIEL ──
```

(Heights en frame **abs**, no top-local).

---

## 8. Standard parts (compra fuera)

| Parte | Especificación | Cant | Notas |
|---|---|---|---|
| Perno M3 | M3 × 6 mm cabeza redondeada | 4 | bottom → top via heat insert |
| Heat insert M3 | M3 × 5 mm OD Ø4.0–4.2 | 4 | embebidos en pilares top |
| Spring bar | Ø1.5 × largo 22 mm | 4 | tienda relojes |
| Correa | 20 mm cualquier estilo | 1 | tienda relojes |
| Perno M3 (electrodo) | M3 × 8 mm acero inox cabeza redonda | 3 | electrodos ECG |
| Tuerca M3 (electrodo) | M3 estándar | 3 | electrodos ECG |
| Imán neodimio N42 | Ø5 × 2 mm | 4 | sólo si `retention=magnetic` |
| Cinta VHB 3M 5952 | 0.5 mm doble faz | 30 × 20 mm | pila colgada |
| Switch tactile | 3 × 7 × 3.5 mm side-press SMD | 2 | el que ya tenés |
| Cable 30 AWG | flexible silicone | 4× 30 mm | soldar switch nuevo a pads SMD |
| Super-glue gel | cyanoacrylate gel | 1 frasco | pegar switches PLANOS sobre PCB |

---

## 9. Validación rápida

| Verificación | Valor esperado |
|---|---|
| Ancho exterior máximo (Z=0) | 98.0 |
| Ancho exterior máximo (Z=15) | 94.4 (= 98 − 2 × taper × 15/17) |
| H_total core | 17.0 |
| H_total con faceplate | 19.0 |
| Distancia entre lugs (inner mismo lado) | 20.0 |
| Distancia entre standoffs MH1-MH2 | 78.0 |
| Altura interior arriba del PCB | 10.4 |
| Distancia desde piel (Z=0) al centro del display | 6.5 |
| Distancia desde piel al centro USB-C | 4.6 (= 3 + 1.6) |
| Volumen aproximado del case completo | ~75 cm³ |

---

*Archivos relacionados:*
- `supaclock_v3_bottom_case.scad`
- `supaclock_v3_top_case.scad`
- `supaclock_v3_faceplate.scad`
- `supaclock_v3_button_caps.scad` (reuse del v2)
- `supaclock_v3_assembly.scad`

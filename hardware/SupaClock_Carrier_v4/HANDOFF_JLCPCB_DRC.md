# Handoff — Ajuste DRC y vías para JLCPCB (SupaClock Carrier v3)

## Contexto

- Proyecto: `hardware/SupaClock_Carrier_v3/SupaClock_Carrier.kicad_pro`
- PCB 2 capas, ya ruteada (XIAO ESP32-S3 SMD + filtro +BATT directo al pad 32, sin J15).
- DRC actual venía heredado de v2, configurado originalmente para una fresadora **LPKF** (de ahí los valores conservadores: `min_via_diameter=1.8 mm`, `min_track_width=0.4`, etc.).
- Ahora el destino de fabricación cambia a **JLCPCB**, que admite reglas mucho más finas. Queremos relajar el DRC a los mínimos de JLCPCB y reducir las vías para aprovechar la densidad disponible.

**Importante:** KiCad estaba abierto al cerrar la sesión anterior (lock file presente). Antes de tocar `.kicad_pro` o `.kicad_pcb`, confirmar que KiCad está cerrado:
```bash
pgrep -a kicad
ls hardware/SupaClock_Carrier_v3/~*.lck 2>/dev/null
```

## Estado actual (a cambiar)

Leído de `SupaClock_Carrier.kicad_pro` → `board.design_settings.rules`:

| Regla | Valor actual (LPKF) | Target JLCPCB |
|---|---|---|
| `min_clearance` | 0.3 | **0.127** |
| `min_track_width` | 0.4 | **0.127** |
| `min_via_diameter` | 1.8 | **0.45** |
| `min_via_annular_width` | 0.4 | **0.13** |
| `min_through_hole_diameter` | 0.8 | **0.3** |
| `min_hole_to_hole` | 0.8 | **0.5** |
| `min_hole_clearance` | 0.3 | **0.25** |
| `min_copper_edge_clearance` | 0.5 | **0.2** |
| `min_silk_clearance` | 0.15 | **0.15** (mantener) |
| `min_text_height` | 0.8 | **1.0** (JLCPCB pide ≥1 mm legible) |
| `min_text_thickness` | 0.15 | **0.15** |
| `min_microvia_diameter` | 0.5 | (no aplica, son blind/buried; no se usan) |

Net classes actuales (`net_settings.classes`):

| Clase | clearance | track_width | via_dia | via_drill |
|---|---|---|---|---|
| Default | 0.3 | 0.4 | 1.8 | 1.0 |
| Analog | 0.3 | 0.4 | 2.0 | 0.8 |
| Power | 0.3 | 0.8 | 2.0 | 0.8 |
| Power_HI | 0.3 | 1.0 | 2.0 | 0.8 |
| Signal_HF | 0.3 | 0.4 | 2.0 | 0.8 |

## Target JLCPCB (2 capas, 1 oz Cu, proceso estándar sin upcharges)

Valores tomados de la sección "PCB Capabilities" pública de JLCPCB (verificar siempre la versión vigente en jlcpcb.com antes de fabricar):

- Min trace / spacing: **0.127 mm** (5 mil). Usamos 0.15 mm como default para holgura.
- Min via: outer **0.45 mm** / drill **0.20 mm**. Estándar sin cargo: 0.6/0.3 mm.
- Min PTH drill: **0.3 mm**.
- Min annular ring: **0.13 mm** (5 mil).
- Min hole-to-hole: **0.5 mm** (placas <100 cm²).
- Min board edge clearance (copper a outline): **0.2 mm**.
- Min silk text: altura **1 mm**, ancho **0.15 mm**.

### Tamaños propuestos de vías y trazas por net class

| Clase | track_width | clearance | via_dia | via_drill | Uso |
|---|---|---|---|---|---|
| Default | 0.2 | 0.15 | 0.6 | 0.3 | Señales genéricas |
| Analog | 0.2 | 0.2 | 0.6 | 0.3 | ECG, MAX30205, MAX30102 |
| Power | 0.4 | 0.2 | 0.8 | 0.4 | +3V3 (corriente moderada) |
| Power_HI | 0.6 | 0.2 | 0.8 | 0.4 | +BATT, +5V/VBUS |
| Signal_HF | 0.2 | 0.2 | 0.6 | 0.3 | SPI LCD, USB D+/D- |

(Si después el ruteo no cabe con 0.2 mm en algún pasaje entre pads SMD del XIAO, bajar a 0.15 mm en esa pista puntualmente o usar via 0.45/0.2 mm en Default — sigue cumpliendo JLCPCB.)

## Cambios a aplicar

### 1. `SupaClock_Carrier.kicad_pro` (JSON)

Editar `board.design_settings.rules` con los valores target arriba. También editar cada entrada de `net_settings.classes` con la tabla de net classes propuesta. Snippet referencia de patch (no tomar literalmente — leer el JSON, mutarlo, re-escribir manteniendo el resto intacto):

```python
import json
p = 'hardware/SupaClock_Carrier_v3/SupaClock_Carrier.kicad_pro'
with open(p) as f: d = json.load(f)

rules = d['board']['design_settings']['rules']
rules.update({
    'min_clearance': 0.127,
    'min_track_width': 0.127,
    'min_via_diameter': 0.45,
    'min_via_annular_width': 0.13,
    'min_through_hole_diameter': 0.3,
    'min_hole_to_hole': 0.5,
    'min_hole_clearance': 0.25,
    'min_copper_edge_clearance': 0.2,
    'min_text_height': 1.0,
})

classes_target = {
    'Default':   {'clearance': 0.15, 'track_width': 0.2, 'via_diameter': 0.6, 'via_drill': 0.3},
    'Analog':    {'clearance': 0.2,  'track_width': 0.2, 'via_diameter': 0.6, 'via_drill': 0.3},
    'Power':     {'clearance': 0.2,  'track_width': 0.4, 'via_diameter': 0.8, 'via_drill': 0.4},
    'Power_HI':  {'clearance': 0.2,  'track_width': 0.6, 'via_diameter': 0.8, 'via_drill': 0.4},
    'Signal_HF': {'clearance': 0.2,  'track_width': 0.2, 'via_diameter': 0.6, 'via_drill': 0.3},
}
for c in d['net_settings']['classes']:
    if c['name'] in classes_target:
        c.update(classes_target[c['name']])

with open(p, 'w') as f: json.dump(d, f, indent=2)
```

### 2. `SupaClock_Carrier.kicad_pcb` (S-expressions)

KiCad replica las design rules en el header del `.kicad_pcb` (sección `(setup ...)`). Buscar y actualizar los mismos campos:

- `(pad_to_mask_clearance ...)` — dejar como está
- `(solder_mask_min_width ...)` — dejar como está
- En `(net_class "Default" ...)` aparecen `(clearance ...)`, `(trace_width ...)`, `(via_dia ...)`, `(via_drill ...)` — actualizar a los valores nuevos.

Si KiCad está cerrado al modificar el `.kicad_pro`, normalmente al reabrir el proyecto KiCad **propaga las design rules al PCB automáticamente** (te mostrará el "Board Setup → Constraints" actualizado). Pero los tamaños default de via/trace en `.kicad_pcb` no siempre se actualizan solos — abrir Board Setup → Net Classes y confirmar que se aplican; si no, editar el `.kicad_pcb` con los mismos valores que en `.kicad_pro`.

### 3. Resizing de vías existentes (acción manual en KiCad UI)

Cambiar las design rules **no encoge automáticamente** las vías ya colocadas (que están a 1.8/1.0 mm de la era LPKF). Para aprovechar el espacio:

1. En Pcbnew: `Edit → Find` → tipo "Via", o filtrar por `(via)` con el Inspector.
2. Mejor: **`Edit → Change Track Width`** o seleccionar todas las vias (clic derecho sobre una → Select → All Tracks/Vias in Net Class), luego `Properties` → asignar el tamaño nuevo de la net class.
3. Alternativa programática: regex sobre `.kicad_pcb` para reescribir `(via (at ...) (size 1.8) (drill 1.0) ...)` → `(size 0.6) (drill 0.3)` para Default. **Antes de hacerlo**: backup del .kicad_pcb. Y considerar que `Power`/`Power_HI` quieren via 0.8/0.4.

Esto último requiere identificar a qué net class pertenece cada via para asignar el tamaño correcto, lo cual está en `(net N "/nombre")` dentro de cada bloque `(via ...)`. Si es muy tedioso, dejar que el usuario lo haga visualmente en KiCad seleccionando por net class.

### 4. Recomendaciones adicionales JLCPCB

- **Tenting de vías**: JLCPCB hace tenting (vias cubiertas con soldermask) por defecto. Si tienes alguna via que quieres descubierta para test/medida, marcarla explícitamente en KiCad.
- **Fiducials**: si la PCB va a JLCPCB SMT Assembly, agregar 3 fiduciales no colineales (1 mm copper + 3 mm mask opening). No es necesario si solo es PCB sin assembly.
- **Nombre/revision en silkscreen**: incluir `SupaClock Carrier v3` y fecha/orden en F.Silk o B.Silk.
- **Outline**: revisar que `Edge.Cuts` sea un loop cerrado único (sin gaps ni overlaps), y que el silkscreen no salga del board edge (clearance ≥ 0.2 mm).
- **Drill table**: que KiCad genere drill files en formato Excellon 2:4 mm para JLCPCB.

## Verificación post-cambio

1. Cerrar KiCad si está abierto.
2. Aplicar cambios al `.kicad_pro` (y `.kicad_pcb` si hace falta).
3. Abrir el proyecto en KiCad. Si pide migración o muestra warnings, anotar y resolver antes de fabricar.
4. `Board Setup → Constraints`: confirmar valores de la tabla "Target JLCPCB" arriba.
5. `Board Setup → Net Classes`: confirmar tabla de net classes propuesta.
6. Reducir vías existentes (paso 3 arriba).
7. `Inspect → Design Rules Checker` → "Run DRC". No debe quedar ningún error de tipo:
   - Clearance violation
   - Track/via too small
   - Hole-to-hole too close
   - Copper-to-edge clearance
8. Exportar gerbers con JLCPCB profile estándar de KiCad (`File → Fabrication Outputs → Gerbers...`). Layers a incluir: `F.Cu B.Cu F.Paste B.Paste F.Silk B.Silk F.Mask B.Mask Edge.Cuts`. Drill: Excellon, formato decimal, plated/non-plated en un mismo archivo (JLCPCB acepta ambos).
9. Subir el zip a la calculadora de JLCPCB para que verifique automaticamente (esto cataloga warnings reales del fabricante).

## Archivos clave

- `hardware/SupaClock_Carrier_v3/SupaClock_Carrier.kicad_pro` — design rules + net classes (JSON)
- `hardware/SupaClock_Carrier_v3/SupaClock_Carrier.kicad_pcb` — vías y traces existentes + duplicado de design rules
- `hardware/SupaClock_Carrier_v3/footprints/SupaClock_Custom.pretty/` — footprints locales (incl. XIAO-ESP32-S3-SMD con pad 32=BAT+)
- `hardware/SupaClock_Carrier_v3/apply_lpkf_rules.py`, `fix_pads_lpkf.py` — scripts heredados, **NO ejecutar** (revertirían el cambio).

## Riesgos / cosas a vigilar

- Si reduces vías 1.8→0.6 mm en pads que tenían thermals dimensionados para vía grande, el cobre fundido del thermal puede verse afectado en simulación de fabricación. Verificar visualmente cada zona de power/GND tras el resize.
- Vías existentes pueden estar colocadas asumiendo holgura LPKF. Tras reducirlas y bajar el clearance, KiCad puede mostrar nuevos warnings de "track to via demasiado cerca" que antes no se veían. Re-rutear esos puntos puntuales.
- Si `apply_lpkf_rules.py` se corrió alguna vez sobre v3, los pads pudieron quedar oversized. Verificar tamaños de pads críticos (XIAO SMD pads 2.75×2.0 mm originales — confirmar que no se inflaron).
- El footprint local `XIAO-ESP32-S3-SMD.kicad_mod` tiene pads de programación bottom-side renumerados arbitrariamente (15→24, 16→26, 17→27, 18→28, 19→29, 20→30, 21→31, 22→34). Si el usuario decide después conectar uno de esos pads a una señal real, el mapeo señal-pad puede no corresponder con el silkscreen físico del módulo Seeed — verificar contra la datasheet antes de rutear.

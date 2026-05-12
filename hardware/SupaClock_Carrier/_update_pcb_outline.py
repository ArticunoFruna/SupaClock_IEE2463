#!/usr/bin/env python3
"""Actualiza el outline del PCB de 50x40 -> 67x85 y agrega 4 holes M2 en
las esquinas. Modificacion surgicaal: NO toca los footprints existentes.

Origen del carrier (top-left) se mantiene en PCB-absolute (46, 36).
"""
import re
from pathlib import Path

PCB = Path(__file__).parent / "SupaClock_Carrier.kicad_pcb"

# Carrier origin en coords absolutas del PCB
OX, OY = 46.0, 36.0
W, H = 67.0, 85.0

# M2 holes en coords carrier-local (4mm de los bordes)
M2_HOLES = [(4, 4), (W - 4, 4), (4, H - 4), (W - 4, H - 4)]

src = PCB.read_text()

# ===========================================================
# 1) Reemplazar las 4 gr_line del outline antiguo (uuids 0001..0004)
# ===========================================================
new_outline = f"""	(gr_line
		(start {OX} {OY})
		(end {OX + W} {OY})
		(stroke
			(width 0.1)
			(type default)
		)
		(layer "Edge.Cuts")
		(uuid "10000000-0000-0000-0000-000000000001")
	)
	(gr_line
		(start {OX + W} {OY})
		(end {OX + W} {OY + H})
		(stroke
			(width 0.1)
			(type default)
		)
		(layer "Edge.Cuts")
		(uuid "10000000-0000-0000-0000-000000000002")
	)
	(gr_line
		(start {OX + W} {OY + H})
		(end {OX} {OY + H})
		(stroke
			(width 0.1)
			(type default)
		)
		(layer "Edge.Cuts")
		(uuid "10000000-0000-0000-0000-000000000003")
	)
	(gr_line
		(start {OX} {OY + H})
		(end {OX} {OY})
		(stroke
			(width 0.1)
			(type default)
		)
		(layer "Edge.Cuts")
		(uuid "10000000-0000-0000-0000-000000000004")
	)"""

# Match each of the 4 gr_line entries by their UUID and replace ALL with new outline
# Strategy: find first uuid 0001 line in gr_line context, walk backwards to find
# the opening "(gr_line", then forward through all 4 outlines to ")" after uuid 0004.
def replace_outline(text):
    # Find by uuids - they're unique. Find the block containing all 4.
    uuid_pattern = re.compile(
        r'\(gr_line\s*\n[^)]*\(uuid "10000000-0000-0000-0000-00000000000[1234]"\)\s*\)',
        re.MULTILINE
    )
    matches = list(uuid_pattern.finditer(text))
    if len(matches) != 4:
        # Try simpler: find each uuid line and walk back to (gr_line
        # Just find spans of (gr_line ... uuid 0001/0002/0003/0004 ... )
        # Use a non-greedy multi-line match
        full_pat = re.compile(
            r'\t\(gr_line.*?\(uuid "10000000-0000-0000-0000-00000000000[1-4]"\)\s*\)',
            re.DOTALL
        )
        matches = list(full_pat.finditer(text))
        if not matches:
            raise SystemExit("No se encontraron las gr_line del outline antiguo")
    # Replace each in order, keeping span boundaries; replace the first with
    # the entire new_outline, and erase the others.
    # Iterate in reverse to preserve indices
    matches.sort(key=lambda m: m.start())
    # Build new text by stitching pieces
    out = []
    last = 0
    for i, m in enumerate(matches):
        out.append(text[last:m.start()])
        if i == 0:
            out.append(new_outline)
        # else: drop the old line (don't append anything)
        last = m.end()
    out.append(text[last:])
    return "".join(out)

src = replace_outline(src)

# ===========================================================
# 2) Insertar 4 footprints de M2 mounting hole antes del gr_line del outline
# ===========================================================
def m2_footprint(idx, x, y):
    uuid_base = f"10000000-0000-0000-0000-00000000{idx:04d}"
    return f"""	(footprint "MountingHole:MountingHole_2.2mm_M2"
		(layer "F.Cu")
		(uuid "{uuid_base}00")
		(at {OX + x} {OY + y})
		(descr "Mounting Hole 2.2mm, no annular, M2")
		(tags "mounting hole 2.2mm no annular m2")
		(property "Reference" "MH{idx}"
			(at 0 -3.5 0)
			(unlocked yes)
			(layer "F.SilkS")
			(uuid "{uuid_base}01")
			(effects (font (size 1 1) (thickness 0.15)))
		)
		(property "Value" "M2_Mount"
			(at 0 3.5 0)
			(unlocked yes)
			(layer "F.Fab")
			(uuid "{uuid_base}02")
			(effects (font (size 1 1) (thickness 0.15)))
		)
		(property "Footprint" ""
			(at 0 0 0)
			(unlocked yes)
			(layer "F.Fab")
			(hide yes)
			(uuid "{uuid_base}03")
			(effects (font (size 1.27 1.27) (thickness 0.15)))
		)
		(property "Datasheet" ""
			(at 0 0 0)
			(unlocked yes)
			(layer "F.Fab")
			(hide yes)
			(uuid "{uuid_base}04")
			(effects (font (size 1.27 1.27) (thickness 0.15)))
		)
		(property "Description" "Mounting Hole 2.2mm, no annular, M2"
			(at 0 0 0)
			(unlocked yes)
			(layer "F.Fab")
			(hide yes)
			(uuid "{uuid_base}05")
			(effects (font (size 1.27 1.27) (thickness 0.15)))
		)
		(attr exclude_from_pos_files exclude_from_bom)
		(fp_circle
			(center 0 0)
			(end 1.45 0)
			(stroke (width 0.05) (type solid))
			(fill none)
			(layer "F.CrtYd")
			(uuid "{uuid_base}10")
		)
		(pad "" np_thru_hole circle
			(at 0 0)
			(size 2.2 2.2)
			(drill 2.2)
			(layers "F&B.Cu" "*.Mask")
			(uuid "{uuid_base}20")
		)
	)
"""

mh_blocks = "".join(m2_footprint(i + 1, x, y) for i, (x, y) in enumerate(M2_HOLES))

# Insertar antes del primer (gr_line del outline nuevo
insert_pos = src.find("\t(gr_line\n\t\t(start ")
if insert_pos < 0:
    raise SystemExit("No se encontro el inicio del bloque outline para insertar M2 holes")
src = src[:insert_pos] + mh_blocks + src[insert_pos:]

# ===========================================================
# 3) Actualizar silk text con nuevas dimensiones
# ===========================================================
src = re.sub(
    r'\(gr_text "SupaClock v1 - Carrier"[^)]*\(at [^)]+\)',
    f'(gr_text "SupaClock v1 - Carrier 67x85mm"\n\t\t(at {OX + W/2} {OY - 3} 0)',
    src,
)

# ===========================================================
# 4) Title block update
# ===========================================================
src = re.sub(
    r'\(title "SupaClock Carrier v1 - PCB"\)',
    '(title "SupaClock Carrier v1 - PCB 67x85mm")',
    src,
)
src = re.sub(
    r'\(comment 1 "Wearable Biometrico Modular - Carrier through-hole 50x40mm"\)',
    '(comment 1 "Wearable Biometrico Modular - Carrier through-hole 67x85mm")',
    src,
)

PCB.write_text(src)
print(f"PCB actualizado: outline {W}x{H}mm + 4 M2 holes en esquinas")
print(f"  Origen carrier (top-left): ({OX}, {OY})")
print(f"  M2 holes: " + ", ".join(f"({OX+x:.1f}, {OY+y:.1f})" for x, y in M2_HOLES))

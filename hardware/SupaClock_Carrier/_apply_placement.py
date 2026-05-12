#!/usr/bin/env python3
"""Auto-placement de footprints en el carrier 67x85mm.

Asume que el usuario hizo F8 (Update PCB from Schematic) y todos los 27
footprints estan en el PCB. Este script los MUEVE a su posicion final.

Coordenadas carrier-local: (0,0) = top-left de la PCB; (67,85) = bottom-right.
Origen carrier en PCB-absolute: (46, 36).

MAX30102 (J3, J14) y MAX30205 (J4) van en BACK SIDE (B.Cu) -- contacto piel.
Resto en TOP SIDE (F.Cu).
"""
import re
from pathlib import Path

PCB = Path(__file__).parent / "SupaClock_Carrier.kicad_pcb"

# Origen carrier en PCB absoluto
OX, OY = 46.0, 36.0

# Placement: ref -> (carrier_x, carrier_y, rotation, layer)
# IMPORTANTE: PinSocket_1x0N_P2.54mm_Vertical tiene pad 1 en (0,0) y pad N en
# (0, (N-1)*2.54). Es decir, el "pin row natural" es VERTICAL hacia +Y.
# Para hacerlo horizontal: rot 270 (pad 1 izq, pad N der).
# Para rotarlo 180 grados: rot 180 (pad 1 abajo, pad N arriba).
# El (at X Y) referencia el ORIGEN del footprint = pad 1.
PLACEMENT = {
    # TOP SIDE: MCU + modulos con pin socket + componentes SMD
    "U1":     (12.0,  40.0,   90, "F.Cu"),     # XIAO ESP32-S3 DIP, rot 90
    # AD8232 horizontal header (JP3 en Eagle): pad 1 a la izq, pad 6 a la der
    "J11":    (27.15, 26.67, 270, "F.Cu"),     # AD8232 6-pin control
    # AD8232 vertical header RA/LA/RL: pad 1 abajo (RA), pad 3 arriba (RL)
    "J13":    (16.99, 12.7,  180, "F.Cu"),     # AD8232 3-pin RA/LA/RL
    # Display socket horizontal: pad 1 izq, pad 8 der
    "J7":     (24.61, 35.33, 270, "F.Cu"),     # ST7789 8-pin socket
    # BMI160 horizontal pin row: pad 1 izq (X=1.38), pad 7 der (X=16.62)
    "J5":     (1.38,  55.0,  270, "F.Cu"),     # BMI160 7-pin
    # MAX17048 (Eagle confirmado): JP4 vertical izq, JP2 vertical der, JP5 horizontal
    # KiCad re-anota J6a -> J6a1 (regla de annotation por sufijo no-numerico)
    "J6a1":   (54.7,  46.97,   0, "F.Cu"),     # MAX17048 left vertical (VCC top, ALT bottom)
    "J6b1":   (61.175, 46.97,  0, "F.Cu"),     # MAX17048 right vertical (SDA top, QST bottom)
    "J6c1":   (58.0,  44.43, 270, "F.Cu"),     # MAX17048 battery horizontal (+ left, - right)
    "J15":    (10.73, 73.0,  270, "F.Cu"),     # XIAO BAT+/BAT- wires (horizontal, +BATT izq)
    "SW1":    (62.0,  38.0,   0,  "F.Cu"),     # Boton SELECT (arriba derecha)
    "SW2":    (62.0,  65.0,   0,  "F.Cu"),     # Boton NEXT (abajo derecha)
    "J12_RA1": (63.0, 51.5,   0,  "F.Cu"),     # M3 entre botones (electrodo RA)
    "J12_LA1": (22.0, 78.0,   0,  "F.Cu"),     # M3 abajo izquierda (electrodo LA)
    "J12_RL1": (45.0, 78.0,   0,  "F.Cu"),     # M3 abajo derecha (electrodo RL)
    # SMD passives reorganizados sin overlap (5mm separacion entre 0805)
    "R5":     (28.0,  27.0,   0,  "F.Cu"),     # POR R 10k (sobre J7, izq)
    "C6":     (33.0,  27.0,   0,  "F.Cu"),     # POR C 100nF (sobre J7)
    "C3":     (38.0,  27.0,   0,  "F.Cu"),     # Decoupling +3V3 100nF (sobre J7, der)
    "R1":     (45.0,  40.0,   0,  "F.Cu"),     # Pull-up I2C SDA 4.7k (a la derecha de J7)
    "R2":     (45.0,  43.0,   0,  "F.Cu"),     # Pull-up I2C SCL 4.7k (a la derecha de J7)
    "R3":     (58.0,  41.0,   0,  "F.Cu"),     # Pull-up SW1 10k (DNP)
    "R4":     (58.0,  62.0,   0,  "F.Cu"),     # Pull-up SW2 10k (DNP)
    "C2":     (18.0,  75.0,   0,  "F.Cu"),     # Bulk +BATT 10uF (cerca de J15)
    "C4":     (60.0,  41.0,   0,  "F.Cu"),     # SW1 debounce 100nF (DNP)
    "C5":     (60.0,  62.0,   0,  "F.Cu"),     # SW2 debounce 100nF (DNP)
    # BACK SIDE: sensores en contacto con piel
    "J3":     (33.5,  45.0,   0,  "B.Cu"),     # MAX30102 castellated top (GND/RD/IRD/INT)
    "J14":    (33.5,  66.0, 180,  "B.Cu"),     # MAX30102 castellated bot (VIN/SDA/SCL/GND)
    "J4":     (33.5,  78.0,   0,  "B.Cu"),     # MAX30205 8-pad flush
}


def move_footprint(pcb_text, ref, target_x_local, target_y_local, target_rot, target_layer):
    """Encuentra el footprint por reference y actualiza posicion+rotacion+capa.

    El bloque de footprint en KiCad es:
      (footprint "..."
          (layer "F.Cu")          <-- pos 1 layer
          ...
          (at X Y [ROT])          <-- pos 2 placement
          ...
          (property "Reference" "REF" ...
          ...
      )

    Se busca el bloque que contenga la propiedad Reference == ref, y se ajustan
    sus dos campos (layer ...) (al inicio del bloque) y (at X Y ROT).
    """
    # Convertir carrier-local a PCB absoluto
    target_x = OX + target_x_local
    target_y = OY + target_y_local

    # Buscar el bloque (footprint ...) que contiene Reference "ref"
    # Usar un escaneo manual con balance de parentesis
    idx = 0
    while True:
        m = re.search(r'\(footprint "[^"]+"\n', pcb_text[idx:])
        if not m:
            break
        start = idx + m.start()
        # Encontrar el cierre del bloque footprint mediante balance de parentesis
        depth = 0
        i = start
        while i < len(pcb_text):
            c = pcb_text[i]
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0:
                    end = i + 1
                    break
            i += 1
        else:
            raise SystemExit(f"Bloque footprint sin cierre desde pos {start}")
        block = pcb_text[start:end]
        # Es este el footprint con la referencia que buscamos?
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        if ref_m and ref_m.group(1) == ref:
            # Detectar layer actual del footprint
            cur_layer_m = re.search(r'\(footprint "[^"]+"\s*\n\s*\(layer "([^"]+)"', block)
            cur_layer = cur_layer_m.group(1) if cur_layer_m else "F.Cu"
            # Actualizar (layer "...") justo despues de (footprint "..."
            new_block = re.sub(
                r'(\(footprint "[^"]+"\s*\n\s*\(layer )"[^"]+"',
                lambda m: m.group(1) + f'"{target_layer}"',
                block,
                count=1,
            )
            # Detectar el layer de la PRIMERA pad para saber si necesitamos flip.
            # (cur_layer del footprint puede estar ya a B.Cu pero los pads pueden
            # haber quedado en F.* si no se flipea via GUI)
            first_pad_layer_m = re.search(r'\(layers "([^"]+)"', new_block)
            actual_pad_side = "F" if first_pad_layer_m and first_pad_layer_m.group(1).startswith("F.") else "B"
            target_pad_side = "B" if target_layer == "B.Cu" else "F"
            if actual_pad_side != target_pad_side:
                if target_pad_side == "B":
                    swap = [("F.Cu", "B.Cu"), ("F.Paste", "B.Paste"), ("F.Mask", "B.Mask"),
                            ("F.SilkS", "B.SilkS"), ("F.Fab", "B.Fab"), ("F.CrtYd", "B.CrtYd")]
                else:
                    swap = [("B.Cu", "F.Cu"), ("B.Paste", "F.Paste"), ("B.Mask", "F.Mask"),
                            ("B.SilkS", "F.SilkS"), ("B.Fab", "F.Fab"), ("B.CrtYd", "F.CrtYd")]
                # Saltar el header del footprint (no re-tocar el (layer ...) del footprint)
                header_end = re.search(r'\(layer "[^"]+"\)', new_block).end()
                body = new_block[header_end:]
                for a, b in swap:
                    body = body.replace(f'"{a}"', f'"__TMP__{b}"')
                body = body.replace("__TMP__", "")
                new_block = new_block[:header_end] + body
            # Actualizar (at X Y [ROT])  - puede no traer rotacion si rot=0
            # Asumimos que el primer (at ...) DESPUES del (layer ...) y antes de
            # (property ... es la posicion del footprint.
            # Construir nueva linea
            new_at = f"(at {target_x} {target_y} {target_rot})"
            new_block = re.sub(
                r'\(at -?\d+\.?\d*\s+-?\d+\.?\d*(?:\s+-?\d+\.?\d*)?\)',
                new_at,
                new_block,
                count=1,
            )
            pcb_text = pcb_text[:start] + new_block + pcb_text[end:]
            return pcb_text, True
        idx = end
    return pcb_text, False


OBSOLETE_REFS = [
    "J12",   # 3-pin viejo (reemplazado por J12_RA1/LA1/RL1)
    "J6",    # 4-pin viejo (reemplazado por J6a1/b1/c1)
    "J8",    # TP4056 (reemplazado por XIAO BMS interno via J15)
    "J9",    # Battery JST (la bateria va al JST onboard del MAX17048)
]


def remove_footprint(pcb_text, ref):
    """Elimina del PCB el bloque (footprint ...) con esta Reference."""
    idx = 0
    while True:
        m = re.search(r'\(footprint "[^"]+"\n', pcb_text[idx:])
        if not m:
            return pcb_text, False
        start = idx + m.start()
        depth = 0
        i = start
        while i < len(pcb_text):
            c = pcb_text[i]
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0:
                    end = i + 1
                    break
            i += 1
        block = pcb_text[start:end]
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        if ref_m and ref_m.group(1) == ref:
            # Eliminar el bloque + el newline/whitespace que lo precede si aplica
            before_start = start
            while before_start > 0 and pcb_text[before_start - 1] in "\t":
                before_start -= 1
            # Tambien eliminar el newline anterior si quedo solo
            return pcb_text[:before_start].rstrip() + "\n" + pcb_text[end:].lstrip("\n"), True
        idx = end


def main():
    src = PCB.read_text()

    # Paso 1: eliminar refs obsoletos
    removed = []
    for ref in OBSOLETE_REFS:
        new_src, ok = remove_footprint(src, ref)
        if ok:
            src = new_src
            removed.append(ref)

    # Paso 2: placement
    found = []
    missing = []
    for ref, (cx, cy, rot, layer) in PLACEMENT.items():
        new_src, ok = move_footprint(src, ref, cx, cy, rot, layer)
        if ok:
            src = new_src
            found.append(ref)
        else:
            missing.append(ref)

    PCB.write_text(src)
    if removed:
        print(f"Footprints obsoletos removidos: {removed}")
    print(f"Placement aplicado a {len(found)}/{len(PLACEMENT)} footprints")
    if missing:
        print(f"NO encontrados (haz F8 primero): {missing}")
    else:
        print("Todos los footprints colocados OK")


if __name__ == "__main__":
    main()

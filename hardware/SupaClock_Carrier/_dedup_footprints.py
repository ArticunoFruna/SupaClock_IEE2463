#!/usr/bin/env python3
"""Elimina footprints duplicados (misma Reference, distinto footprint).
KiCad's F8 a veces deja el footprint viejo Y el nuevo cuando cambias el fp
en el esquematico. Este script mantiene SOLO el que coincide con lo que
espera el esquematico actual.
"""
import re
from pathlib import Path

PCB = Path(__file__).parent / "SupaClock_Carrier.kicad_pcb"

# Footprint esperado por cada Reference (segun el esquematico)
EXPECTED = {
    "U1": "Seeed_Studio_XIAO_Footprints:XIAO-ESP32-S3-DIP",
    "J3": "SupaClock_Custom:MAX30102_Castellated_1x4",
    "J14": "SupaClock_Custom:MAX30102_Castellated_1x4",
    "J4": "SupaClock_Custom:MAX30205_8pad_Flush",
    "J5": "Connector_PinSocket_2.54mm:PinSocket_1x07_P2.54mm_Vertical",
    "J6a1": "Connector_PinSocket_2.54mm:PinSocket_1x03_P2.54mm_Vertical",
    "J6b1": "Connector_PinSocket_2.54mm:PinSocket_1x03_P2.54mm_Vertical",
    "J6c1": "Connector_PinSocket_2.54mm:PinSocket_1x02_P2.54mm_Vertical",
    "J7": "Connector_PinSocket_2.54mm:PinSocket_1x08_P2.54mm_Vertical",
    "J11": "Connector_PinSocket_2.54mm:PinSocket_1x06_P2.54mm_Vertical",
    "J13": "Connector_PinSocket_2.54mm:PinSocket_1x03_P2.54mm_Vertical",
    "J12_RA1": "SupaClock_Custom:M3_Electrode_Pad",
    "J12_LA1": "SupaClock_Custom:M3_Electrode_Pad",
    "J12_RL1": "SupaClock_Custom:M3_Electrode_Pad",
    "J15": "Connector_PinSocket_2.54mm:PinSocket_1x02_P2.54mm_Vertical",
    "SW1": "SupaClock_Custom:SW_Tactile_Side_3.5x7.8mm",
    "SW2": "SupaClock_Custom:SW_Tactile_Side_3.5x7.8mm",
}


def find_footprint_blocks(text):
    """Yields (start, end, ref, fp_name) tuples for each footprint block."""
    i = 0
    while True:
        m = re.search(r'\(footprint "[^"]+"\n', text[i:])
        if not m:
            return
        start = i + m.start()
        depth = 0
        j = start
        while j < len(text):
            c = text[j]
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0:
                    end = j + 1
                    break
            j += 1
        block = text[start:end]
        fp_m = re.search(r'\(footprint "([^"]+)"', block)
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        if fp_m and ref_m:
            yield start, end, ref_m.group(1), fp_m.group(1)
        i = end


def main():
    src = PCB.read_text()

    # Pasada 1: indexar todos los bloques por referencia
    instances = {}  # ref -> list of (start, end, fp_name)
    for start, end, ref, fp in find_footprint_blocks(src):
        instances.setdefault(ref, []).append((start, end, fp))

    # Determinar bloques a eliminar
    to_remove = []  # list of (start, end)
    kept = []
    duplicates_seen = []
    for ref, lst in instances.items():
        if len(lst) == 1:
            kept.append((ref, lst[0][2]))
            continue
        # Mas de un footprint con la misma Reference
        duplicates_seen.append((ref, len(lst), [t[2] for t in lst]))
        # Buscar el que matchea EXPECTED
        expected_fp = EXPECTED.get(ref)
        keep_idx = None
        for i, (s, e, fp) in enumerate(lst):
            if fp == expected_fp:
                keep_idx = i
                break
        if keep_idx is None:
            # No matchea esperado: mantener el primero
            keep_idx = 0
        kept.append((ref, lst[keep_idx][2]))
        for i, (s, e, fp) in enumerate(lst):
            if i != keep_idx:
                to_remove.append((s, e, ref, fp))

    # Pasada 2: eliminar de atras hacia adelante para preservar indices
    to_remove.sort(key=lambda t: t[0], reverse=True)
    for s, e, ref, fp in to_remove:
        # Recortar whitespace alrededor para no dejar huecos
        ws_before = 0
        while s - ws_before > 0 and src[s - ws_before - 1] in "\t":
            ws_before += 1
        src = src[:s - ws_before].rstrip() + "\n" + src[e:].lstrip("\n")

    PCB.write_text(src)

    print(f"Instances totales antes: {sum(len(v) for v in instances.values())}")
    print(f"Duplicados detectados: {len(duplicates_seen)}")
    for ref, count, fps in duplicates_seen:
        print(f"  {ref}: {count} instancias")
        for fp in fps:
            print(f"      - {fp}")
    print(f"\nFootprints eliminados: {len(to_remove)}")
    for s, e, ref, fp in to_remove:
        print(f"  {ref} ({fp})")
    print(f"\nFootprints conservados: {len(kept)}")


if __name__ == "__main__":
    main()

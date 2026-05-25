#!/usr/bin/env bash
# Extracts lib_symbols block for the adapter schematic
# Symbols needed: Conn_01x08, Conn_01x24, R, C, C_Polarized, 2N7000, +3V3, GND, PWR_FLAG
set -euo pipefail

KSYM=/usr/share/kicad/symbols
OUT=/tmp/lib_symbols_block.txt
> "$OUT"

extract() {
    local file="$1" name="$2"
    # awk: print from (symbol "name" to its matching closing paren at indent level 1
    awk -v target="$name" '
        BEGIN{found=0}
        $0 ~ "^\t\\(symbol \"" target "\"" { found=1 }
        found==1 {
            print
            if ($0 ~ /^\t\)$/) { exit }
        }
    ' "$file" >> "$OUT"
}

extract "$KSYM/Connector_Generic.kicad_sym" "Conn_01x08"
extract "$KSYM/Connector_Generic.kicad_sym" "Conn_01x24"
extract "$KSYM/Device.kicad_sym"            "R"
extract "$KSYM/Device.kicad_sym"            "C"
extract "$KSYM/Device.kicad_sym"            "C_Polarized_Small"
extract "$KSYM/Transistor_FET.kicad_sym"    "2N7000"
extract "$KSYM/power.kicad_sym"             "+3V3"
extract "$KSYM/power.kicad_sym"             "GND"

# Patch lib_id prefixes (KiCad expects "LibName:SymbolName")
sed -i 's|^\t(symbol "Conn_01x08"|\t(symbol "Connector_Generic:Conn_01x08"|' "$OUT"
sed -i 's|^\t(symbol "Conn_01x24"|\t(symbol "Connector_Generic:Conn_01x24"|' "$OUT"
sed -i 's|^\t(symbol "R"|\t(symbol "Device:R"|' "$OUT"
sed -i 's|^\t(symbol "C"$|\t(symbol "Device:C"|' "$OUT"
sed -i 's|^\t(symbol "C_Polarized_Small"|\t(symbol "Device:C_Polarized_Small"|' "$OUT"
sed -i 's|^\t(symbol "2N7000"|\t(symbol "Transistor_FET:2N7000"|' "$OUT"
sed -i 's|^\t(symbol "+3V3"|\t(symbol "power:+3V3"|' "$OUT"
sed -i 's|^\t(symbol "GND"|\t(symbol "power:GND"|' "$OUT"

echo "Written: $OUT"
wc -l "$OUT"

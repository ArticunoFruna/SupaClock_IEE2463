import os
import re

dir_path = "/home/articunot/Documents/PlatformIO/Projects/SupaClock/hardware"
main_sch = os.path.join(dir_path, "SupaClock_Carrier_v4/SupaClock_MainBoard/SupaClock_MainBoard.kicad_sch")
ecg_sch = os.path.join(dir_path, "ECG/AD8232_Heart_Rate_Monitor.kicad_sch")
fg_sch = os.path.join(dir_path, "FullGei/SparkFun_Lipo_Fuel_Gauge.kicad_sch")
disp_sch = os.path.join(dir_path, "SupaClock_Display_Adapter/SupaClock_Display_Adapter.kicad_sch")

def extract_lib_symbols(filepath):
    print(f"Extracting symbols from {filepath}...")
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()
    
    # We want to find the (lib_symbols ... ) block
    # KiCad v6+ format has:
    # (lib_symbols
    #    (symbol "Library:SymbolName" ... )
    #    ...
    # )
    # Let's find "(lib_symbols" and then count matching parentheses to extract the block
    idx = content.find("(lib_symbols")
    if idx == -1:
        print("No lib_symbols block found.")
        return {}
    
    # Count matching parentheses to find the end of (lib_symbols ...)
    paren_count = 0
    end_idx = -1
    for i in range(idx, len(content)):
        char = content[i]
        if char == '(':
            paren_count += 1
        elif char == ')':
            paren_count -= 1
            if paren_count == 0:
                end_idx = i + 1
                break
                
    if end_idx == -1:
        print("Failed to find end of lib_symbols block.")
        return {}
        
    lib_symbols_block = content[idx:end_idx]
    
    # Now let's extract individual symbol definitions from inside the block
    # Each symbol definition starts with "(symbol \"[^\"]+\"" and ends with its matching parenthesis
    # Let's find all (symbol "..." starts
    symbol_defs = {}
    
    # We can scan the block for (symbol "..."
    # The block starts with (lib_symbols, so we skip it.
    pos = 12 # length of "(lib_symbols"
    while pos < len(lib_symbols_block):
        # find next "(symbol "
        match = re.search(r'\(symbol\s+\"([^\"]+)\"', lib_symbols_block[pos:])
        if not match:
            break
        
        start_in_slice = match.start()
        sym_name = match.group(1)
        
        # Absolute position in block
        sym_start_pos = pos + start_in_slice
        
        # Count matching parentheses
        paren_count = 0
        sym_end_pos = -1
        for i in range(sym_start_pos, len(lib_symbols_block)):
            char = lib_symbols_block[i]
            if char == '(':
                paren_count += 1
            elif char == ')':
                paren_count -= 1
                if paren_count == 0:
                    sym_end_pos = i + 1
                    break
        
        if sym_end_pos != -1:
            sym_def = lib_symbols_block[sym_start_pos:sym_end_pos]
            symbol_defs[sym_name] = sym_def
            pos = sym_end_pos
        else:
            pos += 1
            
    print(f"Extracted {len(symbol_defs)} symbol definitions.")
    return symbol_defs

# Run extraction
main_syms = extract_lib_symbols(main_sch)
ecg_syms = extract_lib_symbols(ecg_sch)
fg_syms = extract_lib_symbols(fg_sch)
disp_syms = extract_lib_symbols(disp_sch)

# Merge all symbols
all_syms = {}
all_syms.update(main_syms)
all_syms.update(ecg_syms)
all_syms.update(fg_syms)
all_syms.update(disp_syms)

print(f"\nTotal merged unique symbol definitions: {len(all_syms)}")

# Write to a file
out_path = os.path.join(dir_path, "all_extracted_symbols.txt")
with open(out_path, "w", encoding="utf-8") as f:
    f.write("(lib_symbols\n")
    for name, definition in sorted(all_syms.items()):
        f.write(definition + "\n")
    f.write(")\n")

print(f"Wrote all symbols to {out_path}")

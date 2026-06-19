import os
import re
import shutil

dir_path = "/home/articunot/Documents/PlatformIO/Projects/SupaClock/hardware/SupaClock_Carrier_v4"
main_custom_pretty = os.path.join(dir_path, "SupaClock_MainBoard/footprints/SupaClock_Custom.pretty")
sensor_custom_pretty = os.path.join(dir_path, "SupaClock_SensorBoard/footprints/SupaClock_Custom.pretty")

os.makedirs(main_custom_pretty, exist_ok=True)
os.makedirs(sensor_custom_pretty, exist_ok=True)

# 1. AD8232 (LFCSP-20, 4x4mm, pitch 0.5mm)
# Source: /usr/share/kicad/footprints/Package_CSP.pretty/LFCSP-20-1EP_4x4mm_P0.5mm_EP2.5x2.5mm_ThermalVias.kicad_mod
ad8232_src = "/usr/share/kicad/footprints/Package_CSP.pretty/LFCSP-20-1EP_4x4mm_P0.5mm_EP2.5x2.5mm_ThermalVias.kicad_mod"

# 2. MAX30205 (TDFN-8, 3x3mm, pitch 0.65mm)
# Source: /usr/share/kicad/footprints/Package_DFN_QFN.pretty/DFN-8-1EP_3x3mm_P0.65mm_EP1.55x2.4mm.kicad_mod
max30205_src = "/usr/share/kicad/footprints/Package_DFN_QFN.pretty/DFN-8-1EP_3x3mm_P0.65mm_EP1.55x2.4mm.kicad_mod"

# 3. MAX17048 (WDFN-8, 2x2mm, pitch 0.5mm)
# Source: /usr/share/kicad/footprints/Package_DFN_QFN.pretty/WDFN-8-1EP_2x2mm_P0.5mm_EP0.8x1.2mm.kicad_mod
max17048_src = "/usr/share/kicad/footprints/Package_DFN_QFN.pretty/WDFN-8-1EP_2x2mm_P0.5mm_EP0.8x1.2mm.kicad_mod"

def modify_ad8232(src, dst_name):
    print(f"Modifying AD8232 footprint {src}...")
    with open(src, "r") as f:
        content = f.read()
    
    # Rename footprint
    content = content.replace("LFCSP-20-1EP_4x4mm_P0.5mm_EP2.5x2.5mm_ThermalVias", dst_name)
    
    # We want to find pads 1 to 20 and stretch them.
    # In KiCad 6+: (pad "1" smd rect (at -1.95 1) (size 0.6 0.25) ...)
    # Let's write a regex that matches pads and modifies them.
    # Left side: Y in [1.0, 0.5, 0, -0.5, -1.0], at X around -1.95. Extend X size to 0.9, shift X to -2.15
    # Bottom side: X in [-1.0, -0.5, 0, +0.5, +1.0], at Y around 1.95 (wait, bottom is Y positive or negative depending on coords, standard KiCad Y is down positive, so bottom is Y positive). Extend Y size to 0.9, shift Y to 2.15
    # Right side: Y in [1.0, 0.5, 0, -0.5, -1.0], at X around 1.95. Extend X size to 0.9, shift X to 2.15
    # Top side: X in [-1.0, -0.5, 0, +0.5, +1.0], at Y around -1.95. Extend Y size to 0.9, shift Y to -2.15

    def pad_repl(match):
        pad_num = match.group(1)
        pad_type = match.group(2)
        pad_shape = match.group(3)
        x_str = match.group(4)
        y_str = match.group(5)
        w_str = match.group(6)
        h_str = match.group(7)
        rest = match.group(8)
        
        x = float(x_str)
        y = float(y_str)
        w = float(w_str)
        h = float(h_str)
        
        # Determine orientation
        if abs(x) > abs(y):
            # Left or Right side
            w = 1.0  # Extend width (smd length)
            if x < 0:
                x = -2.15  # Shift left pad left
            else:
                x = 2.15   # Shift right pad right
        else:
            # Top or Bottom side
            h = 1.0  # Extend height (smd length)
            if y < 0:
                y = -2.15  # Shift top pad up
            else:
                y = 2.15   # Shift bottom pad down
                
        return f'(pad "{pad_num}" {pad_type} {pad_shape} (at {x:.3f} {y:.3f}) (size {w:.3f} {h:.3f}) {rest}'

    # Regex for pad matching: (pad "1" smd rect (at -1.95 1) (size 0.6 0.25) ...)
    # Wait, some pads might not be rect, they could be roundrect or oval. Let's use wildcard for shape.
    pattern = r'\(pad\s+\"(\d+)\"\s+(smd)\s+(\w+)\s+\(at\s+([-\d\.]+)\s+([-\d\.]+)\)\s+\(size\s+([-\d\.]+)\s+([-\d\.]+)\)(.*?\n)'
    new_content = re.sub(pattern, pad_repl, content)
    return new_content

def modify_max30205(src, dst_name):
    print(f"Modifying MAX30205 footprint {src}...")
    with open(src, "r") as f:
        content = f.read()
        
    content = content.replace("DFN-8-1EP_3x3mm_P0.65mm_EP1.55x2.4mm", dst_name)
    
    # MAX30205 has 8 pins on left and right sides.
    # Left: X around -1.45. Right: X around 1.45.
    # Pads are horizontal (height is smaller than width). Extend width to 1.2, shift X to -1.65 (left) or 1.65 (right).
    def pad_repl(match):
        pad_num = match.group(1)
        pad_type = match.group(2)
        pad_shape = match.group(3)
        x_str = match.group(4)
        y_str = match.group(5)
        w_str = match.group(6)
        h_str = match.group(7)
        rest = match.group(8)
        
        x = float(x_str)
        y = float(y_str)
        w = float(w_str)
        h = float(h_str)
        
        if pad_num == "9":
            # Keep the central pad at 0 0 without shifting
            return f'(pad "{pad_num}" {pad_type} {pad_shape} (at 0.000 0.000) (size {w_str} {h_str}) {rest}'
            
        w = 1.2
        if x < 0:
            x = -1.65
        else:
            x = 1.65
            
        return f'(pad "{pad_num}" {pad_type} {pad_shape} (at {x:.3f} {y:.3f}) (size {w:.3f} {h:.3f}) {rest}'
        
    pattern = r'\(pad\s+\"(\d+)\"\s+(smd)\s+(\w+)\s+\(at\s+([-\d\.]+)\s+([-\d\.]+)\)\s+\(size\s+([-\d\.]+)\s+([-\d\.]+)\)(.*?\n)'
    new_content = re.sub(pattern, pad_repl, content)
    
    # Add a thermal via in the center pad (pad 9)
    # Drill 0.8mm, pad size 1.2mm fits well within the 1.55x2.4mm EP
    via_str = '  (pad "9" thru_hole circle (at 0 0) (size 1.2 1.2) (drill 0.8) (layers "*.Cu" "*.Mask"))\n'
    new_content = new_content.rstrip().rstrip(')') + via_str + ')'
    return new_content

def modify_max17048(src, dst_name):
    print(f"Modifying MAX17048 footprint {src}...")
    with open(src, "r") as f:
        content = f.read()
        
    content = content.replace("WDFN-8-1EP_2x2mm_P0.5mm_EP0.8x1.2mm", dst_name)
    
    # MAX17048 is WDFN-8 2x2mm. Pads are at X around -0.95 and 0.95.
    # Extend width to 0.9, shift X to -1.15 (left) or 1.15 (right).
    def pad_repl(match):
        pad_num = match.group(1)
        pad_type = match.group(2)
        pad_shape = match.group(3)
        x_str = match.group(4)
        y_str = match.group(5)
        w_str = match.group(6)
        h_str = match.group(7)
        rest = match.group(8)
        
        x = float(x_str)
        y = float(y_str)
        w = float(w_str)
        h = float(h_str)
        
        if pad_num == "9":
            # Rename pad "9" to "EP" to match schematic symbol and keep at 0 0
            return f'(pad "EP" {pad_type} {pad_shape} (at 0.000 0.000) (size {w_str} {h_str}) {rest}'
            
        w = 0.9
        if x < 0:
            x = -1.15
        else:
            x = 1.15
            
        return f'(pad "{pad_num}" {pad_type} {pad_shape} (at {x:.3f} {y:.3f}) (size {w:.3f} {h:.3f}) {rest}'
        
    pattern = r'\(pad\s+\"(\d+)\"\s+(smd)\s+(\w+)\s+\(at\s+([-\d\.]+)\s+([-\d\.]+)\)\s+\(size\s+([-\d\.]+)\s+([-\d\.]+)\)(.*?\n)'
    new_content = re.sub(pattern, pad_repl, content)
    
    # Add thermal via in EP pad "EP"
    # Drill 0.5mm, pad size 0.8mm avoids overlapping outer signal pads of the tiny 2x2mm WDFN package
    via_str = '  (pad "EP" thru_hole circle (at 0 0) (size 0.8 0.8) (drill 0.5) (layers "*.Cu" "*.Mask"))\n'
    new_content = new_content.rstrip().rstrip(')') + via_str + ')'
    return new_content

# Generate modified footprints
ad8232_mod = modify_ad8232(ad8232_src, "AD8232_LFCSP20_HandSolder")
max30205_mod = modify_max30205(max30205_src, "MAX30205_TDFN8_HandSolder")
max17048_mod = modify_max17048(max17048_src, "MAX17048_WDFN8_HandSolder")

# Write to library pretty folders
for pretty in [main_custom_pretty, sensor_custom_pretty]:
    with open(os.path.join(pretty, "AD8232_LFCSP20_HandSolder.kicad_mod"), "w") as f:
        f.write(ad8232_mod)
    with open(os.path.join(pretty, "MAX30205_TDFN8_HandSolder.kicad_mod"), "w") as f:
        f.write(max30205_mod)
    with open(os.path.join(pretty, "MAX17048_WDFN8_HandSolder.kicad_mod"), "w") as f:
        f.write(max17048_mod)

print("Footprints generated successfully in both MainBoard and SensorBoard custom libraries!")


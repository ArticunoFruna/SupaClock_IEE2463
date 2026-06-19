import re

filepath = "/home/articunot/Documents/PlatformIO/Projects/SupaClock/hardware/all_extracted_symbols.txt"

def parse_pin_offsets():
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

    # Find symbols
    # A symbol definition starts with (symbol "..." and ends with its matching parenthesis
    # Let's write a simple recursive-descent style S-expression parser
    def parse_sexpr(s):
        tokens = re.findall(r'\(|\)|"[^"]*"|[^\s()]+', s)
        stack = [[]]
        for token in tokens:
            if token == '(':
                new_list = []
                stack[-1].append(new_list)
                stack.append(new_list)
            elif token == ')':
                if len(stack) > 1:
                    stack.pop()
            else:
                if token.startswith('"') and token.endswith('"'):
                    token = token[1:-1]
                stack[-1].append(token)
        return stack[0][0] if stack[0] else []

    try:
        sexpr = parse_sexpr(content)
    except Exception as e:
        print(f"Error parsing S-expression: {e}")
        return

    # Sexpr is (lib_symbols (symbol ...) (symbol ...) ...)
    symbols_to_find = [
        "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD",
        "AD8232_Heart_Rate_Monitor-eagle-import:AD8232",
        "SparkFun_Lipo_Fuel_Gauge-eagle-import:MAX1704X",
        "Connector_Generic:Conn_01x06",
        "Connector_Generic:Conn_01x24",
        "Connector_Generic:Conn_01x02",
        "Connector_Generic:Conn_01x07",
        "Device:R",
        "Device:C",
        "Device:C_Polarized_Small",
        "Switch:SW_Push",
        "power:+3V3",
        "power:GND",
        "power:+BATT",
        "power:+5V"
    ]

    print("PIN_OFFSETS = {")
    for child in sexpr[1:]:
        if isinstance(child, list) and child[0] == 'symbol':
            sym_name = child[1]
            if sym_name in symbols_to_find or any(sym_name.endswith(":" + name) for name in symbols_to_find):
                # Find all pins in this symbol
                # Pin node: (pin <type> <shape> (at X Y rot) (length L) (name "...") (number "...") ...)
                pins = []
                
                def find_pins(node):
                    if not isinstance(node, list):
                        return
                    if node[0] == 'pin':
                        # Find the 'at' coordinate
                        at_node = None
                        number_node = None
                        name_node = None
                        for item in node:
                            if isinstance(item, list):
                                if item[0] == 'at':
                                    at_node = item
                                elif item[0] == 'number':
                                    number_node = item[1]
                                elif item[0] == 'name':
                                    name_node = item[1]
                        if at_node and number_node:
                            px = float(at_node[1])
                            py = float(at_node[2])
                            # Standard KiCad .kicad_sym files have Y pointing UP, but schematics have Y pointing DOWN.
                            # So the Y offset is negated when converting to schematic coords!
                            # Let's double check if we need to negate Y here.
                            # Yes! The build_schematic.py did: "negate Y values from the .kicad_sym pin definitions".
                            # Let's output it.
                            pins.append((number_node, name_node, px, -py))
                    for item in node:
                        find_pins(item)
                
                find_pins(child)
                # Sort pins by number
                pins.sort(key=lambda x: int(x[0]) if x[0].isdigit() else 99)
                print(f"    '{sym_name}': [")
                for num, name, px, py in pins:
                    print(f"        # Pin {num} ({name}): ({px}, {py})")
                print(f"        " + ", ".join(f"({px}, {py})" for num, name, px, py in pins))
                print("    ],")
    print("}")

if __name__ == '__main__':
    parse_pin_offsets()

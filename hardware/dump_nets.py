import re
import sys

def parse_sch(filepath):
    print(f"Parsing {filepath}...")
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()
    
    # We want to find symbols and their properties, as well as wires/pins/labels
    # KiCad v6+ schematic files contain (symbol (lib_id ...) (at ...) (in_bom ...) ... (uuid ...))
    # and wires (wire (pts (xy ...) (xy ...)) (uuid ...))
    # and labels (label "..." (at ...) (uuid ...))
    # and global_labels (global_label "..." (at ...) (uuid ...))
    # Let's search for symbols first.
    
    # A simple way is to find all symbol instances:
    # (symbol (lib_id "...") (at ...) ... (property "Reference" "U1" ...) ... (uuid "...") )
    # Let's parse nested parentheses or use regex.
    # Since KiCad files use standard S-expressions, we can parse them into lists.
    
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
    
    # Find all symbols and labels
    symbols = []
    labels = []
    global_labels = []
    pins = []
    wires = []
    hierarchical_labels = []
    
    def walk(node):
        if not isinstance(node, list) or len(node) == 0:
            return
        if node[0] == 'symbol' and len(node) > 1 and isinstance(node[1], list) and node[1][0] == 'lib_id':
            # It's an instantiated symbol if it has a uuid and is not in lib_symbols
            # Wait, lib_symbols also has symbols. Let's check parentage.
            # In kicad_sch, lib_symbols is under (kicad_sch ... (lib_symbols ...))
            symbols.append(node)
        elif node[0] == 'label':
            labels.append(node)
        elif node[0] == 'global_label':
            global_labels.append(node)
        elif node[0] == 'hierarchical_label':
            hierarchical_labels.append(node)
        elif node[0] == 'wire':
            wires.append(node)
        
        for child in node:
            walk(child)
            
    walk(sexpr)
    
    # Let's filter out symbol definitions from lib_symbols (they don't have properties like 'in_bom' or 'at' directly under them, or rather they are inside lib_symbols)
    # Let's inspect the symbols we found.
    # An instantiated symbol has (property "Reference" "U1" ...) and is not inside a 'lib_symbols' node.
    instantiated_symbols = []
    for sym in symbols:
        # Let's find properties
        ref = None
        val = None
        for child in sym:
            if isinstance(child, list) and child[0] == 'property' and len(child) > 2:
                if child[1] == 'Reference':
                    ref = child[2]
                elif child[1] == 'Value':
                    val = child[2]
        if ref:
            instantiated_symbols.append((ref, val, sym))
            
    print(f"Found {len(instantiated_symbols)} instantiated symbols:")
    for ref, val, sym in sorted(instantiated_symbols, key=lambda x: x[0]):
        print(f"  {ref}: {val}")
        
    # Let's also print labels
    print(f"\nLabels:")
    for lbl in labels:
        if len(lbl) > 1:
            print(f"  Local Label: {lbl[1]} at {lbl[2] if len(lbl) > 2 else ''}")
    for lbl in global_labels:
        if len(lbl) > 1:
            print(f"  Global Label: {lbl[1]}")
            
if __name__ == '__main__':
    if len(sys.argv) > 1:
        parse_sch(sys.argv[1])
    else:
        print("Provide schematic file path.")

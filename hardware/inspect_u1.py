import re
import sys

def parse_u1_connections(filepath):
    print(f"Parsing netlist {filepath} for U1...")
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

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

    sexpr = parse_sexpr(content)
    nets_block = None
    for child in sexpr:
        if isinstance(child, list) and child[0] == 'nets':
            nets_block = child
            break

    if not nets_block:
        print("No nets block found.")
        return

    # Let's map each pin of U1 to its net and all other nodes connected to that net
    u1_pins = {}
    for i in range(1, 21):
        u1_pins[str(i)] = None
    u1_pins['PAD'] = None

    for net in nets_block[1:]:
        if isinstance(net, list) and net[0] == 'net':
            net_name = ""
            nodes = []
            for item in net:
                if isinstance(item, list):
                    if item[0] == 'name':
                        net_name = item[1]
                    elif item[0] == 'node':
                        node_ref = ""
                        node_pin = ""
                        for subitem in item:
                            if isinstance(subitem, list):
                                if subitem[0] == 'ref':
                                    node_ref = subitem[1]
                                elif subitem[0] == 'pin':
                                    node_pin = subitem[1]
                        nodes.append((node_ref, node_pin))
            
            # Check if any node is U1
            for ref, pin in nodes:
                if ref == 'U1':
                    u1_pins[pin] = (net_name, [f"{r}-{p}" for r, p in nodes if r != 'U1'])

    print("AD8232 (U1) Pin Connections:")
    for pin, info in sorted(u1_pins.items(), key=lambda x: int(x[0]) if x[0].isdigit() else 99):
        if info:
            print(f"  Pin {pin} ({info[0]}): {', '.join(info[1])}")
        else:
            print(f"  Pin {pin}: Not connected in netlist")

if __name__ == '__main__':
    parse_u1_connections("ECG/ad8232.net")

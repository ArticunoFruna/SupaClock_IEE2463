import re
import sys

def parse_netlist(filepath):
    print(f"Parsing netlist {filepath}...")
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

    try:
        sexpr = parse_sexpr(content)
    except Exception as e:
        print(f"Error parsing S-expression: {e}")
        return

    # Find the (nets ...) block
    nets_block = None
    for child in sexpr:
        if isinstance(child, list) and child[0] == 'nets':
            nets_block = child
            break

    if not nets_block:
        print("No nets block found in netlist.")
        return

    for net in nets_block[1:]:
        if isinstance(net, list) and net[0] == 'net':
            net_code = ""
            net_name = ""
            nodes = []
            for item in net:
                if isinstance(item, list):
                    if item[0] == 'code':
                        net_code = item[1]
                    elif item[0] == 'name':
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
                        nodes.append(f"{node_ref}-{node_pin}")
            print(f"Net {net_code} ({net_name}):")
            print("  " + ", ".join(nodes))

if __name__ == '__main__':
    if len(sys.argv) > 1:
        parse_netlist(sys.argv[1])
    else:
        print("Provide netlist file path.")

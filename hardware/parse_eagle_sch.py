import xml.etree.ElementTree as ET
import sys

def parse_eagle(filepath):
    print(f"Parsing Eagle schematic: {filepath}")
    tree = ET.parse(filepath)
    root = tree.getroot()
    
    # We want to find all nets and their connections (pinref parts and pins)
    # Nets are under /eagle/drawing/schematic/sheets/sheet/nets/net
    # Let's find sheet first
    nets = root.findall('.//sheet/nets/net')
    
    print(f"Found {len(nets)} nets:")
    for net in nets:
        net_name = net.get('name')
        # Find all pinrefs in this net
        pinrefs = net.findall('.//pinref')
        conn_list = []
        for pr in pinrefs:
            part = pr.get('part')
            pin = pr.get('pin')
            conn_list.append(f"{part}-{pin}")
        
        # Also let's check for labels/notes in this net if any
        # (Though pinrefs are the most important for electrical connections)
        if conn_list:
            print(f"  Net '{net_name}': {', '.join(conn_list)}")

if __name__ == '__main__':
    if len(sys.argv) > 1:
        parse_eagle(sys.argv[1])
    else:
        print("Provide Eagle schematic file path.")

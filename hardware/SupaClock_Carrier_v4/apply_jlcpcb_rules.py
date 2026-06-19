#!/usr/bin/env python3
import os
import shutil
import json
import re
import fnmatch

# Paths
dir_path = os.path.dirname(os.path.abspath(__file__))
pro_path = os.path.join(dir_path, 'SupaClock_Carrier.kicad_pro')
pcb_path = os.path.join(dir_path, 'SupaClock_Carrier.kicad_pcb')

# Backups
shutil.copy2(pro_path, pro_path + '.bak_jlcpcb')
shutil.copy2(pcb_path, pcb_path + '.bak_jlcpcb')
print("Created backups for .kicad_pro and .kicad_pcb")

# Update .kicad_pro
with open(pro_path, 'r') as f:
    pro_data = json.load(f)

# Rules update
rules = pro_data['board']['design_settings']['rules']
rules.update({
    'min_clearance': 0.127,
    'min_track_width': 0.127,
    'min_via_diameter': 0.45,
    'min_via_annular_width': 0.13,
    'min_through_hole_diameter': 0.3,
    'min_hole_to_hole': 0.5,
    'min_hole_clearance': 0.25,
    'min_copper_edge_clearance': 0.2,
    'min_text_height': 1.0,
})

# Net classes update
classes_target = {
    'Default':   {'clearance': 0.15, 'track_width': 0.2, 'via_diameter': 0.6, 'via_drill': 0.3},
    'Analog':    {'clearance': 0.2,  'track_width': 0.2, 'via_diameter': 0.6, 'via_drill': 0.3},
    'Power':     {'clearance': 0.2,  'track_width': 0.4, 'via_diameter': 0.8, 'via_drill': 0.4},
    'Power_HI':  {'clearance': 0.2,  'track_width': 0.6, 'via_diameter': 0.8, 'via_drill': 0.4},
    'Signal_HF': {'clearance': 0.2,  'track_width': 0.2, 'via_diameter': 0.6, 'via_drill': 0.3},
}

for c in pro_data['net_settings']['classes']:
    if c['name'] in classes_target:
        c.update(classes_target[c['name']])

# Predefined sizes update
track_widths = pro_data['board']['design_settings'].get('track_widths', [])
for w in [0.2, 0.4, 0.6, 0.8, 1.0]:
    if w not in track_widths:
        track_widths.append(w)
track_widths.sort()
pro_data['board']['design_settings']['track_widths'] = track_widths

via_dimensions = pro_data['board']['design_settings'].get('via_dimensions', [])
via_sizes = [(v['diameter'], v['drill']) for v in via_dimensions]
for d, dr in [(0.6, 0.3), (0.8, 0.4)]:
    if (d, dr) not in via_sizes:
        via_dimensions.append({'diameter': d, 'drill': dr})
# sort by diameter
via_dimensions.sort(key=lambda v: v['diameter'])
pro_data['board']['design_settings']['via_dimensions'] = via_dimensions

with open(pro_path, 'w') as f:
    json.dump(pro_data, f, indent=2)
print("Updated SupaClock_Carrier.kicad_pro")

# Get patterns for matching nets to classes
patterns = []
for item in pro_data['net_settings']['netclass_patterns']:
    patterns.append((item['pattern'], item['netclass']))

def get_netclass(net_name):
    for pattern, netclass in patterns:
        normalized_net = net_name
        if normalized_net.startswith('/'):
            normalized_net_no_slash = normalized_net[1:]
        else:
            normalized_net_no_slash = normalized_net
            normalized_net = '/' + normalized_net
        
        if fnmatch.fnmatch(normalized_net, pattern) or fnmatch.fnmatch(normalized_net_no_slash, pattern):
            return netclass
        if '*' not in pattern and (pattern in normalized_net or pattern in normalized_net_no_slash):
            return netclass
    return 'Default'

# Update .kicad_pcb vias
with open(pcb_path, 'r') as f:
    pcb_content = f.read()

# Find each via block (handling tabs, newlines, etc.)
via_blocks = re.findall(r'\(via\b.*?\n\t\)', pcb_content, re.DOTALL)
print(f"Processing {len(via_blocks)} vias in PCB...")

modified_count = 0
for via_block in via_blocks:
    net_match = re.search(r'\(net\s+\"([^\"]+)\"\)', via_block)
    if not net_match:
        continue
    net = net_match.group(1)
    netclass = get_netclass(net)
    target = classes_target.get(netclass, classes_target['Default'])
    dia = target['via_diameter']
    drill = target['via_drill']
    
    # Replace size and drill
    new_via_block = re.sub(r'\(size\s+[\d\.]+\)', f'(size {dia})', via_block)
    new_via_block = re.sub(r'\(drill\s+[\d\.]+\)', f'(drill {drill})', new_via_block)
    
    if new_via_block != via_block:
        pcb_content = pcb_content.replace(via_block, new_via_block)
        modified_count += 1
        print(f"Resized via on Net {net} ({netclass}): size {dia}, drill {drill}")

with open(pcb_path, 'w') as f:
    f.write(pcb_content)

print(f"Updated {modified_count} vias in SupaClock_Carrier.kicad_pcb")

#!/usr/bin/env python3
import re
import os
import shutil

dir_path = os.path.dirname(os.path.abspath(__file__))
pcb_path = os.path.join(dir_path, 'SupaClock_Carrier.kicad_pcb')
bak_path = os.path.join(dir_path, 'SupaClock_Carrier.kicad_pcb.bak_jlcpcb')

# Backup the current file first
shutil.copy2(pcb_path, pcb_path + '.bak_user_edits')
print("Backed up current user edited board.")

with open(pcb_path, 'r') as f:
    content = f.read()

def find_sexpr_bounds(text, keyword):
    pos = text.find('(' + keyword)
    if pos == -1:
        return None
    paren_count = 0
    end = pos
    while end < len(text):
        if text[end] == '(':
            paren_count += 1
        elif text[end] == ')':
            paren_count -= 1
            if paren_count == 0:
                end += 1
                return pos, end
        end += 1
    return None

def update_effects_by_uuid(content, uuid, justify_mirror=False, font_size=None, font_thickness=None):
    idx = content.find(f'"{uuid}"')
    if idx == -1:
        print(f"UUID {uuid} not found!")
        return content
    
    # Find start of the block by searching backwards for '\n\t\t('
    start_pos = idx
    while start_pos > 0:
        if content[start_pos-3:start_pos+1] == '\n\t\t(':
            break
        start_pos -= 1
        
    paren_count = 0
    end_pos = start_pos
    while end_pos < len(content):
        if content[end_pos] == '(':
            paren_count += 1
        elif content[end_pos] == ')':
            paren_count -= 1
            if paren_count == 0:
                end_pos += 1
                break
        end_pos += 1
        
    block = content[start_pos:end_pos]
    
    # Indentation detection
    indent_match = re.match(r'^\s*', block)
    indent = indent_match.group(0) if indent_match else '\t\t\t\t'
    if not indent:
        indent = '\t\t\t\t'
    else:
        indent = indent + '\t'
        
    # Process the block
    if justify_mirror:
        if '(justify' in block:
            if 'mirror' not in block:
                block = re.sub(r'\(justify\s+([^\)]+)\)', r'(justify \1 mirror)', block)
        else:
            bounds = find_sexpr_bounds(block, 'font')
            if bounds:
                font_start, font_end = bounds
                block = block[:font_end] + f'\n{indent}(justify mirror)' + block[font_end:]
            else:
                effects_idx = block.find('(effects')
                if effects_idx != -1:
                    block = block[:effects_idx+8] + f'\n{indent}(justify mirror)' + block[effects_idx+8:]
                    
    if font_size and font_thickness:
        block = re.sub(r'\(size\s+[\d\.]+(\s+[\d\.]+)?\)', f'(size {font_size} {font_size})', block)
        block = re.sub(r'\(thickness\s+[\d\.]+\)', f'(thickness {font_thickness})', block)
        
    return content[:start_pos] + block + content[end_pos:]

# UUIDs to mirror (J3, J4, J14)
uuids_to_mirror = [
    # J3
    'be36f221-a1b6-4d1e-af2e-d474b3e318cf', # Reference
    '439a77aa-3a11-49f3-943a-5024110ae596', # Value
    'c4fb83b4-5ffe-4e0b-b973-770947868044', # fp_text user "${REFERENCE}"
    # J4
    '1de62558-7e9e-4cad-b5fb-b9a443cf89e9', # Reference
    'f022074b-9b7f-4481-a5d3-ec3f13ae0a39', # Value
    'c1e4f987-5811-470f-8b00-dbac5c7ec7f4', # fp_text user "${REFERENCE}"
    # J14
    'c60335f8-45c7-40ba-94eb-b866d4a59d9d', # Reference
    '6057a477-9ec1-48e3-9795-3819eaaceb8f', # Value
    '1f218914-e166-400d-abd2-5c264398cbf0', # fp_text user "${REFERENCE}"
]

for uuid in uuids_to_mirror:
    content = update_effects_by_uuid(content, uuid, justify_mirror=True)

# U1 size fix (Reference)
u1_uuid = 'ce14da61-7610-4ef8-a9c2-624a58dc488d'
content = update_effects_by_uuid(content, u1_uuid, font_size=1.0, font_thickness=0.15)

with open(pcb_path, 'w') as f:
    f.write(content)

print("Finished adjusting silkscreen text sizes and mirrors.")

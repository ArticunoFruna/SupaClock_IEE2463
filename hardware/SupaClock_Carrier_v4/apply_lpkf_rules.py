#!/usr/bin/env python3
"""
apply_lpkf_rules.py — Apply LPKF DRC rules + I2C_SDA via elimination
=====================================================================
Applies Paso 1 (DRC rules, power widening, GND pour) and Paso 2 (I2C_SDA
via elimination). SPI_CS via and BTN_NEXT routing require manual KiCad work.
"""
import pcbnew, shutil, os

PCB = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   'SupaClock_Carrier.kicad_pcb')

def mm(v): return pcbnew.FromMM(v)
def vec(x,y): return pcbnew.VECTOR2I(mm(x), mm(y))

# Backup
bak = PCB + '.bak_lpkf'
shutil.copy2(PCB, bak)
print(f'Backup: {os.path.basename(bak)}')

board = pcbnew.LoadBoard(PCB)
F_CU = board.GetLayerID('F.Cu')
B_CU = board.GetLayerID('B.Cu')

# Save original positions
orig_pos = {}
for fp in board.GetFootprints():
    orig_pos[fp.GetReference()] = (
        pcbnew.ToMM(fp.GetPosition().x),
        pcbnew.ToMM(fp.GetPosition().y))

# ══════════ PASO 1: DRC RULES ══════════
print('\n── Paso 1: DRC Rules ──')
ds = board.GetDesignSettings()
ds.m_MinClearance = mm(0.40)
ds.m_TrackMinWidth = mm(0.40)
ds.m_ViasMinSize = mm(2.00)
ds.m_MinThroughDrill = mm(0.80)
ds.m_ViasMinAnnularWidth = mm(0.60)
ds.m_CopperEdgeClearance = mm(0.50)
print('  Board-level rules set')

for nc_name in board.GetNetClasses():
    nc = board.GetNetClasses()[nc_name]
    nc.SetViaDiameter(mm(2.0))
    nc.SetViaDrill(mm(0.8))
print('  Net class vias updated')

via_list = ds.m_ViasDimensionsList
for i in range(via_list.size()):
    if pcbnew.ToMM(via_list[i].m_Diameter) > 0:
        via_list[i].m_Diameter = mm(2.0)
        via_list[i].m_Drill = mm(0.8)

# Update existing vias
for t in board.GetTracks():
    if type(t).__name__ == 'PCB_VIA':
        t.SetFrontWidth(mm(2.0))
        t.SetDrill(mm(0.8))
print('  Via objects: 2.0mm/0.8mm')

# Widen power tracks
power_nets = {'/GND', '/+3V3', '/+BATT'}
w_count = 0
for t in board.GetTracks():
    if type(t).__name__ == 'PCB_TRACK':
        if t.GetNet().GetNetname() in power_nets:
            if pcbnew.ToMM(t.GetWidth()) < 0.8:
                t.SetWidth(mm(0.8))
                w_count += 1
print(f'  Power tracks widened: {w_count}')

# GND pour B.Cu
gnd_net = board.GetNetInfo().GetNetItem('/GND')
zone = pcbnew.ZONE(board)
zone.SetNet(gnd_net); zone.SetLayer(B_CU)
zone.SetZoneName('GND_Pour_BCu')
zone.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
zone.SetThermalReliefGap(mm(0.5))
zone.SetThermalReliefSpokeWidth(mm(0.5))
zone.SetMinThickness(mm(0.4))
zone.SetLocalClearance(mm(0.4))
zone.SetIsFilled(False)
zone.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_AREA)
zone.SetMinIslandArea(mm(10)*mm(10))
outline = zone.Outline(); outline.NewOutline()
for x,y in [(61.5,23.5),(146.5,23.5),(146.5,90.5),(61.5,90.5)]:
    outline.Append(mm(x), mm(y))
board.Add(zone)
print('  GND pour on B.Cu added')

# ══════════ PASO 2: I2C_SDA VIA ELIMINATION ══════════
print('\n── Paso 2: I2C_SDA Via Elimination ──')
i2c_sda = board.GetNetInfo().GetNetItem('/I2C_SDA')

via1 = (107.3641, 70.6258)
via2 = (112.4520, 74.0374)
to_remove = []
for t in board.GetTracks():
    if t.GetNet().GetNetname() != '/I2C_SDA': continue
    if type(t).__name__ == 'PCB_VIA':
        to_remove.append(t); continue
    sx = pcbnew.ToMM(t.GetStart().x)
    sy = pcbnew.ToMM(t.GetStart().y)
    ex = pcbnew.ToMM(t.GetEnd().x)
    ey = pcbnew.ToMM(t.GetEnd().y)
    for vx,vy in [via1, via2]:
        if (abs(sx-vx)<0.01 and abs(sy-vy)<0.01) or \
           (abs(ex-vx)<0.01 and abs(ey-vy)<0.01):
            to_remove.append(t); break

for t in to_remove:
    board.Remove(t)
print(f'  Removed {len(to_remove)} items (2 vias + connected tracks)')

# B.Cu bridges
for s,e in [((110.31,76.18),(119.99,69.94)),
            ((107.36,65.06),(111.90,61.85))]:
    t = pcbnew.PCB_TRACK(board)
    t.SetNet(i2c_sda); t.SetLayer(B_CU)
    t.SetStart(vec(s[0],s[1])); t.SetEnd(vec(e[0],e[1]))
    t.SetWidth(mm(0.4)); board.Add(t)
print('  B.Cu bridges added')

# Remove dangling F.Cu
for t in list(board.GetTracks()):
    if t.GetNet().GetNetname() == '/I2C_SDA' and type(t).__name__ == 'PCB_TRACK':
        if t.GetLayer() == F_CU:
            sx = pcbnew.ToMM(t.GetStart().x)
            sy = pcbnew.ToMM(t.GetStart().y)
            ex = pcbnew.ToMM(t.GetEnd().x)
            if abs(sx-125.47)<0.01 and abs(sy-69.94)<0.01 and abs(ex-116.55)<0.01:
                board.Remove(t)
                print('  Removed dangling F.Cu stub')

# ══════════ FILL + SAVE ══════════
print('\n── Filling zones ──')
filler = pcbnew.ZONE_FILLER(board)
filler.Fill(list(board.Zones()))
board.Save(PCB)
print(f'Saved to {os.path.basename(PCB)}')

# ══════════ VERIFY ══════════
print('\n' + '='*50)
print('VERIFICATION')
print('='*50)
b2 = pcbnew.LoadBoard(PCB)
ds2 = b2.GetDesignSettings()
checks = [
    ('Min clearance', pcbnew.ToMM(ds2.m_MinClearance), 0.40),
    ('Min via dia', pcbnew.ToMM(ds2.m_ViasMinSize), 2.00),
    ('Min via drill', pcbnew.ToMM(ds2.m_MinThroughDrill), 0.80),
]
for name,act,exp in checks:
    s = 'OK' if abs(act-exp)<0.001 else 'FAIL'
    print(f'  [{s:4s}] {name}: {act:.2f} (exp {exp:.2f})')

pw_bad = sum(1 for t in b2.GetTracks()
             if type(t).__name__=='PCB_TRACK' and
             t.GetNet().GetNetname() in power_nets and
             pcbnew.ToMM(t.GetWidth()) < 0.8)
print(f'  [{"OK" if pw_bad==0 else "FAIL":4s}] Power track violations: {pw_bad}')

zones = len(b2.Zones())
print(f'  [{"OK" if zones>=1 else "FAIL":4s}] Zones: {zones}')

vias = sum(1 for t in b2.GetTracks() if type(t).__name__=='PCB_VIA')
print(f'  [{"OK" if vias==1 else "INFO":4s}] Vias remaining: {vias} (SPI_CS only)')

# Golden rule
fp_moved = 0
for fp in b2.GetFootprints():
    r = fp.GetReference()
    if r in orig_pos:
        ox,oy = orig_pos[r]
        nx = pcbnew.ToMM(fp.GetPosition().x)
        ny = pcbnew.ToMM(fp.GetPosition().y)
        if abs(ox-nx)>0.001 or abs(oy-ny)>0.001:
            fp_moved += 1
print(f'  [{"OK" if fp_moved==0 else "FAIL":4s}] Footprints moved: {fp_moved}')

print(f'\n{"ALL OK" if pw_bad==0 and fp_moved==0 else "ISSUES"}')
print(f'\n⚠️  MANUAL WORK NEEDED in KiCad pcbnew:')
print(f'  1. SPI_CS via at (113.58, 50.54): delete via, re-route on F.Cu')
print(f'     Route between ECG_SDN (y=46.63) and ECG_OUT (y=47.85)')
print(f'  2. BTN_NEXT: connect U1 pad7 (128.88,34.88) to SW2 (141.90,59.25)')
print(f'     Route along board right edge on F.Cu or B.Cu')
print(f'  3. I2C_SDA/SCL B.Cu crossings: adjust bridge track waypoints')

#!/usr/bin/env python3
"""Generates SupaClock_Display_Adapter.kicad_sch from scratch.

Reads the prebuilt lib_symbols block (/tmp/lib_symbols_block_full.txt) and
emits a connected schematic via global labels placed at actual pin tips.

Pin-tip coords (relative to symbol center, at rotation 0):
  Conn_01x08: pin i at (-5.08, 7.62 - (i-1)*2.54)
  Conn_01x24: pin i at (-5.08, 27.94 - (i-1)*2.54)
  R (Device:R, vertical): pin 1 at (0, +2.54), pin 2 at (0, -2.54)
  C (Device:C, vertical):  pin 1 at (0, +2.54), pin 2 at (0, -2.54)
  C_Polarized_Small:       pin 1 at (0, +2.0),  pin 2 at (0, -2.0)
  2N7000 (TO-92):          pin 1=S at (-2.54, -2.54), pin 2=G at (-5.08, 0), pin 3=D at (-2.54, +2.54)
  Power +3V3: pin 1 at (0, 0) -- connects upward from anchor (use rot=0)
  Power GND:  pin 1 at (0, 0) -- connects downward, place above with rot=0

Symbol rotation transforms pin offsets:
  rot 0:   (dx, dy)
  rot 90:  (-dy, dx)
  rot 180: (-dx, -dy)
  rot 270: (dy, -dx)
"""
import uuid
from pathlib import Path

HERE = Path(__file__).parent
OUT = HERE / "SupaClock_Display_Adapter.kicad_sch"
LIB_BLOCK = Path("/tmp/lib_symbols_block_v2.txt")

ROOT_SHEET_UUID = "00000000-0000-0000-0000-000000000002"
PROJECT_NAME = "SupaClock_Display_Adapter"


def U() -> str:
    return str(uuid.uuid4())


GRID = 1.27  # KiCad default connection grid (50 mil)


def S(v: float) -> float:
    """Snap a coordinate to the schematic grid."""
    return round(v / GRID) * GRID


# Pin offsets at rotation 0 (relative to symbol center, in schematic coords).
# IMPORTANT: KiCad symbol libraries use Y-UP convention but schematics use
# Y-DOWN. So Y values from the .kicad_sym pin definitions are NEGATED here.
PIN_OFFSETS = {
    "Connector_Generic:Conn_01x08": [(-5.08, -7.62 + i * 2.54) for i in range(8)],
    "Connector_Generic:Conn_01x24": [(-5.08, -27.94 + i * 2.54) for i in range(24)],
    "Device:R":                     [(0, -3.81), (0,  3.81)],
    "Device:C":                     [(0, -3.81), (0,  3.81)],
    "Device:C_Polarized_Small":     [(0, -2.54), (0,  2.54)],
    "Transistor_FET:2N7000":        [(2.54, 5.08), (-5.08, 0), (2.54, -5.08)],
    "power:+3V3":                   [(0, 0)],
    "power:GND":                    [(0, 0)],
    "power:PWR_FLAG":               [(0, 0)],
}


def rotate(dx: float, dy: float, rot: int):
    if rot == 0:    return ( dx,  dy)
    if rot == 90:   return (-dy,  dx)
    if rot == 180: return (-dx, -dy)
    if rot == 270:  return ( dy, -dx)
    raise ValueError(rot)


class Sch:
    def __init__(self):
        self.symbols = []          # symbol-instance blocks
        self.labels = []           # global label blocks
        self.wires = []            # wire blocks
        self.placed = []           # (ref, lib_id, x, y, rot) for documentation

    def place(self, ref, value, lib_id, footprint, x, y, rot=0, mirror=""):
        x = S(x); y = S(y)
        sym_uuid = U()
        n_pins = len(PIN_OFFSETS[lib_id])
        pin_uuids = [U() for _ in range(n_pins)]
        pin_block = "\n".join(
            f'\t\t(pin "{i+1}"\n\t\t\t(uuid "{pin_uuids[i]}")\n\t\t)'
            for i in range(n_pins)
        )
        mirror_line = f"\n\t\t(mirror {mirror})" if mirror else ""
        # Hide reference/value for power symbols (they show their own label)
        hide_ref = lib_id.startswith("power:")
        hide_val = lib_id.startswith("power:")
        ref_hide = "\n\t\t\t(hide yes)" if hide_ref else ""
        val_hide = "\n\t\t\t(hide yes)" if hide_val else ""
        s = f"""\t(symbol
\t\t(lib_id "{lib_id}")
\t\t(at {x} {y} {rot}){mirror_line}
\t\t(unit 1)
\t\t(exclude_from_sim no)
\t\t(in_bom yes)
\t\t(on_board yes)
\t\t(dnp no)
\t\t(uuid "{sym_uuid}")
\t\t(property "Reference" "{ref}"
\t\t\t(at {x + 3.0} {y - 3.0} 0){ref_hide}
\t\t\t(effects
\t\t\t\t(font (size 1.27 1.27))
\t\t\t\t(justify left)
\t\t\t)
\t\t)
\t\t(property "Value" "{value}"
\t\t\t(at {x + 3.0} {y + 1.5} 0){val_hide}
\t\t\t(effects
\t\t\t\t(font (size 1.27 1.27))
\t\t\t\t(justify left)
\t\t\t)
\t\t)
\t\t(property "Footprint" "{footprint}"
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(effects
\t\t\t\t(font (size 1.27 1.27))
\t\t\t)
\t\t)
\t\t(property "Datasheet" ""
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(effects (font (size 1.27 1.27)))
\t\t)
\t\t(property "Description" ""
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(effects (font (size 1.27 1.27)))
\t\t)
{pin_block}
\t\t(instances
\t\t\t(project "{PROJECT_NAME}"
\t\t\t\t(path "/{ROOT_SHEET_UUID}"
\t\t\t\t\t(reference "{ref}")
\t\t\t\t\t(unit 1)
\t\t\t\t)
\t\t\t)
\t\t)
\t)"""
        self.symbols.append(s)
        self.placed.append((ref, lib_id, x, y, rot))
        return sym_uuid

    def pin_xy(self, lib_id, x, y, rot, pin_i):
        """Absolute pin-tip coordinate. pin_i is 1-based."""
        dx, dy = PIN_OFFSETS[lib_id][pin_i - 1]
        rdx, rdy = rotate(dx, dy, rot)
        return (x + rdx, y + rdy)

    def label(self, text, x, y, rot=0, shape="bidirectional"):
        x = S(x); y = S(y)
        s = f"""\t(global_label "{text}"
\t\t(shape {shape})
\t\t(at {x} {y} {rot})
\t\t(fields_autoplaced yes)
\t\t(effects
\t\t\t(font (size 1.27 1.27))
\t\t\t(justify left)
\t\t)
\t\t(uuid "{U()}")
\t\t(property "Intersheetrefs" "${{INTERSHEET_REFS}}"
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(effects
\t\t\t\t(font (size 1.27 1.27))
\t\t\t\t(justify left)
\t\t\t)
\t\t)
\t)"""
        self.labels.append(s)

    def power_at(self, lib_id_short, x, y, rot=0):
        """Place a power symbol (+3V3 or GND) at exact (x,y)."""
        lib_id = f"power:{lib_id_short}"
        ref = f"#PWR_{U()[:4]}"
        self.place(ref, lib_id_short, lib_id, "", x, y, rot)

    def wire(self, x0, y0, x1, y1):
        s = f"""\t(wire
\t\t(pts (xy {x0} {y0}) (xy {x1} {y1}))
\t\t(stroke (width 0) (type default))
\t\t(uuid "{U()}")
\t)"""
        self.wires.append(s)

    def label_pin(self, ref, lib_id, x, y, rot, pin_i, net, label_shape="bidirectional"):
        """Place a label exactly at the pin tip — KiCad treats co-located label
        as electrically connected. Label rotation makes text point AWAY from the symbol."""
        px, py = self.pin_xy(lib_id, x, y, rot, pin_i)
        # Compute outward direction: the pin tip is at distance from center,
        # so direction = (px-x, py-y) normalized. Label rotation:
        #   pin extends LEFT (-X): label rot = 180 (text points left)
        #   pin extends RIGHT (+X): label rot = 0
        #   pin extends UP (+Y): label rot = 90
        #   pin extends DOWN (-Y): label rot = 270
        ddx = px - x
        ddy = py - y
        if abs(ddx) > abs(ddy):
            lr = 180 if ddx < 0 else 0
        else:
            lr = 90 if ddy > 0 else 270
        self.label(net, px, py, lr, label_shape)

    def power_pin(self, ref, lib_id, x, y, rot, pin_i, power_net):
        """Place a power symbol exactly at the pin tip."""
        px, py = self.pin_xy(lib_id, x, y, rot, pin_i)
        # +3V3 anchor points downward (so place above the connection? no, AT the connection;
        # the symbol's "leg" extends upward from its anchor)
        # GND anchor points upward; the symbol extends downward
        # For a power pin connection: just place the power symbol AT pin tip with rot=0.
        # The visual orientation of the power symbol depends on rotation:
        #   +3V3 rot 0: arrow points up (good for top of schematic)
        #   GND rot 0: chevron points down (good for bottom)
        # For connections on the side of components, use rotation 90/270.
        # Simplest: always rot=0 (visually messy but electrically correct).
        if power_net == "+3V3":
            r = 0  # arrow up
        else:
            r = 180  # GND chevron down (default rot 0 is down, but 180 flips it - both OK)
        self.power_at(power_net, px, py, r)


# ============================================================================
# Layout (A3 sheet, units in mm)
# ============================================================================
sch = Sch()

# --- J1: 1×8 male header (mates with carrier J7 socket) ---
# Pin offsets at rot=0: pin1 at (-5.08, +7.62)..pin8 at (-5.08, -10.16)
# Place at (80, 120). Pin tips on LEFT side (x ≈ 74.92).
J1_REF, J1_LIB = "J1", "Connector_Generic:Conn_01x08"
J1_X, J1_Y, J1_ROT = 80.0, 120.0, 0
sch.place(J1_REF, "Conn_01x08",
          J1_LIB,
          "Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical",
          J1_X, J1_Y, J1_ROT)

# J1 pin → net (matches carrier J7 net order)
j1_nets = {
    1: ("GND",      "power"),
    2: ("+3V3",     "power"),
    3: ("SPI_SCK",  "label"),
    4: ("SPI_MOSI", "label"),
    5: ("+3V3",     "power"),
    6: ("SPI_DC",   "label"),
    7: ("SPI_CS",   "label"),
    8: ("BLK_PWM",  "label"),
}
for pin_i, (net, kind) in j1_nets.items():
    if kind == "power":
        sch.power_pin(J1_REF, J1_LIB, J1_X, J1_Y, J1_ROT, pin_i, net)
    else:
        sch.label_pin(J1_REF, J1_LIB, J1_X, J1_Y, J1_ROT, pin_i, net)

# --- J2: 24-pin FPC ---
# Place at (220, 130) with rot=180.
# At rot=0 pin1 is at (-5.08, +27.94); after rot=180 → (+5.08, -27.94)
# So at (220, 130, 180), pin1 tip = (225.08, 102.06), pin24 tip = (225.08, 160.48)
J2_REF, J2_LIB = "J2", "Connector_Generic:Conn_01x24"
J2_X, J2_Y, J2_ROT = 220.0, 130.0, 180
sch.place(J2_REF, "Conn_01x24",
          J2_LIB,
          "Connector_FFC-FPC:Hirose_FH12-24S-0.5SH_1x24-1MP_P0.50mm_Horizontal",
          J2_X, J2_Y, J2_ROT)

# J2 pin → net per the 1.3" panel datasheet pinout:
#   1 LEDA, 2 LEDK, 3 GND, 4 VDD-2.8V, 5 VDDIO, 6 IM1/2, 7 RESET, 8 CS,
#   9 D/C (=SCL in 4-SPI), 10 WR (=D/C in 4-SPI), 11 RD, 12 SDA,
#   13-20 DB0-DB7, 21 TE, 22 NC, 23-24 GND
j2_nets = {
    1:  ("LED_A",    "label"),
    2:  ("LED_K",    "label"),
    3:  ("GND",      "power"),
    4:  ("+3V3",     "power"),
    5:  ("+3V3",     "power"),
    6:  ("IM_HI",    "label"),
    7:  ("RST_HI",   "label"),
    8:  ("SPI_CS",   "label"),
    9:  ("SPI_SCK",  "label"),
    10: ("SPI_DC",   "label"),
    11: ("RD_HI",    "label"),
    12: ("SPI_MOSI", "label"),
    13: ("GND",      "power"),
    14: ("GND",      "power"),
    15: ("GND",      "power"),
    16: ("GND",      "power"),
    17: ("GND",      "power"),
    18: ("GND",      "power"),
    19: ("GND",      "power"),
    20: ("GND",      "power"),
    21: ("TE_NC",    "label"),
    22: ("NC_22",    "label"),
    23: ("GND",      "power"),
    24: ("GND",      "power"),
}
for pin_i, (net, kind) in j2_nets.items():
    if kind == "power":
        sch.power_pin(J2_REF, J2_LIB, J2_X, J2_Y, J2_ROT, pin_i, net)
    else:
        sch.label_pin(J2_REF, J2_LIB, J2_X, J2_Y, J2_ROT, pin_i, net)

# --- R3, R4, R5: 10k pull-ups for IM_HI, RST_HI, RD_HI to +3V3 ---
# Resistor vertical: pin1 (top) at (0, +2.54), pin2 (bottom) at (0, -2.54)
for ref, net, ry in [("R3", "IM_HI", 85.0), ("R4", "RST_HI", 95.0),
                     ("R5", "RD_HI", 105.0)]:
    rx = 145.0
    sch.place(ref, "10k", "Device:R",
              "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal",
              rx, ry, 0)
    # Pin 1 (top, +2.54) → +3V3
    sch.power_pin(ref, "Device:R", rx, ry, 0, 1, "+3V3")
    # Pin 2 (bottom, -2.54) → label
    sch.label_pin(ref, "Device:R", rx, ry, 0, 2, net)

# --- R1: 10k gate pull-down for Q1 ---
# Place at (150, 170). Pin 1 top → label BLK_PWM (gate),
# pin 2 bottom → GND.
R1_X, R1_Y = 150.0, 170.0
sch.place("R1", "10k", "Device:R",
          "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal",
          R1_X, R1_Y, 0)
sch.label_pin("R1", "Device:R", R1_X, R1_Y, 0, 1, "BLK_PWM")
sch.power_pin("R1", "Device:R", R1_X, R1_Y, 0, 2, "GND")

# --- Q1: 2N7000 — gate = BLK_PWM, source = GND, drain = LED_K ---
# Place at (170, 180). Pin offsets (rot 0): S=(-2.54,-2.54), G=(-5.08,0), D=(-2.54,+2.54)
Q1_X, Q1_Y, Q1_ROT = 170.0, 180.0, 0
sch.place("Q1", "2N7000", "Transistor_FET:2N7000",
          "Package_TO_SOT_THT:TO-92_Inline", Q1_X, Q1_Y, Q1_ROT)
sch.power_pin("Q1", "Transistor_FET:2N7000", Q1_X, Q1_Y, Q1_ROT, 1, "GND")
sch.label_pin("Q1", "Transistor_FET:2N7000", Q1_X, Q1_Y, Q1_ROT, 2, "BLK_PWM")
sch.label_pin("Q1", "Transistor_FET:2N7000", Q1_X, Q1_Y, Q1_ROT, 3, "LED_K")

# --- R2: 22 ohm LED current limit ---
# Between +3V3 and LED_A
R2_X, R2_Y = 190.0, 165.0
sch.place("R2", "22", "Device:R",
          "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal",
          R2_X, R2_Y, 0)
sch.power_pin("R2", "Device:R", R2_X, R2_Y, 0, 1, "+3V3")
sch.label_pin("R2", "Device:R", R2_X, R2_Y, 0, 2, "LED_A")

# --- C1: 10µF bulk (polarized) between +3V3 and GND ---
C1_X, C1_Y = 105.0, 140.0
sch.place("C1", "10uF", "Device:C_Polarized_Small",
          "Capacitor_THT:CP_Radial_D5.0mm_P2.50mm",
          C1_X, C1_Y, 0)
sch.power_pin("C1", "Device:C_Polarized_Small", C1_X, C1_Y, 0, 1, "+3V3")
sch.power_pin("C1", "Device:C_Polarized_Small", C1_X, C1_Y, 0, 2, "GND")

# --- C2: 100nF high-freq decoupling ---
C2_X, C2_Y = 115.0, 140.0
sch.place("C2", "100nF", "Device:C",
          "Capacitor_THT:C_Disc_D3.0mm_W1.6mm_P2.50mm",
          C2_X, C2_Y, 0)
sch.power_pin("C2", "Device:C", C2_X, C2_Y, 0, 1, "+3V3")
sch.power_pin("C2", "Device:C", C2_X, C2_Y, 0, 2, "GND")

# --- PWR_FLAGs: declare power nets as driven so ERC stops complaining ---
# Co-locate each PWR_FLAG with an existing power symbol of the same net.
# Snap to grid (S()) for clean placement.
sch.place("#FLG01", "PWR_FLAG", "power:PWR_FLAG", "", S(60.0), S(110.0), 0)
sch.label("+3V3", S(60.0), S(110.0), 0, "input")
sch.place("#FLG02", "PWR_FLAG", "power:PWR_FLAG", "", S(60.0), S(150.0), 0)
sch.label("GND",  S(60.0), S(150.0), 0, "input")

# ============================================================================
# Title block + assembly
# ============================================================================
HEADER = f"""(kicad_sch
\t(version 20260306)
\t(generator "eeschema")
\t(generator_version "10.0")
\t(uuid "{ROOT_SHEET_UUID}")
\t(paper "A3")
\t(title_block
\t\t(title "SupaClock Display Adapter v1")
\t\t(date "2026-05-25")
\t\t(rev "v1.0")
\t\t(company "PUC IEE2913 - Grupo 10")
\t\t(comment 1 "1.3in 240x240 ST7789 panel + 24-pin FPC")
\t\t(comment 2 "Daughterboard for SupaClock_Carrier J7 (1x8 header)")
\t\t(comment 3 "LPKF ProtoMat S64 single-sided")
\t)
\t(lib_symbols
"""

LIB_SYMBOLS = LIB_BLOCK.read_text()

MIDDLE = "\n\t)\n"

BODY = "\n".join(sch.symbols + sch.labels + sch.wires)

FOOTER = """
\t(sheet_instances
\t\t(path "/"
\t\t\t(page "1")
\t\t)
\t)
\t(embedded_fonts no)
)
"""

OUT.write_text(HEADER + LIB_SYMBOLS + MIDDLE + BODY + FOOTER)
print(f"Written: {OUT}")
print(f"  symbols: {len(sch.symbols)}")
print(f"  labels:  {len(sch.labels)}")
print(f"  wires:   {len(sch.wires)}")

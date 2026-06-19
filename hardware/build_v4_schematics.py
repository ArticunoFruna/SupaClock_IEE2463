import uuid
import os

ROOT_SHEET_UUID = "00000000-0000-0000-0000-000000000001"
GRID = 1.27
dir_path = "/home/articunot/Documents/PlatformIO/Projects/SupaClock/hardware/SupaClock_Carrier_v4"

def U() -> str:
    return str(uuid.uuid4())

def S(v: float) -> float:
    return round(v / GRID) * GRID

# Negated Y pin offsets relative to symbol center, mapped by pin name string
PIN_OFFSETS = {
    'Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD': {
        '1': (-21.59, -1.27), '2': (-21.59, 2.54), '3': (-21.59, 6.35), '4': (-21.59, 10.16),
        '5': (-21.59, 13.97), '6': (-21.59, 17.78), '7': (-21.59, 21.59), '8': (21.59, 21.59),
        '9': (21.59, 17.78), '10': (21.59, 13.97), '11': (21.59, 10.16), '12': (21.59, 6.35),
        '13': (21.59, 2.54), '14': (21.59, -1.27), '24': (-12.7, -22.86), '25': (-8.89, -22.86),
        '26': (-5.08, -22.86), '27': (-1.27, -22.86), '28': (2.54, -22.86), '29': (6.35, -22.86),
        '30': (10.16, -22.86), '31': (13.97, -22.86), '32': (-3.81, 31.75), '33': (1.27, 31.75),
        '34': (6.35, 31.75)
    },
    'AD8232_Heart_Rate_Monitor-eagle-import:AD8232': {
        '1': (-20.32, -40.64), '2': (-20.32, -35.56), '3': (-20.32, -30.48), '4': (-20.32, -25.4),
        '5': (-20.32, -15.24), '6': (-20.32, -10.16), '7': (-20.32, -5.08), '8': (-20.32, 5.08),
        '9': (-20.32, 15.24), '10': (-20.32, 25.4), '11': (20.32, 25.4), '12': (20.32, 20.32),
        '13': (20.32, 15.24), '14': (20.32, 5.08), '15': (20.32, -0.0), '16': (20.32, -5.08),
        '17': (20.32, -12.7), '18': (20.32, -22.86), '19': (20.32, -30.48), '20': (20.32, -40.64),
        'PAD': (0.0, 33.02)
    },
    'SparkFun_Lipo_Fuel_Gauge-eagle-import:MAX1704X': {
        '1': (-10.16, -5.08), '2': (-10.16, -2.54), '3': (-10.16, 0.0), '4': (-10.16, 2.54),
        '5': (10.16, 2.54), '6': (10.16, 0.0), '7': (10.16, -2.54), '8': (10.16, -5.08),
        'EP': (-10.16, 2.54)
    },
    'Sensor_Temperature:MAX30205': {
        '1': (-10.16, -5.08), '2': (-10.16, -2.54), '3': (-10.16, 0.0), '4': (-10.16, 2.54),
        '5': (10.16, 2.54), '6': (10.16, 0.0), '7': (10.16, -2.54), '8': (10.16, -5.08)
    },
    'Connector_Generic:Conn_01x02': {
        '1': (-5.08, -0.0), '2': (-5.08, 2.54)
    },
    'Connector_Generic:Conn_01x04': {
        '1': (-5.08, -3.81), '2': (-5.08, -1.27), '3': (-5.08, 1.27), '4': (-5.08, 3.81)
    },
    'Connector_Generic:Conn_01x06': {
        '1': (-5.08, -5.08), '2': (-5.08, -2.54), '3': (-5.08, -0.0), '4': (-5.08, 2.54),
        '5': (-5.08, 5.08), '6': (-5.08, 7.62)
    },
    'Connector_Generic:Conn_01x07': {
        '1': (-5.08, -7.62), '2': (-5.08, -5.08), '3': (-5.08, -2.54), '4': (-5.08, -0.0),
        '5': (-5.08, 2.54), '6': (-5.08, 5.08), '7': (-5.08, 7.62)
    },
    'Connector_Generic:Conn_01x24': {
        str(i+1): (-5.08, -27.94 + i * 2.54) for i in range(24)
    },
    'Device:R': {
        '1': (0.0, -3.81), '2': (0.0, 3.81)
    },
    'Device:C': {
        '1': (0.0, -3.81), '2': (0.0, 3.81)
    },
    'Device:C_Polarized_Small': {
        '1': (0.0, -2.54), '2': (0.0, 2.54)
    },
    'Switch:SW_Push': {
        '1': (-5.08, 0.0), '2': (5.08, 0.0)
    },
    'power:+3V3': {
        '1': (0, 0)
    },
    'power:GND': {
        '1': (0, 0)
    },
    'power:+BATT': {
        '1': (0, 0)
    },
    'power:+5V': {
        '1': (0, 0)
    },
    'Device:LED': {
        '1': (0.0, -2.54), '2': (0.0, 2.54)
    },
    'AD8232_Heart_Rate_Monitor-eagle-import:AUDIO-JACKSMD2': {
        'TIP': (-5.08, 5.08), 'RING': (-5.08, 0), 'SLEEVE': (-5.08, -5.08)
    },
    'Connector_Generic:Conn_01x01': {
        '1': (0.0, 0.0)
    },
    'AD8232_Heart_Rate_Monitor-eagle-import:SOLDERJUMPERTRACE': {
        '1': (-2.54, 0.0), '2': (2.54, 0.0)
    },
    'Transistor_FET:2N7000': {
        '1': (2.54, 5.08), '2': (-5.08, 0.0), '3': (2.54, -5.08)
    },
    'AD8232_Heart_Rate_Monitor-eagle-import:LED-RED0603': {
        'A': (0.0, -2.54), 'C': (0.0, 5.08)
    }
}

def rotate(dx: float, dy: float, rot: int):
    if rot == 0:    return ( dx,  dy)
    if rot == 90:   return (-dy,  dx)
    if rot == 180: return (-dx, -dy)
    if rot == 270:  return ( dy, -dx)
    raise ValueError(rot)

class Sch:
    def __init__(self, proj_name):
        self.proj_name = proj_name
        self.symbols = []
        self.labels = []
        self.wires = []

    def place(self, ref, value, lib_id, footprint, x, y, rot=0, mirror=""):
        x = S(x); y = S(y)
        sym_uuid = U()
        n_pins = len(PIN_OFFSETS[lib_id])
        pin_block = "\n".join(
            f'\t\t(pin "{pin_num}" (uuid "{U()}"))'
            for pin_num in PIN_OFFSETS[lib_id].keys()
        )
        mirror_line = f"\n\t\t(mirror {mirror})" if mirror else ""
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
\t\t\t(effects (font (size 1.27 1.27)) (justify left))
\t\t)
\t\t(property "Value" "{value}"
\t\t\t(at {x + 3.0} {y + 1.5} 0){val_hide}
\t\t\t(effects (font (size 1.27 1.27)) (justify left))
\t\t)
\t\t(property "Footprint" "{footprint}"
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(effects (font (size 1.27 1.27)))
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
\t\t\t(project "{self.proj_name}"
\t\t\t\t(path "/{ROOT_SHEET_UUID}"
\t\t\t\t\t(reference "{ref}")
\t\t\t\t\t(unit 1)
\t\t\t\t)
\t\t\t)
\t\t)
\t)"""
        self.symbols.append(s)
        return sym_uuid

    def pin_xy(self, lib_id, x, y, rot, pin_i):
        pin_key = str(pin_i)
        dx, dy = PIN_OFFSETS[lib_id][pin_key]
        rdx, rdy = rotate(dx, dy, rot)
        return (x + rdx, y + rdy)

    def label(self, text, x, y, rot=0, shape="bidirectional"):
        x = S(x); y = S(y)
        s = f"""\t(global_label "{text}"
\t\t(shape {shape})
\t\t(at {x} {y} {rot})
\t\t(fields_autoplaced yes)
\t\t(effects (font (size 1.27 1.27)) (justify left))
\t\t(uuid "{U()}")
\t\t(property "Intersheetrefs" "${{INTERSHEET_REFS}}"
\t\t\t(at {x} {y} 0)
\t\t\t(hide yes)
\t\t\t(effects (font (size 1.27 1.27)) (justify left))
\t\t)
\t)"""
        self.labels.append(s)

    def power_at(self, lib_id_short, x, y, rot=0):
        lib_id = f"power:{lib_id_short}"
        ref = f"#PWR_{U()[:4]}"
        self.place(ref, lib_id_short, lib_id, "", x, y, rot)

    def label_pin(self, ref, lib_id, x, y, rot, pin_i, net, label_shape="bidirectional"):
        px, py = self.pin_xy(lib_id, x, y, rot, pin_i)
        pin_key = str(pin_i)
        dx, dy = PIN_OFFSETS[lib_id][pin_key]
        rdx, rdy = rotate(dx, dy, rot)
        if abs(rdx) > abs(rdy):
            lr = 180 if rdx < 0 else 0
        else:
            lr = 90 if rdy > 0 else 270
        self.label(net, px, py, lr, label_shape)

    def power_pin(self, ref, lib_id, x, y, rot, pin_i, power_net):
        px, py = self.pin_xy(lib_id, x, y, rot, pin_i)
        if power_net == "GND":
            self.power_at("GND", px, py, 0)
        elif power_net in ["+3V3", "3.3V"]:
            self.power_at("+3V3", px, py, 0)
        elif power_net == "+BATT":
            self.power_at("+BATT", px, py, 0)
        else:
            self.power_at(power_net, px, py, 0)

    def wire(self, x0, y0, x1, y1):
        s = f"""\t(wire
\t\t(pts (xy {x0} {y0}) (xy {x1} {y1}))
\t\t(stroke (width 0) (type default))
\t\t(uuid "{U()}")
\t)"""
        self.wires.append(s)

# Load library symbols block
lib_symbols_block_path = "/home/articunot/Documents/PlatformIO/Projects/SupaClock/hardware/all_extracted_symbols.txt"
with open(lib_symbols_block_path, "r", encoding="utf-8") as f:
    symbols_raw = f.read()

symbols_inside = symbols_raw.strip()
if symbols_inside.startswith("(lib_symbols"):
    symbols_inside = symbols_inside[12:].strip()
if symbols_inside.endswith(")"):
    symbols_inside = symbols_inside[:-1].strip()

# Custom MAX30205 symbol S-expression definition
max30205_def = """(symbol "Sensor_Temperature:MAX30205"
			(pin_names
				(offset 1.016)
			)
			(exclude_from_sim no)
			(in_bom yes)
			(on_board yes)
			(in_pos_files yes)
			(duplicate_pin_numbers_are_jumpers no)
			(property "Reference" "U"
				(at -7.62 7.874 0)
				(show_name no)
				(do_not_autoplace no)
				(effects
					(font
						(size 1.27 1.27)
					)
					(justify left bottom)
				)
			)
			(property "Value" "MAX30205"
				(at -7.62 -7.112 0)
				(show_name no)
				(do_not_autoplace no)
				(effects
					(font
						(size 1.27 1.27)
					)
					(justify left bottom)
				)
			)
			(property "Footprint" "SupaClock_Custom:MAX30205_TDFN8_HandSolder"
				(at 0 0 0)
				(show_name no)
				(do_not_autoplace no)
				(hide yes)
				(effects
					(font
						(size 1.27 1.27)
					)
				)
			)
			(property "Datasheet" "https://datasheets.maximintegrated.com/en/ds/MAX30205.pdf"
				(at 0 0 0)
				(show_name no)
				(do_not_autoplace no)
				(hide yes)
				(effects
					(font
						(size 1.27 1.27)
					)
				)
			)
			(property "Description" "Human Body Temperature Sensor, I2C, TDFN-8"
				(at 0 0 0)
				(show_name no)
				(do_not_autoplace no)
				(hide yes)
				(effects
					(font
						(size 1.27 1.27)
					)
				)
			)
			(symbol "MAX30205_0_1"
				(rectangle
					(start -7.62 7.62)
					(end 7.62 -5.08)
					(stroke
						(width 0.254)
						(type solid)
					)
					(fill
						(type none)
					)
				)
			)
			(symbol "MAX30205_1_1"
				(pin bidirectional line
					(at -10.16 5.08 0)
					(length 2.54)
					(name "SDA"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
					(number "1"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
				)
				(pin input line
					(at -10.16 2.54 0)
					(length 2.54)
					(name "SCL"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
					(number "2"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
				)
				(pin open_collector line
					(at -10.16 0 0)
					(length 2.54)
					(name "OS"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
					(number "3"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
				)
				(pin power_in line
					(at -10.16 -2.54 0)
					(length 2.54)
					(name "GND"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
					(number "4"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
				)
				(pin input line
					(at 10.16 -2.54 180)
					(length 2.54)
					(name "A2"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
					(number "5"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
				)
				(pin input line
					(at 10.16 0 180)
					(length 2.54)
					(name "A1"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
					(number "6"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
				)
				(pin input line
					(at 10.16 2.54 180)
					(length 2.54)
					(name "A0"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
					(number "7"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
				)
				(pin power_in line
					(at 10.16 5.08 180)
					(length 2.54)
					(name "VDD"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
					(number "8"
						(effects
							(font
								(size 1.27 1.27)
							)
						)
					)
				)
			)
		)"""

symbols_inside = symbols_inside + "\n" + max30205_def

def write_schematic(path, sch_builder, title):
    header = f"""(kicad_sch
\t(version 20260306)
\t(generator "eeschema")
\t(generator_version "10.0")
\t(uuid "{ROOT_SHEET_UUID}")
\t(paper "A3")
\t(title_block
\t\t(title "{title}")
\t\t(date "2026-06-16")
\t\t(rev "v4.0")
\t\t(company "PUC IEE2463 - SupaClock")
\t\t(comment 1 "Capacitors and resistors standardized to 0805 SMD")
\t\t(comment 2 "Hand-solderable footprints with back thermal vias")
\t\t(comment 3 "Separated into MainBoard and SensorBoard stacked design")
\t)
\t(lib_symbols
{symbols_inside}
\t)
"""
    body = "\n".join(sch_builder.symbols + sch_builder.labels + sch_builder.wires)
    footer = """
\t(sheet_instances
\t\t(path "/"
\t\t\t(page "1")
\t\t)
\t)
\t(embedded_fonts no)
)
"""
    with open(path, "w", encoding="utf-8") as f:
        f.write(header + body + footer)
    print(f"Written: {path}")

# ============================================================================
# MAIN BOARD
# ============================================================================
m = Sch("SupaClock_MainBoard")

# 1. XIAO ESP32-S3 (U1)
U1_X, U1_Y = 160.0, 100.0
m.place("U1", "XIAO-ESP32-S3", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", "Seeed_Studio_XIAO_Footprints:XIAO-ESP32-S3-SMD", U1_X, U1_Y)

m.label_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "1", "ECG_OUT")
m.label_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "2", "ECG_SDN")
m.label_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "3", "BLK_PWM")
m.label_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "4", "SPI_DC")
m.label_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "5", "I2C_SDA")
m.label_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "6", "I2C_SCL")
m.label_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "7", "BTN_NEXT")
m.label_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "8", "SPI_CS")
m.label_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "9", "SPI_SCK")
m.label_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "10", "BTN_SELECT")
m.label_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "11", "SPI_MOSI")
m.power_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "12", "+3V3")
m.power_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "13", "GND")
m.power_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "32", "+BATT")
m.power_pin("U1", "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-SMD", U1_X, U1_Y, 0, "33", "GND")

# 2. SELECT button (SW1) & NEXT button (SW2)
SW1_X, SW1_Y = 90.0, 60.0
m.place("SW1", "SELECT", "Switch:SW_Push", "SupaClock_Custom:SW_Tactile_Side_3.5x7.8mm", SW1_X, SW1_Y)
m.label_pin("SW1", "Switch:SW_Push", SW1_X, SW1_Y, 0, "1", "BTN_SELECT")
m.power_pin("SW1", "Switch:SW_Push", SW1_X, SW1_Y, 0, "2", "GND")

C4_X, C4_Y = 70.0, 60.0
m.place("C4", "100nF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", C4_X, C4_Y)
m.label_pin("C4", "Device:C", C4_X, C4_Y, 0, "1", "BTN_SELECT")
m.power_pin("C4", "Device:C", C4_X, C4_Y, 0, "2", "GND")

R3_X, R3_Y = 70.0, 45.0
m.place("R3", "10k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", R3_X, R3_Y)
m.label_pin("R3", "Device:R", R3_X, R3_Y, 0, "1", "BTN_SELECT")
m.power_pin("R3", "Device:R", R3_X, R3_Y, 0, "2", "+3V3")

SW2_X, SW2_Y = 90.0, 90.0
m.place("SW2", "NEXT", "Switch:SW_Push", "SupaClock_Custom:SW_Tactile_Side_3.5x7.8mm", SW2_X, SW2_Y)
m.label_pin("SW2", "Switch:SW_Push", SW2_X, SW2_Y, 0, "1", "BTN_NEXT")
m.power_pin("SW2", "Switch:SW_Push", SW2_X, SW2_Y, 0, "2", "GND")

C5_X, C5_Y = 70.0, 90.0
m.place("C5", "100nF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", C5_X, C5_Y)
m.label_pin("C5", "Device:C", C5_X, C5_Y, 0, "1", "BTN_NEXT")
m.power_pin("C5", "Device:C", C5_X, C5_Y, 0, "2", "GND")

R4_X, R4_Y = 70.0, 75.0
m.place("R4", "10k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", R4_X, R4_Y)
m.label_pin("R4", "Device:R", R4_X, R4_Y, 0, "1", "BTN_NEXT")
m.power_pin("R4", "Device:R", R4_X, R4_Y, 0, "2", "+3V3")

# 3. I2C pull-ups R1, R2 (4.7k)
R1_X, R1_Y = 70.0, 110.0
m.place("R1", "4.7k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", R1_X, R1_Y)
m.label_pin("R1", "Device:R", R1_X, R1_Y, 0, "1", "I2C_SDA")
m.power_pin("R1", "Device:R", R1_X, R1_Y, 0, "2", "+3V3")

R2_X, R2_Y = 70.0, 125.0
m.place("R2", "4.7k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", R2_X, R2_Y)
m.label_pin("R2", "Device:R", R2_X, R2_Y, 0, "1", "I2C_SCL")
m.power_pin("R2", "Device:R", R2_X, R2_Y, 0, "2", "+3V3")

# 4. MAX17048 Fuel Gauge (U2) at (260, 60)
U2_X, U2_Y = 260.0, 60.0
m.place("U2", "MAX17048", "SparkFun_Lipo_Fuel_Gauge-eagle-import:MAX1704X", "SupaClock_Custom:MAX17048_WDFN8_HandSolder", U2_X, U2_Y)
m.power_pin("U2", "SparkFun_Lipo_Fuel_Gauge-eagle-import:MAX1704X", U2_X, U2_Y, 0, "1", "GND") # CTG
m.label_pin("U2", "SparkFun_Lipo_Fuel_Gauge-eagle-import:MAX1704X", U2_X, U2_Y, 0, "2", "CELL_FILT")
m.label_pin("U2", "SparkFun_Lipo_Fuel_Gauge-eagle-import:MAX1704X", U2_X, U2_Y, 0, "3", "VDD_FILT")
m.power_pin("U2", "SparkFun_Lipo_Fuel_Gauge-eagle-import:MAX1704X", U2_X, U2_Y, 0, "4", "GND")
m.label_pin("U2", "SparkFun_Lipo_Fuel_Gauge-eagle-import:MAX1704X", U2_X, U2_Y, 0, "5", "FG_ALT")
m.label_pin("U2", "SparkFun_Lipo_Fuel_Gauge-eagle-import:MAX1704X", U2_X, U2_Y, 0, "6", "FG_QST")
m.label_pin("U2", "SparkFun_Lipo_Fuel_Gauge-eagle-import:MAX1704X", U2_X, U2_Y, 0, "7", "I2C_SCL")
m.label_pin("U2", "SparkFun_Lipo_Fuel_Gauge-eagle-import:MAX1704X", U2_X, U2_Y, 0, "8", "I2C_SDA")
m.power_pin("U2", "SparkFun_Lipo_Fuel_Gauge-eagle-import:MAX1704X", U2_X, U2_Y, 0, "EP", "GND")

# R_FG1 (1k 0805)
RFG1_X, RFG1_Y = 230.0, 35.0
m.place("R_FG1", "1k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", RFG1_X, RFG1_Y)
m.label_pin("R_FG1", "Device:R", RFG1_X, RFG1_Y, 0, "1", "CELL_FILT")
m.power_pin("R_FG1", "Device:R", RFG1_X, RFG1_Y, 0, "2", "+BATT")

# C_FG1 (1uF 0805)
CFG1_X, CFG1_Y = 230.0, 50.0
m.place("C_FG1", "1uF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", CFG1_X, CFG1_Y)
m.label_pin("C_FG1", "Device:C", CFG1_X, CFG1_Y, 0, "1", "CELL_FILT")
m.power_pin("C_FG1", "Device:C", CFG1_X, CFG1_Y, 0, "2", "GND")

# R_FG2 (100 ohm 0805)
RFG2_X, RFG2_Y = 245.0, 35.0
m.place("R_FG2", "100", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", RFG2_X, RFG2_Y)
m.label_pin("R_FG2", "Device:R", RFG2_X, RFG2_Y, 0, "1", "VDD_FILT")
m.power_pin("R_FG2", "Device:R", RFG2_X, RFG2_Y, 0, "2", "+BATT")

# C_FG2 (10nF 0805)
CFG2_X, CFG2_Y = 245.0, 50.0
m.place("C_FG2", "10nF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", CFG2_X, CFG2_Y)
m.label_pin("C_FG2", "Device:C", CFG2_X, CFG2_Y, 0, "1", "VDD_FILT")
m.power_pin("C_FG2", "Device:C", CFG2_X, CFG2_Y, 0, "2", "GND")

# R_FG_ALT (10k 0805 pull-up on ALT)
RFGALT_X, RFGALT_Y = 290.0, 45.0
m.place("R_FG_ALT", "10k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", RFGALT_X, RFGALT_Y)
m.label_pin("R_FG_ALT", "Device:R", RFGALT_X, RFGALT_Y, 0, "1", "FG_ALT")
m.power_pin("R_FG_ALT", "Device:R", RFGALT_X, RFGALT_Y, 0, "2", "+3V3")

# R_FG_QST (10k 0805 pull-down on QST)
RFGQST_X, RFGQST_Y = 290.0, 65.0
m.place("R_FG_QST", "10k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", RFGQST_X, RFGQST_Y)
m.label_pin("R_FG_QST", "Device:R", RFGQST_X, RFGQST_Y, 0, "1", "FG_QST")
m.power_pin("R_FG_QST", "Device:R", RFGQST_X, RFGQST_Y, 0, "2", "GND")

# 5. JST Battery Connector (J_BAT) at (260, 110)
JBAT_X, JBAT_Y = 260.0, 110.0
m.place("J_BAT", "CONN_BATT", "Connector_Generic:Conn_01x02", "Connector_JST:JST_PH_B2B-PH-K_1x02_P2.00mm_Vertical", JBAT_X, JBAT_Y)
m.power_pin("J_BAT", "Connector_Generic:Conn_01x02", JBAT_X, JBAT_Y, 0, "1", "+BATT")
m.power_pin("J_BAT", "Connector_Generic:Conn_01x02", JBAT_X, JBAT_Y, 0, "2", "GND")

# Decoupling Cap C2 (10uF 0805) for battery input
C2_X, C2_Y = 280.0, 110.0
m.place("C2", "10uF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", C2_X, C2_Y)
m.power_pin("C2", "Device:C", C2_X, C2_Y, 0, "1", "+BATT")
m.power_pin("C2", "Device:C", C2_X, C2_Y, 0, "2", "GND")

# 6. ST7789 Display FPC Connector (J_FPC) at (170, 220)
JFPC_X, JFPC_Y = 170.0, 220.0
m.place("J_FPC", "ST7789_24P", "Connector_Generic:Conn_01x24", "Connector_FFC-FPC:Hirose_FH12-24S-0.5SH_1x24-1MP_P0.50mm_Horizontal", JFPC_X, JFPC_Y)
fpc_nets = {
    1: "LED_A", 2: "LED_K", 3: "GND", 4: "+3V3", 5: "+3V3",
    6: "IM_HI", 7: "RST_HI", 8: "SPI_CS", 9: "SPI_SCK", 10: "SPI_DC",
    11: "RD_HI", 12: "SPI_MOSI", 13: "GND", 14: "GND", 15: "GND",
    16: "GND", 17: "GND", 18: "GND", 19: "GND", 20: "GND",
    21: "TE_NC", 22: "NC_22", 23: "GND", 24: "GND"
}
for pin_i, net in fpc_nets.items():
    if net == "GND":
        m.power_pin("J_FPC", "Connector_Generic:Conn_01x24", JFPC_X, JFPC_Y, 0, str(pin_i), "GND")
    elif net == "+3V3":
        m.power_pin("J_FPC", "Connector_Generic:Conn_01x24", JFPC_X, JFPC_Y, 0, str(pin_i), "+3V3")
    else:
        m.label_pin("J_FPC", "Connector_Generic:Conn_01x24", JFPC_X, JFPC_Y, 0, str(pin_i), net)

# Q1 (2N7002, SOT-23) for Backlight logic
Q1_X, Q1_Y = 100.0, 220.0
m.place("Q1", "2N7002", "Transistor_FET:2N7000", "Package_TO_SOT_SMD:SOT-23_Handsoldering", Q1_X, Q1_Y)
m.power_pin("Q1", "Transistor_FET:2N7000", Q1_X, Q1_Y, 0, "1", "GND")       # Source
m.label_pin("Q1", "Transistor_FET:2N7000", Q1_X, Q1_Y, 0, "2", "BLK_PWM")    # Gate
m.label_pin("Q1", "Transistor_FET:2N7000", Q1_X, Q1_Y, 0, "3", "LED_K")      # Drain

# R_GATE (10k 0805) pull-down for Gate of Q1
RGATE_X, RGATE_Y = 100.0, 195.0
m.place("R_GATE", "10k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", RGATE_X, RGATE_Y)
m.label_pin("R_GATE", "Device:R", RGATE_X, RGATE_Y, 0, "1", "BLK_PWM")
m.power_pin("R_GATE", "Device:R", RGATE_X, RGATE_Y, 0, "2", "GND")

# R_LEDA (22R 0805) current limiting resistor
RLEDA_X, RLEDA_Y = 120.0, 195.0
m.place("R_LEDA", "22", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", RLEDA_X, RLEDA_Y)
m.power_pin("R_LEDA", "Device:R", RLEDA_X, RLEDA_Y, 0, "1", "+3V3")
m.label_pin("R_LEDA", "Device:R", RLEDA_X, RLEDA_Y, 0, "2", "LED_A")

# FPC Configuration Pull-ups: R_IM (10k), R_RD (10k), R_RST (10k)
# R_IM
RIM_X, RIM_Y = 120.0, 240.0
m.place("R_IM", "10k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", RIM_X, RIM_Y)
m.label_pin("R_IM", "Device:R", RIM_X, RIM_Y, 0, "1", "IM_HI")
m.power_pin("R_IM", "Device:R", RIM_X, RIM_Y, 0, "2", "+3V3")

# R_RD
RRD_X, RRD_Y = 135.0, 240.0
m.place("R_RD", "10k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", RRD_X, RRD_Y)
m.label_pin("R_RD", "Device:R", RRD_X, RRD_Y, 0, "1", "RD_HI")
m.power_pin("R_RD", "Device:R", RRD_X, RRD_Y, 0, "2", "+3V3")

# R_RST
RRST_X, RRST_Y = 150.0, 240.0
m.place("R_RST", "10k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", RRST_X, RRST_Y)
m.label_pin("R_RST", "Device:R", RRST_X, RRST_Y, 0, "1", "RST_HI")
m.power_pin("R_RST", "Device:R", RRST_X, RRST_Y, 0, "2", "+3V3")

# C_RST (100nF 0805) auto-reset capacitor
CRST_X, CRST_Y = 150.0, 260.0
m.place("C_RST", "100nF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", CRST_X, CRST_Y)
m.label_pin("C_RST", "Device:C", CRST_X, CRST_Y, 0, "1", "RST_HI")
m.power_pin("C_RST", "Device:C", CRST_X, CRST_Y, 0, "2", "GND")

# C_DISP1 (10uF) & C_DISP2 (100nF) for display decoupling
CDISP1_X, CDISP1_Y = 120.0, 275.0
m.place("C_DISP1", "10uF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", CDISP1_X, CDISP1_Y)
m.power_pin("C_DISP1", "Device:C", CDISP1_X, CDISP1_Y, 0, "1", "+3V3")
m.power_pin("C_DISP1", "Device:C", CDISP1_X, CDISP1_Y, 0, "2", "GND")

CDISP2_X, CDISP2_Y = 135.0, 275.0
m.place("C_DISP2", "100nF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", CDISP2_X, CDISP2_Y)
m.power_pin("C_DISP2", "Device:C", CDISP2_X, CDISP2_Y, 0, "1", "+3V3")
m.power_pin("C_DISP2", "Device:C", CDISP2_X, CDISP2_Y, 0, "2", "GND")

# 7. BMI160 IMU castellated breakout J_IMU at (340, 60)
JIMU_X, JIMU_Y = 340.0, 60.0
m.place("J_IMU", "GY-BMI160", "Connector_Generic:Conn_01x07", "Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Vertical", JIMU_X, JIMU_Y)
m.power_pin("J_IMU", "Connector_Generic:Conn_01x07", JIMU_X, JIMU_Y, 0, "1", "+3V3")   # VCC
m.label_pin("J_IMU", "Connector_Generic:Conn_01x07", JIMU_X, JIMU_Y, 0, "2", "IMU_INT1")
m.power_pin("J_IMU", "Connector_Generic:Conn_01x07", JIMU_X, JIMU_Y, 0, "3", "GND")   # GND
m.label_pin("J_IMU", "Connector_Generic:Conn_01x07", JIMU_X, JIMU_Y, 0, "4", "I2C_SCL")
m.label_pin("J_IMU", "Connector_Generic:Conn_01x07", JIMU_X, JIMU_Y, 0, "5", "I2C_SDA")
m.power_pin("J_IMU", "Connector_Generic:Conn_01x07", JIMU_X, JIMU_Y, 0, "6", "+3V3")   # SA0 tied to 3.3V
m.power_pin("J_IMU", "Connector_Generic:Conn_01x07", JIMU_X, JIMU_Y, 0, "7", "GND")   # Tie last pin to GND

# 8. 6-pin Sensor Board interface connector J_CONN_SENSOR at (340, 120)
JCONN_X, JCONN_Y = 340.0, 120.0
m.place("J_CONN_SENSOR", "SensorBoard_Conn", "Connector_Generic:Conn_01x06", "Connector_JST:JST_PH_B6B-PH-K_1x06_P2.00mm_Vertical", JCONN_X, JCONN_Y)
m.power_pin("J_CONN_SENSOR", "Connector_Generic:Conn_01x06", JCONN_X, JCONN_Y, 0, "1", "+3V3")
m.power_pin("J_CONN_SENSOR", "Connector_Generic:Conn_01x06", JCONN_X, JCONN_Y, 0, "2", "GND")
m.label_pin("J_CONN_SENSOR", "Connector_Generic:Conn_01x06", JCONN_X, JCONN_Y, 0, "3", "I2C_SDA")
m.label_pin("J_CONN_SENSOR", "Connector_Generic:Conn_01x06", JCONN_X, JCONN_Y, 0, "4", "I2C_SCL")
m.label_pin("J_CONN_SENSOR", "Connector_Generic:Conn_01x06", JCONN_X, JCONN_Y, 0, "5", "ECG_OUT")
m.label_pin("J_CONN_SENSOR", "Connector_Generic:Conn_01x06", JCONN_X, JCONN_Y, 0, "6", "ECG_SDN")


# Write MainBoard Schematic
write_schematic(os.path.join(dir_path, "SupaClock_MainBoard/SupaClock_MainBoard.kicad_sch"), m, "SupaClock MainBoard v4")

# ============================================================================
# SENSOR BOARD
# ============================================================================
s = Sch("SupaClock_SensorBoard")

# 1. 6-pin interface connector J_CONN_SENSOR at (60, 60)
JCONNS_X, JCONNS_Y = 60.0, 60.0
s.place("J_CONN_SENSOR", "MainBoard_Conn", "Connector_Generic:Conn_01x06", "Connector_JST:JST_PH_B6B-PH-K_1x06_P2.00mm_Vertical", JCONNS_X, JCONNS_Y)
s.power_pin("J_CONN_SENSOR", "Connector_Generic:Conn_01x06", JCONNS_X, JCONNS_Y, 0, "1", "+3V3")
s.power_pin("J_CONN_SENSOR", "Connector_Generic:Conn_01x06", JCONNS_X, JCONNS_Y, 0, "2", "GND")
s.label_pin("J_CONN_SENSOR", "Connector_Generic:Conn_01x06", JCONNS_X, JCONNS_Y, 0, "3", "I2C_SDA")
s.label_pin("J_CONN_SENSOR", "Connector_Generic:Conn_01x06", JCONNS_X, JCONNS_Y, 0, "4", "I2C_SCL")
s.label_pin("J_CONN_SENSOR", "Connector_Generic:Conn_01x06", JCONNS_X, JCONNS_Y, 0, "5", "ECG_OUT")
s.label_pin("J_CONN_SENSOR", "Connector_Generic:Conn_01x06", JCONNS_X, JCONNS_Y, 0, "6", "ECG_SDN")

# 2. MAX30205 Temperature Sensor (U3) at (120, 60)
U3_X, U3_Y = 120.0, 60.0
s.place("U3", "MAX30205", "Sensor_Temperature:MAX30205", "SupaClock_Custom:MAX30205_TDFN8_HandSolder", U3_X, U3_Y)
s.label_pin("U3", "Sensor_Temperature:MAX30205", U3_X, U3_Y, 0, "1", "I2C_SDA")
s.label_pin("U3", "Sensor_Temperature:MAX30205", U3_X, U3_Y, 0, "2", "I2C_SCL")
s.label_pin("U3", "Sensor_Temperature:MAX30205", U3_X, U3_Y, 0, "3", "TEMP_OS")
s.power_pin("U3", "Sensor_Temperature:MAX30205", U3_X, U3_Y, 0, "4", "GND")
s.power_pin("U3", "Sensor_Temperature:MAX30205", U3_X, U3_Y, 0, "5", "GND") # A2
s.power_pin("U3", "Sensor_Temperature:MAX30205", U3_X, U3_Y, 0, "6", "GND") # A1
s.power_pin("U3", "Sensor_Temperature:MAX30205", U3_X, U3_Y, 0, "7", "GND") # A0
s.power_pin("U3", "Sensor_Temperature:MAX30205", U3_X, U3_Y, 0, "8", "+3V3") # VDD

# C_TEMP (0.1uF 0805) decoupling for MAX30205
CTEMP_X, CTEMP_Y = 150.0, 60.0
s.place("C_TEMP", "0.1uF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", CTEMP_X, CTEMP_Y)
s.power_pin("C_TEMP", "Device:C", CTEMP_X, CTEMP_Y, 0, "1", "+3V3")
s.power_pin("C_TEMP", "Device:C", CTEMP_X, CTEMP_Y, 0, "2", "GND")

# 3. MAX30102 Pulse Sensor breakout module J_PULSE at (60, 120)
JPULSE_X, JPULSE_Y = 60.0, 120.0
s.place("J_PULSE", "MAX30102", "Connector_Generic:Conn_01x04", "SupaClock_Custom:MAX30102_Castellated_1x4", JPULSE_X, JPULSE_Y)
s.power_pin("J_PULSE", "Connector_Generic:Conn_01x04", JPULSE_X, JPULSE_Y, 0, "1", "+3V3")
s.label_pin("J_PULSE", "Connector_Generic:Conn_01x04", JPULSE_X, JPULSE_Y, 0, "2", "I2C_SDA")
s.label_pin("J_PULSE", "Connector_Generic:Conn_01x04", JPULSE_X, JPULSE_Y, 0, "3", "I2C_SCL")
s.power_pin("J_PULSE", "Connector_Generic:Conn_01x04", JPULSE_X, JPULSE_Y, 0, "4", "GND")

# 4. AD8232 ECG Circuit
U2S_X, U2S_Y = 220.0, 150.0
s.place("U2", "AD8232", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", "SupaClock_Custom:AD8232_LFCSP20_HandSolder", U2S_X, U2S_Y)

s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "1", "HPDRIVE")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "2", "IN_P")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "3", "IN_N")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "4", "RLDFB")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "5", "RLD_OUT")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "6", "SW_FILT")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "7", "OPAMP_IN_P")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "8", "REFOUT")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "9", "OPAMP_IN_N")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "10", "ECG_OUT")
# # s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "11", "LO_MINUS") # Removed per user request # Removed per user request
# # s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "12", "LO_PLUS") # Removed per user request # Removed per user request
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "13", "ECG_SDN")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "14", "AC_DC_SEL")
s.power_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "15", "+3V3")
s.power_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "16", "GND")
s.power_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "17", "+3V3")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "18", "REFIN")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "19", "IAOUT")
s.label_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "20", "HPSENSE")
s.power_pin("U2", "AD8232_Heart_Rate_Monitor-eagle-import:AD8232", U2S_X, U2S_Y, 0, "PAD", "GND")

# R3 (180k) from LA to IN_P
s.place("R3", "180k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 280.0, 30.0)
s.label_pin("R3", "Device:R", 280.0, 30.0, 0, "1", "LA")
s.label_pin("R3", "Device:R", 280.0, 30.0, 0, "2", "IN_P")

# R4 (180k) from RA to IN_N
s.place("R4", "180k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 280.0, 45.0)
s.label_pin("R4", "Device:R", 280.0, 45.0, 0, "1", "RA")
s.label_pin("R4", "Device:R", 280.0, 45.0, 0, "2", "IN_N")

# R1 (10M) bias resistor from LA to BIAS_V
s.place("R1", "10M", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 280.0, 60.0)
s.label_pin("R1", "Device:R", 280.0, 60.0, 0, "1", "LA")
s.label_pin("R1", "Device:R", 280.0, 60.0, 0, "2", "BIAS_V")

# R2 (10M) bias resistor from RA to BIAS_V
s.place("R2", "10M", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 280.0, 75.0)
s.label_pin("R2", "Device:R", 280.0, 75.0, 0, "1", "RA")
s.label_pin("R2", "Device:R", 280.0, 75.0, 0, "2", "BIAS_V")

# R17 (0R) from BIAS_V to +3V3
s.place("R17", "0", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 280.0, 90.0)
s.label_pin("R17", "Device:R", 280.0, 90.0, 0, "1", "BIAS_V")
s.power_pin("R17", "Device:R", 280.0, 90.0, 0, "2", "+3V3")

# R15 (10k) pull-up on SDN
s.place("R15", "10k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 280.0, 105.0)
s.label_pin("R15", "Device:R", 280.0, 105.0, 0, "1", "ECG_SDN")
s.power_pin("R15", "Device:R", 280.0, 105.0, 0, "2", "+3V3")

# REFIN Voltage divider: R10 (10M), R14 (10M), C7 (0.1uF)
# R10
s.place("R10", "10M", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 280.0, 120.0)
s.label_pin("R10", "Device:R", 280.0, 120.0, 0, "1", "REFIN")
s.power_pin("R10", "Device:R", 280.0, 120.0, 0, "2", "+3V3")

# R14
s.place("R14", "10M", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 280.0, 135.0)
s.label_pin("R14", "Device:R", 280.0, 135.0, 0, "1", "REFIN")
s.power_pin("R14", "Device:R", 280.0, 135.0, 0, "2", "GND")

# C7
s.place("C7", "0.1uF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", 280.0, 150.0)
s.label_pin("C7", "Device:C", 280.0, 150.0, 0, "1", "REFIN")
s.power_pin("C7", "Device:C", 280.0, 150.0, 0, "2", "GND")

# High-pass filter components:
# C4 (0.33uF) between HPDRIVE and HPSENSE
s.place("C4", "0.33uF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", 320.0, 30.0)
s.label_pin("C4", "Device:C", 320.0, 30.0, 0, "1", "HPDRIVE")
s.label_pin("C4", "Device:C", 320.0, 30.0, 0, "2", "HPSENSE")

# R11 (10M) from HPSENSE to FILT_14
s.place("R11", "10M", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 320.0, 45.0)
s.label_pin("R11", "Device:R", 320.0, 45.0, 0, "1", "HPSENSE")
s.label_pin("R11", "Device:R", 320.0, 45.0, 0, "2", "FILT_14")

# R13 (10M) from IAOUT to FILT_14
s.place("R13", "10M", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 320.0, 60.0)
s.label_pin("R13", "Device:R", 320.0, 60.0, 0, "1", "IAOUT")
s.label_pin("R13", "Device:R", 320.0, 60.0, 0, "2", "FILT_14")

# R12 (1.4M) from FILT_14 to SW_FILT
s.place("R12", "1.4M", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 320.0, 75.0)
s.label_pin("R12", "Device:R", 320.0, 75.0, 0, "1", "FILT_14")
s.label_pin("R12", "Device:R", 320.0, 75.0, 0, "2", "SW_FILT")

# C6 (0.33uF) from SW_FILT to REFOUT
s.place("C6", "0.33uF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", 320.0, 90.0)
s.label_pin("C6", "Device:C", 320.0, 90.0, 0, "1", "SW_FILT")
s.label_pin("C6", "Device:C", 320.0, 90.0, 0, "2", "REFOUT")

# Right Leg Drive (RLD):
# C2 (1nF) between RLDFB and RLD_OUT
s.place("C2", "1nF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", 320.0, 105.0)
s.label_pin("C2", "Device:C", 320.0, 105.0, 0, "1", "RLDFB")
s.label_pin("C2", "Device:C", 320.0, 105.0, 0, "2", "RLD_OUT")

# R5 (360k) from RLD_OUT to RL
s.place("R5", "360k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 320.0, 120.0)
s.label_pin("R5", "Device:R", 320.0, 120.0, 0, "1", "RLD_OUT")
s.label_pin("R5", "Device:R", 320.0, 120.0, 0, "2", "RL")

# Output low-pass filter:
# R6 (360k) from IAOUT to FILT_9
s.place("R6", "360k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 360.0, 30.0)
s.label_pin("R6", "Device:R", 360.0, 30.0, 0, "1", "IAOUT")
s.label_pin("R6", "Device:R", 360.0, 30.0, 0, "2", "FILT_9")

# C1 (1.5nF) from FILT_9 to ECG_OUT
s.place("C1", "1.5nF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", 360.0, 45.0)
s.label_pin("C1", "Device:C", 360.0, 45.0, 0, "1", "FILT_9")
s.label_pin("C1", "Device:C", 360.0, 45.0, 0, "2", "ECG_OUT")

# R7 (1M) from FILT_9 to OPAMP_IN_P
s.place("R7", "1M", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 360.0, 60.0)
s.label_pin("R7", "Device:R", 360.0, 60.0, 0, "1", "FILT_9")
s.label_pin("R7", "Device:R", 360.0, 60.0, 0, "2", "OPAMP_IN_P")

# C3 (10nF) from OPAMP_IN_P to REFOUT
s.place("C3", "10nF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", 360.0, 75.0)
s.label_pin("C3", "Device:C", 360.0, 75.0, 0, "1", "OPAMP_IN_P")
s.label_pin("C3", "Device:C", 360.0, 75.0, 0, "2", "REFOUT")

# R8 (100k) from REFOUT to OPAMP_IN_N
s.place("R8", "100k", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 360.0, 90.0)
s.label_pin("R8", "Device:R", 360.0, 90.0, 0, "1", "REFOUT")
s.label_pin("R8", "Device:R", 360.0, 90.0, 0, "2", "OPAMP_IN_N")

# R9 (1M) from OPAMP_IN_N to ECG_OUT
s.place("R9", "1M", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 360.0, 105.0)
s.label_pin("R9", "Device:R", 360.0, 105.0, 0, "1", "OPAMP_IN_N")
s.label_pin("R9", "Device:R", 360.0, 105.0, 0, "2", "ECG_OUT")

# C5 (0.1uF) bypass cap for AD8232 power
s.place("C5", "0.1uF", "Device:C", "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder", 360.0, 120.0)
s.power_pin("C5", "Device:C", 360.0, 120.0, 0, "1", "+3V3")
s.power_pin("C5", "Device:C", 360.0, 120.0, 0, "2", "GND")

# Leads-off select mode: tie AC_DC_SEL (pin 14) to GND via R19 (0R)
s.place("R19", "0", "Device:R", "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder", 360.0, 135.0)
s.label_pin("R19", "Device:R", 360.0, 135.0, 0, "1", "AC_DC_SEL")
s.power_pin("R19", "Device:R", 360.0, 135.0, 0, "2", "GND")

# Debug LED (D1), R16 and SJ4 removed per user request to keep only core ECG, jack, and electrode pads.


# 5. Jack and direct electrode contact pads
# 3.5mm Jack (JP2) at (60, 200)
s.place("JP2", "ECG_JACK", "AD8232_Heart_Rate_Monitor-eagle-import:AUDIO-JACKSMD2", "Connector_Audio:Jack_3.5mm_CUI_SJ-3523-SMT_Horizontal", 60.0, 200.0)
s.label_pin("JP2", "AD8232_Heart_Rate_Monitor-eagle-import:AUDIO-JACKSMD2", 60.0, 200.0, 0, "TIP", "RL")
s.label_pin("JP2", "AD8232_Heart_Rate_Monitor-eagle-import:AUDIO-JACKSMD2", 60.0, 200.0, 0, "RING", "LA")
s.label_pin("JP2", "AD8232_Heart_Rate_Monitor-eagle-import:AUDIO-JACKSMD2", 60.0, 200.0, 0, "SLEEVE", "RA")

# M3 electrode pads: J_LA, J_RA, J_RL at (60, 240)
s.place("J_LA", "ECG_LA_pad_M3", "Connector_Generic:Conn_01x01", "SupaClock_Custom:M3_Electrode_Pad", 60.0, 240.0)
s.label_pin("J_LA", "Connector_Generic:Conn_01x01", 60.0, 240.0, 0, "1", "LA")

s.place("J_RA", "ECG_RA_pad_M3", "Connector_Generic:Conn_01x01", "SupaClock_Custom:M3_Electrode_Pad", 60.0, 255.0)
s.label_pin("J_RA", "Connector_Generic:Conn_01x01", 60.0, 255.0, 0, "1", "RA")

s.place("J_RL", "ECG_RL_pad_M3", "Connector_Generic:Conn_01x01", "SupaClock_Custom:M3_Electrode_Pad", 60.0, 270.0)
s.label_pin("J_RL", "Connector_Generic:Conn_01x01", 60.0, 270.0, 0, "1", "RL")

# Write SensorBoard Schematic
write_schematic(os.path.join(dir_path, "SupaClock_SensorBoard/SupaClock_SensorBoard.kicad_sch"), s, "SupaClock SensorBoard v4")

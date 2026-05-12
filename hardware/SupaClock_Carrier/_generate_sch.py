#!/usr/bin/env python3
"""
Generador de SupaClock_Carrier.kicad_sch (KiCad 9.0.9, formato 20231120).

Extrae los lib_symbols necesarios desde /usr/share/kicad/symbols/ y los embebe
en el archivo de esquema. Coloca instancias de símbolo en una grilla razonable
y conecta los pines mediante stub-wires + net labels locales.

No intenta producir un esquema "bonito" — el objetivo es que abra en KiCad sin
errores, con todas las conexiones correctas y reasignables visualmente luego.

Carrier v1: XIAO ESP32-S3 + módulos through-hole via pin headers.
"""

import re
import uuid
from pathlib import Path

KICAD_SYM_DIR = Path("/usr/share/kicad/symbols")
DOCS_DIR = Path("/home/articunot/Documents/PlatformIO/Projects/SupaClock/docs")
SEEED_SYM = DOCS_DIR / "XIAO_Series_SCH_Symbols" / "Seeed_Studio_XIAO_Series.kicad_sym"
OUT_FILE = Path(__file__).parent / "SupaClock_Carrier.kicad_sch"

# Mapeo lib_name -> archivo .kicad_sym
LIB_SOURCES = {
    "Connector_Generic": KICAD_SYM_DIR / "Connector_Generic.kicad_sym",
    "Device": KICAD_SYM_DIR / "Device.kicad_sym",
    "Switch": KICAD_SYM_DIR / "Switch.kicad_sym",
    "power": KICAD_SYM_DIR / "power.kicad_sym",
    "Seeed_Studio_XIAO_Series": SEEED_SYM,
}

# ----------------------------------------------------------------------
# Símbolos requeridos: (lib_name, symbol_name)
# ----------------------------------------------------------------------
NEEDED_SYMBOLS = [
    ("Connector_Generic", "Conn_01x01"),
    ("Connector_Generic", "Conn_01x02"),
    ("Connector_Generic", "Conn_01x03"),
    ("Connector_Generic", "Conn_01x04"),
    ("Connector_Generic", "Conn_01x06"),
    ("Connector_Generic", "Conn_01x07"),
    ("Connector_Generic", "Conn_01x08"),
    ("Device", "R"),
    ("Device", "C"),
    ("Switch", "SW_Push"),
    ("power", "GND"),
    ("power", "+3V3"),
    ("power", "+5V"),
    ("power", "+BATT"),
    ("Seeed_Studio_XIAO_Series", "XIAO-ESP32-S3-DIP"),
]


def extract_symbol(lib: str, sym: str) -> str:
    """Extrae un (symbol ...) completo desde una librería .kicad_sym."""
    src = LIB_SOURCES[lib].read_text()
    # Buscar línea que abre el símbolo
    pattern = re.compile(
        r'^\t\(symbol "' + re.escape(sym) + r'"\n', re.MULTILINE
    )
    m = pattern.search(src)
    if not m:
        raise ValueError(f"Symbol {lib}:{sym} not found")
    start = m.start()
    # Balance de paréntesis a partir del start
    depth = 0
    i = start
    while i < len(src):
        c = src[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                # Incluir el ) y el newline
                end = i + 1
                # Cambiar el lib_id embebido: KiCad espera "lib:sym"
                text = src[start:end]
                # Renombrar de "Conn_01x07" a "Connector_Generic:Conn_01x07"
                text = text.replace(
                    f'\t(symbol "{sym}"\n',
                    f'\t(symbol "{lib}:{sym}"\n',
                    1,
                )
                return text + "\n"
        i += 1
    raise ValueError(f"Unbalanced parens in {lib}:{sym}")


def uid() -> str:
    return str(uuid.uuid4())


# ----------------------------------------------------------------------
# Layout: posiciones en mm (KiCad usa unidades de 1mm en formato 20231120)
# Hoja A3 (420x297 mm). Grilla de 5.08 mm (0.2") típica de KiCad.
# ----------------------------------------------------------------------
PAGE = "A3"

# Coordenadas globales (origen arriba-izq)
# Layout en columnas: power izquierda | XIAO centro-izq | sensores I2C derecha
#                     bottom: display + ECG + botones

# Cada entry: ref, lib_id, value, x, y, rotation_deg, datasheet/footprint, pin_nets
# pin_nets: dict {pin_number: net_label} — controla los labels que cuelgan de cada pin

# ----------------------------------------------------------------------
# Definición de componentes
# ----------------------------------------------------------------------
COMPONENTS = []


def snap(v, grid=1.27):
    """Snap a coordinate al grid 1.27 mm (50 mil) de KiCad."""
    return round(v / grid) * grid


def socket_fp(n):
    """Footprint estandar de pin-socket vertical 2.54mm para N pines."""
    return f"Connector_PinSocket_2.54mm:PinSocket_1x{n:02d}_P2.54mm_Vertical"


# Footprints estandar por tipo de componente (se aplican automaticamente
# en add() si no se pasa footprint explicito):
DEFAULT_FOOTPRINTS = {
    "Device:R":      "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder",
    "Device:C":      "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder",
    # Tactile side-press SMD 3.5x7.8x3.8mm — footprint custom local con pads
    # exactos segun datasheet del usuario (1.45x1.3, pitch 6.75x3.1 mm).
    "Switch:SW_Push": "SupaClock_Custom:SW_Tactile_Side_3.5x7.8mm",
    "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-DIP":
        "Seeed_Studio_XIAO_Footprints:XIAO-ESP32-S3-DIP",
}


def add(ref, lib_id, value, x, y, rot, pin_nets, footprint="", dnp=False):
    # Resolver footprint por defecto si no se pasó explícito:
    if not footprint:
        if lib_id in DEFAULT_FOOTPRINTS:
            footprint = DEFAULT_FOOTPRINTS[lib_id]
        elif lib_id.startswith("Connector_Generic:Conn_01x"):
            n = int(lib_id.split("Conn_01x")[1])
            footprint = socket_fp(n)
    COMPONENTS.append(
        dict(
            ref=ref,
            lib_id=lib_id,
            value=value,
            x=snap(x),
            y=snap(y),
            rot=rot,
            pin_nets=pin_nets,
            footprint=footprint,
            dnp=dnp,
        )
    )


# ============================================================
# POWER ENTRY - BMS INTERNO DEL XIAO ESP32-S3 (sin TP4056 externo)
# ============================================================
# Arquitectura v3:
#   Batería LiPo -> JST en MAX17048 (J6c) -> traza del carrier -> J15
#   J15 -> wires soldados a mano -> XIAO BAT+/BAT- (pads SMD del fondo)
#   XIAO ETA6098 carga la celda desde USB-C; SY8089 buck -> 3V3 al sistema.
#
# El MAX17048 sensa V_cell a través de su pad "+" (= +BATT) y sirve la
# telemetría I2C usando VCC=+3V3 (LDO/level-shifter del módulo SparkFun).

# J15: pads en el carrier donde se sueldan los wires hacia XIAO BAT+/BAT-
# El usuario corta 2 wires (~3 cm) que van de aquí a los pads SMD del XIAO
add(
    "J15",
    "Connector_Generic:Conn_01x02",
    "XIAO_BAT_Wires",
    x=80,
    y=40,
    rot=0,
    pin_nets={
        1: "+BATT",  # -> XIAO BAT+ pad (bottom)
        2: "GND",    # -> XIAO BAT- pad (bottom)
    },
)

# Cap de bulk sobre +BATT (cerca de J15 para amortiguar picos de BLE/WiFi
# por la inductancia de los wires hasta el XIAO)
add(
    "C2",
    "Device:C",
    "10uF",
    x=60,
    y=55,
    rot=0,
    pin_nets={1: "+BATT", 2: "GND"},
)

# Decoupling 100nF cerca del XIAO sobre el rail +3V3 (salida del buck interno)
add(
    "C3",
    "Device:C",
    "100nF",
    x=90,
    y=55,
    rot=0,
    pin_nets={1: "+3V3", 2: "GND"},
)


# ============================================================
# MICROCONTROLADOR: Seeed XIAO ESP32-S3 (símbolo DIP oficial de Seeed)
# ============================================================
# Pin map XIAO ESP32-S3 v1.2 (post-feedback - usa BMS interno del XIAO):
#   1 = D0  / GPIO1  / ADC1_CH0 -> ECG_OUT      (analog input)
#   2 = D1  / GPIO2             -> ECG_SDN      (digital out, controla AD8232 SDN)
#   3 = D2  / GPIO3             -> BLK_PWM      (PWM out, controla backlight ST7789)
#   4 = D3  / GPIO4             -> SPI_DC
#   5 = D4  / GPIO5  / I2C SDA  -> I2C_SDA
#   6 = D5  / GPIO6  / I2C SCL  -> I2C_SCL
#   7 = D6  / GPIO43 / UART_TX  -> BTN_NEXT
#   8 = D7  / GPIO44 / UART_RX  -> SPI_CS
#   9 = D8  / GPIO7  / SCK      -> SPI_SCK
#  10 = D9  / GPIO8  / MISO     -> BTN_SELECT
#  11 = D10 / GPIO9  / MOSI     -> SPI_MOSI
#  12 = 3V3 (salida del buck SY8089 interno -> rail +3V3 del carrier)
#  13 = GND
#  14 = VBUS / 5V              -> NC (USB del XIAO se usa solo para carga + dev)
#
# Notas:
# - ST7789 RES queda atado a +3V3 (POR confiable - se libera D2 para BLK_PWM).
# - BAT+/BAT- del XIAO se conectan al carrier via 2 wires soldados (J15) - no
#   vienen por los pines DIP.
# - IMU_INT1 ya no se usa - el BMI160 sirve via polling I2C.
add(
    "U1",
    "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-DIP",
    "XIAO-ESP32-S3",
    x=130,
    y=80,
    rot=0,
    pin_nets={
        1:  "ECG_OUT",
        2:  "ECG_SDN",
        3:  "BLK_PWM",
        4:  "SPI_DC",
        5:  "I2C_SDA",
        6:  "I2C_SCL",
        7:  "BTN_NEXT",
        8:  "SPI_CS",
        9:  "SPI_SCK",
        10: "BTN_SELECT",
        11: "SPI_MOSI",
        12: "+3V3",
        13: "GND",
        14: "_NC_",  # VBUS - sin uso desde el carrier (USB del XIAO independiente)
    },
)


# ============================================================
# I2C BUS — pull-ups (4.7k a 3V3, opcionales)
# ============================================================
add(
    "R1",
    "Device:R",
    "4.7k",
    x=200,
    y=50,
    rot=0,
    pin_nets={1: "+3V3", 2: "I2C_SDA"},
)
add(
    "R2",
    "Device:R",
    "4.7k",
    x=215,
    y=50,
    rot=0,
    pin_nets={1: "+3V3", 2: "I2C_SCL"},
)


# ============================================================
# SENSORES I2C (lado derecho)
# ============================================================
# J3a + J3b: MAX30102 MH-ET LIVE (módulo 16x21mm, pines en DOS bordes)
#   Borde "top"    (J3a): GND, RD, IRD, INT     (4 pines — solo GND eléctricamente útil)
#   Borde "bottom" (J3b): VIN, SDA, SCL, GND    (4 pines — bus I2C principal)
# Carrier requiere dos pin sockets enfrentados con spacing ~16mm.
add(
    "J3",
    "Connector_Generic:Conn_01x04",
    "MAX30102_top",
    x=240,
    y=40,
    rot=0,
    pin_nets={
        1: "GND",   # GND (top edge label)
        2: "_NC_",  # RD  (control LED rojo, no requerido externamente)
        3: "_NC_",  # IRD (control LED IR, no requerido externamente)
        4: "_NC_",  # INT (no usado - polling en v1)
    },
    footprint="SupaClock_Custom:MAX30102_Castellated_1x4",
)
add(
    "J14",
    "Connector_Generic:Conn_01x04",
    "MAX30102_bot",
    x=240,
    y=70,
    rot=0,
    pin_nets={
        1: "+3V3",     # VIN
        2: "I2C_SDA",  # SDA
        3: "I2C_SCL",  # SCL
        4: "GND",      # GND (segundo punto de tierra del módulo)
    },
    footprint="SupaClock_Custom:MAX30102_Castellated_1x4",
)
# J4: MAX30205 module (temperatura) 8 pads SMD flush mount
# Pin order confirmado por usuario (todos en un solo lado):
#   VCC, GND, SDA, SCL, OS, A0, A1, A2
# Estrategia mecánica: pads SMD planos en el carrier (NO pin socket); el módulo
# va PEGADO a la PCB y se cubre con un pedazo de aluminio + thermal pad para
# transferir calor de la piel/case al chip.
# A0/A1/A2 = GND -> dirección I2C 0x48 (estándar MAX30205)
# OS = NC (overtemperature shutdown - alert no usado)
add(
    "J4",
    "Connector_Generic:Conn_01x08",
    "MAX30205_8pad",
    x=270,
    y=55,
    rot=0,
    pin_nets={
        1: "+3V3",     # VCC
        2: "GND",
        3: "I2C_SDA",
        4: "I2C_SCL",
        5: "_NC_",     # OS - overtemperature shutdown (open-drain alert)
        6: "GND",      # A0 -> low
        7: "GND",      # A1 -> low
        8: "GND",      # A2 -> low  =>  I2C addr 0x48
    },
    footprint="SupaClock_Custom:MAX30205_8pad_Flush",
)
# J5: BMI160 BAISHUN — solo el lado de 7 pines (el de 5 sin conexión).
# Pin order confirmado por usuario (top -> bottom):
#   VIN, 3V3, GND, SCL, SDA, CS, SAO
# Configuración para modo I2C @ 0x68:
#   CS  = HIGH (+3V3) -> activa modo I2C
#   SAO = LOW  (GND)  -> dirección 0x68 (HIGH = 0x69)
add(
    "J5",
    "Connector_Generic:Conn_01x07",
    "BMI160_7p",
    x=240,
    y=95,
    rot=0,
    pin_nets={
        1: "+3V3",       # VIN
        2: "_NC_",       # 3V3 (salida LDO onboard - no requerida)
        3: "GND",
        4: "I2C_SCL",
        5: "I2C_SDA",
        6: "+3V3",       # CS  = HIGH -> modo I2C
        7: "GND",        # SAO = LOW  -> dirección 0x68
    },
)
# J6a/J6b/J6c: MAX17048 SparkFun Fuel Gauge (red PCB)
# Pin order confirmado por usuario (mirando el módulo desde arriba):
#   J6a (lado IZQUIERDO, junto al pad "-"):  VCC, GND, ALT
#   J6b (lado DERECHO,   junto al pad "+"):  SDA, SCL, QST
#   J6c (pads BAT,       "+" y "-"):         +BATT, GND  (paralelo al JST onboard)
#
# El chip se alimenta con VCC = +3V3 (módulo SparkFun tiene level-shifter
# I2C para que SDA/SCL sean 3V3 incluso si la celda está en 4.2V).
# La batería conecta al JST onboard del módulo; el carrier sensa V_cell via J6c.
add(
    "J6a",
    "Connector_Generic:Conn_01x03",
    "MAX17048_L",
    x=290,
    y=80,
    rot=0,
    pin_nets={
        1: "+3V3",     # VCC (logic supply + level shifter)
        2: "GND",
        3: "_NC_",     # ALT (alert int - no usado en v1)
    },
)
add(
    "J6b",
    "Connector_Generic:Conn_01x03",
    "MAX17048_R",
    x=320,
    y=80,
    rot=0,
    pin_nets={
        1: "I2C_SDA",
        2: "I2C_SCL",
        3: "_NC_",     # QST (quickstart input - no requerido en v1)
    },
)
add(
    "J6c",
    "Connector_Generic:Conn_01x02",
    "MAX17048_BAT",
    x=305,
    y=110,
    rot=0,
    pin_nets={
        1: "+BATT",    # + pad (battery cell positivo, paralelo al JST onboard)
        2: "GND",      # - pad (battery cell negativo)
    },
)


# ============================================================
# PANTALLA ST7789 (abajo izquierda)
# ============================================================
# J7: 8-pin: GND, VCC, SCL, SDA, RES, DC, CS, BLK
# RES atado a +3V3 (POR confiable del controlador, libera D2 del XIAO para BLK_PWM).
# BLK controlado por PWM desde XIAO D2/GPIO3 (apagar pantalla / dimming).
add(
    "J7",
    "Connector_Generic:Conn_01x08",
    "ST7789_1.69in",
    x=130,
    y=160,
    rot=0,
    pin_nets={
        1: "GND",       # GND
        2: "+3V3",      # VCC
        3: "SPI_SCK",   # SCL
        4: "SPI_MOSI",  # SDA
        5: "SPI_RES",   # RES - via RC POR (R5 10k + C6 100nF) para reset confiable
        6: "SPI_DC",    # DC
        7: "SPI_CS",    # CS
        8: "BLK_PWM",   # BLK - PWM desde XIAO D2 para control de brillo / apagado
    },
)

# R5 + C6: RC POR network sobre RES del ST7789
#   R5 10kΩ de +3V3 a SPI_RES (pull-up estatico)
#   C6 100nF de SPI_RES a GND
#   Constante de tiempo RC = 10k * 100nF = 1ms (suficiente para POR del ST7789)
#   En power-up: C6 descargada -> SPI_RES = 0V (reset activo). C6 carga via R5 ->
#   tras ~1ms SPI_RES sube a +3V3 (reset liberado). El controlador del display
#   ve un pulso de reset limpio cada vez que sube la alimentacion.
add(
    "R5",
    "Device:R",
    "10k",
    x=145,
    y=145,
    rot=0,
    pin_nets={1: "+3V3", 2: "SPI_RES"},
)
add(
    "C6",
    "Device:C",
    "100nF",
    x=155,
    y=145,
    rot=0,
    pin_nets={1: "SPI_RES", 2: "GND"},
)


# ============================================================
# AD8232 SparkFun-style (red PCB con jack 3.5mm)
# - 6-pin header: GND, 3V3, OUTPUT, LO-, LO+, SDN
# - 3 pads aparte: RA, LA, RL (alternativos al jack 3.5mm)
# ============================================================
add(
    "J11",
    "Connector_Generic:Conn_01x06",
    "AD8232_SparkFun",
    x=200,
    y=160,
    rot=0,
    pin_nets={
        1: "GND",
        2: "+3V3",      # 3.3V
        3: "ECG_OUT",   # OUTPUT (analog -> ADC del XIAO GPIO1)
        4: "_NC_",      # LO-  (leads-off detect, no usado en v1)
        5: "_NC_",      # LO+  (leads-off detect, no usado en v1)
        6: "ECG_SDN",   # SDN controlado por XIAO D1 - HIGH=activo, LOW=shutdown
    },
)

# M3 electrodos: 3 pads separados al borde de la PCB
#   J12_RA: borde derecho ENTRE los 2 botones (contacto con la mano opuesta)
#   J12_LA: borde inferior izquierdo (contacto con la muñeca, izq)
#   J12_RL: borde inferior derecho (contacto con la muñeca, der - driven leg)
# Cada uno es 1-pin con footprint custom M3_Electrode_Pad (pad 6mm Cu + hole
# 3.2mm NPTH para tornillo M3 304). Se colocan independientes en el layout.
add(
    "J12_RA",
    "Connector_Generic:Conn_01x01",
    "ECG_RA_pad_M3",
    x=160,
    y=160,
    rot=0,
    pin_nets={1: "ECG_RA"},
    footprint="SupaClock_Custom:M3_Electrode_Pad",
)
add(
    "J12_LA",
    "Connector_Generic:Conn_01x01",
    "ECG_LA_pad_M3",
    x=160,
    y=175,
    rot=0,
    pin_nets={1: "ECG_LA"},
    footprint="SupaClock_Custom:M3_Electrode_Pad",
)
add(
    "J12_RL",
    "Connector_Generic:Conn_01x01",
    "ECG_RL_pad_M3",
    x=160,
    y=190,
    rot=0,
    pin_nets={1: "ECG_RL"},
    footprint="SupaClock_Custom:M3_Electrode_Pad",
)

# J13: Pads RA/LA/RL del módulo AD8232 SparkFun (off-board, mismo carrier).
# Representa el otro extremo de los jumpers de electrodo — cierra el net en
# el esquemático para que ERC sea coherente.
add(
    "J13",
    "Connector_Generic:Conn_01x03",
    "AD8232_Pads_RA_LA_RL",
    x=140,
    y=160,
    rot=0,
    pin_nets={
        1: "ECG_RA",
        2: "ECG_LA",
        3: "ECG_RL",
    },
)


# ============================================================
# BOTONES (abajo derecha)
# ============================================================
# SW1: SELECT
add(
    "SW1",
    "Switch:SW_Push",
    "SELECT",
    x=280,
    y=160,
    rot=0,
    pin_nets={1: "BTN_SELECT", 2: "GND"},
)
# SW2: NEXT
add(
    "SW2",
    "Switch:SW_Push",
    "NEXT",
    x=300,
    y=160,
    rot=0,
    pin_nets={1: "BTN_NEXT", 2: "GND"},
)
# Pull-ups botones (opcionales — XIAO ESP32-S3 tiene pull-ups internos pero externos son robustez extra)
add(
    "R3",
    "Device:R",
    "10k",
    x=280,
    y=140,
    rot=0,
    pin_nets={1: "+3V3", 2: "BTN_SELECT"},
    dnp=True,  # default not stuffed — usar pull-up interno
)
add(
    "R4",
    "Device:R",
    "10k",
    x=300,
    y=140,
    rot=0,
    pin_nets={1: "+3V3", 2: "BTN_NEXT"},
    dnp=True,
)
# Capacitores de debounce (opcionales)
add(
    "C4",
    "Device:C",
    "100nF",
    x=280,
    y=175,
    rot=0,
    pin_nets={1: "BTN_SELECT", 2: "GND"},
    dnp=True,
)
add(
    "C5",
    "Device:C",
    "100nF",
    x=300,
    y=175,
    rot=0,
    pin_nets={1: "BTN_NEXT", 2: "GND"},
    dnp=True,
)


# ============================================================
# Test points para señales analógicas críticas (opcional)
# ============================================================
# (Omitidos en v1 — los test points se añaden en revisión de layout)


# ----------------------------------------------------------------------
# Generación de S-expression
# ----------------------------------------------------------------------

# Cargar lib_symbols
lib_symbol_blocks = []
for lib, sym in NEEDED_SYMBOLS:
    lib_symbol_blocks.append(extract_symbol(lib, sym))

lib_symbols_text = "".join(lib_symbol_blocks)

# Generar instancias
symbol_blocks = []
wire_blocks = []
label_blocks = []
nc_blocks = []
junction_blocks = []

# Información de pin offsets para cada lib_id (donde están los pines respecto al origen del símbolo)
# Para Conn_01x0N: pines hacia la derecha, separados 2.54 mm verticalmente.
# Para R, C: 2 pines verticales separados 7.62 mm.
# Para SW_Push: 2 pines horizontales separados 6.35 mm.
# Para símbolos de power: 1 pin en (0,0) hacia arriba.

def conn_pin_offset(n_pins, pin_idx):
    """Pin offset relativo al origen para Conn_01xNN. Pines en lado IZQUIERDO
    (X = -5.08), pin 1 arriba.

    Verificado leyendo todos los Conn_01x02..09:
      n=2 -> top_y=0, n=3 -> top_y=2.54, n=4 -> top_y=2.54, n=5 -> top_y=5.08, ...
      Formula: top_y = floor((n-1)/2) * 2.54
    """
    top_y = ((n_pins - 1) // 2) * 2.54
    return (-5.08, top_y - (pin_idx - 1) * 2.54)


def r_pin_offset(pin_idx):
    """Resistor (Device:R): pin 1 arriba (0, 3.81), pin 2 abajo (0, -3.81)."""
    return (0.0, 3.81 if pin_idx == 1 else -3.81)


def c_pin_offset(pin_idx):
    """Capacitor (Device:C): pin 1 arriba (0, 3.81), pin 2 abajo (0, -3.81)."""
    return (0.0, 3.81 if pin_idx == 1 else -3.81)


def sw_pin_offset(pin_idx):
    """SW_Push: pin 1 izq (-5.08, 0), pin 2 der (5.08, 0)."""
    return (-5.08 if pin_idx == 1 else 5.08, 0.0)


# Pin offsets exactos del XIAO-ESP32-S3-DIP (extraídos de Seeed_Studio_XIAO_Series.kicad_sym)
# Pin numbers siguen el footprint físico XIAO:
#   1-7  = lado izquierdo (D0..D6 -> GPIO1..6, 43)
#   8-14 = lado derecho (D7 RX, D8 SCK, D9 MISO, D10 MOSI, 3V3, GND, VBUS)
XIAO_S3_DIP_PINS = {
    1:  (-2.54, -5.08),   # GPIO1_A0_D0
    2:  (-2.54, -8.89),   # GPIO2_A1_D1
    3:  (-2.54, -12.7),   # GPIO3_A2_D2
    4:  (-2.54, -16.51),  # GPIO4_A3_D3
    5:  (-2.54, -20.32),  # GPIO5_A4_D4_SDA
    6:  (-2.54, -24.13),  # GPIO6_A5_D5_SCL
    7:  (-2.54, -27.94),  # GPIO43_D6_TX
    8:  (44.45, -27.94),  # GPIO44_D7_RX
    9:  (44.45, -24.13),  # GPIO7_A8_D8_SCK
    10: (44.45, -20.32),  # GPIO8_A9_D9_MISO
    11: (44.45, -16.51),  # GPIO9_A10_D10_MOSI
    12: (44.45, -12.7),   # 3V3
    13: (44.45, -8.89),   # GND
    14: (44.45, -5.08),   # VBUS (5V USB)
}


def pin_offset_for(lib_id, pin_idx):
    if lib_id.startswith("Connector_Generic:Conn_01x"):
        n = int(lib_id.split("Conn_01x")[1])
        return conn_pin_offset(n, pin_idx)
    elif lib_id == "Device:R":
        return r_pin_offset(pin_idx)
    elif lib_id == "Device:C":
        return c_pin_offset(pin_idx)
    elif lib_id == "Switch:SW_Push":
        return sw_pin_offset(pin_idx)
    elif lib_id == "Seeed_Studio_XIAO_Series:XIAO-ESP32-S3-DIP":
        return XIAO_S3_DIP_PINS[pin_idx]
    else:
        raise ValueError(f"Unknown lib_id pin offset: {lib_id}")


def rotate(p, deg):
    """Rota un offset (x,y) por deg grados (en coords de librería, Y-up)."""
    import math
    r = math.radians(deg)
    cs, sn = math.cos(r), math.sin(r)
    x, y = p
    return (x * cs - y * sn, x * sn + y * cs)


def lib_to_sch(comp_x, comp_y, lib_off, rot_deg):
    """Convierte un offset de pin en coords de librería (Y-up) a coords absolutas
    en el esquemático (Y-down). Aplica la rotación de la instancia.

    KiCad: abs.x = comp.x + rotated_lib.x ; abs.y = comp.y - rotated_lib.y
    """
    rx, ry = rotate(lib_off, rot_deg)
    return (comp_x + rx, comp_y - ry)


for comp in COMPONENTS:
    ref = comp["ref"]
    lib_id = comp["lib_id"]
    val = comp["value"]
    cx, cy = comp["x"], comp["y"]
    rot = comp["rot"]
    pin_nets = comp["pin_nets"]
    fp = comp["footprint"]
    dnp = comp["dnp"]
    comp_uuid = uid()

    # Bloque de instancia de símbolo
    dnp_field = "yes" if dnp else "no"
    instance_block = f"""	(symbol
		(lib_id "{lib_id}")
		(at {cx} {cy} {rot})
		(unit 1)
		(exclude_from_sim no)
		(in_bom yes)
		(on_board yes)
		(dnp {dnp_field})
		(uuid "{comp_uuid}")
		(property "Reference" "{ref}"
			(at {cx + 5.08} {cy - 5.08} 0)
			(effects
				(font
					(size 1.27 1.27)
				)
				(justify left)
			)
		)
		(property "Value" "{val}"
			(at {cx + 5.08} {cy - 2.54} 0)
			(effects
				(font
					(size 1.27 1.27)
				)
				(justify left)
			)
		)
		(property "Footprint" "{fp}"
			(at {cx} {cy} 0)
			(effects
				(font
					(size 1.27 1.27)
				)
				(hide yes)
			)
		)
		(property "Datasheet" ""
			(at {cx} {cy} 0)
			(effects
				(font
					(size 1.27 1.27)
				)
				(hide yes)
			)
		)
		(property "Description" ""
			(at {cx} {cy} 0)
			(effects
				(font
					(size 1.27 1.27)
				)
				(hide yes)
			)
		)
"""
    # Pines
    for pin_idx in pin_nets:
        pin_uuid = uid()
        instance_block += f'\t\t(pin "{pin_idx}"\n\t\t\t(uuid "{pin_uuid}")\n\t\t)\n'

    # Instance path
    inst_uuid = "00000000-0000-0000-0000-000000000001"
    instance_block += f"""		(instances
			(project "SupaClock_Carrier"
				(path "/{inst_uuid}"
					(reference "{ref}")
					(unit 1)
				)
			)
		)
	)
"""
    symbol_blocks.append(instance_block)

    # Wires + labels: para cada pin, un stub de 2.54 mm hacia afuera y un label
    # Sentinel "_NC_" => generar no_connect en lugar de wire+label.
    for pin_idx, net_name in pin_nets.items():
        po = pin_offset_for(lib_id, pin_idx)  # lib coords (Y-up)
        pin_abs = lib_to_sch(cx, cy, po, rot)

        if net_name == "_NC_":
            nc_uuid = uid()
            nc_blocks.append(
                f"""	(no_connect
		(at {pin_abs[0]} {pin_abs[1]})
		(uuid "{nc_uuid}")
	)
"""
            )
            continue
        # Dirección del stub en lib coords (luego se transforma a sch).
        # Regla general: el stub sale del pin alejandose del cuerpo del simbolo:
        #   pines a la izquierda del cuerpo  (po.x <  0)  -> stub hacia -X
        #   pines a la derecha del cuerpo    (po.x >  0)  -> stub hacia +X
        #   pines verticales (R/C, po.x = 0) -> stub vertical segun po.y
        if lib_id in ("Device:R", "Device:C"):
            dlib = (0.0, 2.54 if po[1] > 0 else -2.54)
        elif po[0] < 0:
            dlib = (-2.54, 0.0)
        else:
            dlib = (2.54, 0.0)
        # Aplicar la misma transformación que al pin
        d_rot = rotate(dlib, rot)
        # En coords de esquemático: dx_sch = drot.x, dy_sch = -drot.y
        dx_sch, dy_sch = d_rot[0], -d_rot[1]
        end_x, end_y = pin_abs[0] + dx_sch, pin_abs[1] + dy_sch

        # Wire
        wire_uuid = uid()
        wire_blocks.append(
            f"""	(wire
		(pts
			(xy {pin_abs[0]} {pin_abs[1]}) (xy {end_x} {end_y})
		)
		(stroke
			(width 0)
			(type default)
		)
		(uuid "{wire_uuid}")
	)
"""
        )

        # Label (global label si es power: +3V3, +5V, GND, +BATT — los power flags se usan)
        # Aquí usamos local labels para todo, KiCad los une por nombre
        label_uuid = uid()
        # Justificación del label según dirección del stub (en sch coords)
        # - stub hacia la derecha (+x): label texto extiende a la derecha, just left, ang 0
        # - stub hacia la izquierda (-x): label texto extiende a la izquierda, just right, ang 0
        # - stub hacia arriba (-y en sch coords): texto vertical reading bottom→top, ang 90
        # - stub hacia abajo (+y en sch coords): texto vertical reading top→bottom, ang 270
        if dx_sch > 0.1:
            justify = "left"
            angle_label = 0
        elif dx_sch < -0.1:
            justify = "right"
            angle_label = 0
        elif dy_sch < -0.1:
            justify = "left"
            angle_label = 90
        else:
            justify = "right"
            angle_label = 90

        label_blocks.append(
            f"""	(label "{net_name}"
		(at {end_x} {end_y} {angle_label})
		(effects
			(font
				(size 1.27 1.27)
			)
			(justify {justify} bottom)
		)
		(uuid "{label_uuid}")
	)
"""
        )

# ----------------------------------------------------------------------
# Construir el archivo final
# ----------------------------------------------------------------------
sheet_uuid = "00000000-0000-0000-0000-000000000001"
sch = f"""(kicad_sch
	(version 20231120)
	(generator "eeschema")
	(generator_version "9.0")
	(uuid "{sheet_uuid}")
	(paper "{PAGE}")
	(title_block
		(title "SupaClock Carrier v1 - Through-Hole Pin Header Board")
		(date "2026-05-11")
		(rev "v1.0")
		(company "PUC IEE2913 - Grupo 10")
		(comment 1 "Wearable Biometrico Modular - Carrier para XIAO ESP32-S3")
		(comment 2 "Hito 25% / Iteracion 1 - LPKF ProtoMat S64 conservative rules")
		(comment 3 "T. Avendano, B. Sepulveda, P. Uribe")
		(comment 4 "")
	)
	(lib_symbols
{lib_symbols_text}	)
{"".join(symbol_blocks)}{"".join(wire_blocks)}{"".join(label_blocks)}{"".join(nc_blocks)}	(sheet_instances
		(path "/"
			(page "1")
		)
	)
	(embedded_fonts no)
)
"""

OUT_FILE.write_text(sch)
print(f"Escrito: {OUT_FILE}")
print(f"Símbolos instanciados: {len(COMPONENTS)}")
print(f"Wires: {len(wire_blocks)}")
print(f"Labels: {len(label_blocks)}")
print(f"No-Connect markers: {len(nc_blocks)}")

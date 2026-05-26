# SupaClock Display Adapter v1

Small single-sided daughter PCB that mates a 1.3″ 240×240 ST7789 panel
(24-pin FPC tail) to the existing `SupaClock_Carrier` board via its J7
1×8 pin socket. Replaces the broken 1.69″ module without modifying the
carrier.

KiCad project: `hardware/SupaClock_Display_Adapter/`

## BOM

| Ref | Value  | Package                                              | Notes                                        |
|-----|--------|------------------------------------------------------|----------------------------------------------|
| J1  | 1×8    | `PinHeader_1x08_P2.54mm_Vertical`                    | Male header → carrier J7 socket              |
| J2  | FPC-24 | `Hirose_FH12-24S-0.5SH_1x24-1MP_P0.50mm_Horizontal`  | Bottom-contact 0.5 mm. Verify panel matches  |
| Q1  | 2N7000 | `TO-92_Inline`                                       | N-MOSFET, BL low-side switch                 |
| R1  | 10 kΩ  | Axial 1/4 W                                          | Gate pull-down (BL off when MCU floats)      |
| R2  | 22 Ω   | Axial 1/4 W                                          | LED current limit (adjust to panel Vf)       |
| R3  | 10 kΩ  | Axial 1/4 W                                          | IM1/2 pull-up → +3V3 (selects 4-SPI mode)    |
| R4  | 10 kΩ  | Axial 1/4 W                                          | RESET pull-up → +3V3                         |
| R5  | 10 kΩ  | Axial 1/4 W                                          | RD pull-up → +3V3 (unused in 4-SPI)          |
| C1  | 10 µF  | Radial electrolytic, P=2.5 mm                        | VDD bulk decoupling                          |
| C2  | 100 nF | Disc ceramic, P=2.5 mm                               | VDD high-freq decoupling                     |

Only **J2** is SMD. Everything else is THT and fits in any lab tray.

## J1 ↔ Carrier J7 pin map

Identical to the broken 1.69″ module — drop-in replacement.

| J1 pin | Net      | Carrier signal      |
|-------:|----------|---------------------|
|  1     | GND      | GND                 |
|  2     | +3V3     | +3V3                |
|  3     | SPI_SCK  | XIAO GPIO7  (SCK)   |
|  4     | SPI_MOSI | XIAO GPIO9  (MOSI)  |
|  5     | +3V3     | +3V3 (duplicate)    |
|  6     | SPI_DC   | XIAO GPIO4  (D/C)   |
|  7     | SPI_CS   | XIAO GPIO44 (CS)    |
|  8     | BLK_PWM  | XIAO GPIO3  (LEDC)  |

## J2 (FPC) pin map — 1.3″ ST7789 4-SPI mode

| FPC pin | Symbol  | Adapter net  | Notes                                       |
|--------:|---------|--------------|---------------------------------------------|
|   1     | LEDA    | LED_A        | → R2 → +3V3                                 |
|   2     | LEDK    | LED_K        | → Q1.D                                      |
|   3     | GND     | GND          |                                             |
|   4     | VDD-2.8V| +3V3         | Bridged with VDDIO                          |
|   5     | VDDIO   | +3V3         |                                             |
|   6     | IM1/2   | IM_HI        | → R3 → +3V3 (selects 4-SPI)                 |
|   7     | RESET   | RST_HI       | → R4 → +3V3 (software reset via SWRESET)    |
|   8     | CS      | SPI_CS       |                                             |
|   9     | D/C     | SPI_SCK      | Acts as **serial clock** in 4-SPI mode      |
|  10     | WR      | SPI_DC       | Acts as **D/C** in 4-SPI mode               |
|  11     | RD      | RD_HI        | → R5 → +3V3 (unused in 4-SPI)               |
|  12     | SDA     | SPI_MOSI     |                                             |
|  13-20  | DB0-DB7 | GND          | All shorted to GND (parallel bus unused)    |
|  21     | TE      | TE_NC        | Tearing effect output — leave floating      |
|  22     | NC      | NC_22        | No connect                                  |
|  23-24  | GND     | GND          |                                             |

**Important quirk:** the cheap-panel datasheet labels are misleading. In
4-SPI mode the panel uses the *D/C pin as serial clock* (because the
chip's parallel-mode timing pin doubles as SCL) and the *WR pin as D/C*.
The adapter wires accordingly — firmware does not need to change.

## Backlight assumption

The 22 Ω current-limit assumes a single backlight LED at ~2.5 V Vf
running from +3V3 directly. **Before assembly, measure your panel's LED
forward voltage** (multimeter in diode mode on FPC pins 1↔2 or by
applying 3.3 V through a 1 kΩ and reading the drop). If Vf > 3.0 V the
panel likely needs a boost converter (the original Waveshare 1.69″ had
one); in that case stick a TPS61040 or AP3015 boost module between +3V3
and LED_A, and bump R2 to the value calculated from the new LED string.

## Assembly order

1. **Drag-solder J2 first** while the board is flat and unobstructed.
   Use plenty of flux, hold one corner pin to align, then sweep across
   pads with a fine-tip iron and braid off bridges. Verify continuity
   with a multimeter — pad 1 to pad 24 should *not* short.
2. Solder Q1, R1-R5 in the central area. THT, easy.
3. Solder C1 (mind polarity) and C2 near the J1 power pins.
4. Solder J1 last so the header fits cleanly into the carrier socket
   for height alignment.
5. Inspect under magnification: every FPC pad must have a smooth fillet,
   no bridges, no cold joints.

## Firmware change required (after assembly)

Switch the panel from 240×280 (1.69″) to 240×240 (1.3″):

- `lib/st7789_driver/st7789.c`:
  - `set_addr_window`: remove the `y_offset = 20` line (no offset for
    240×240 panels — the controller's RAM origin lines up).
  - `st7789_send_buffer`: change `set_addr_window(0, 0, 239, 279)` to
    `set_addr_window(0, 0, 239, 239)`.
  - `st7789_draw_bitmap`: change `y >= 280` to `y >= 240` and
    `yE >= 280 / yE = 279 / 280 - y` to the 240 equivalents.
  - `st7789_fill_screen`: change the `(240 * 280) / chunk_pixels` chunk
    count to `(240 * 240) / chunk_pixels` and the same in
    `max_transfer_sz` in the SPI bus config.
- `include/lv_conf.h`:
  - `LV_HOR_RES_MAX = 240`
  - `LV_VER_RES_MAX = 240` (was 280)

Recompile `pio run -e main_app`. The image position assets (clock face,
icons) may need vertical re-centering if they used the extra 40 lines.

## Single-sided LPKF mill — layout notes

- Place J2 along one short edge so the FPC tail clears the rest of the
  board when folded back.
- Place J1 on the opposite short edge so the daughterboard slots cleanly
  over the carrier J7 socket without crashing into other components.
- Run a wide GND copper pour under the FPC connector — every other pad
  is GND once you tie DB0-DB7 together, so a poured plane saves trace
  count dramatically.
- Cross-overs: the few signals that need to cross (SCK↔MOSI possibly)
  can use 0 Ω jumpers as discrete THT links — easier than a B.Cu trace
  on a single-sided board.
- No silkscreen: print the placement view at 1:1 on paper, tape it
  alongside the board, mark each component with a Sharpie tick after
  populating it.

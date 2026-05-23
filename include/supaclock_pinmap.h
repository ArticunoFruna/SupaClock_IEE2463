/**
 * @file supaclock_pinmap.h
 * @brief Mapa central de pines para el carrier SupaClock v1 (XIAO ESP32-S3 DIP).
 *
 * Derivado de hardware/SupaClock_Carrier/netlist.net (rev v1.0, 2026-05-11).
 * Es la única fuente de verdad: cualquier driver que necesite un GPIO debe
 * incluir este header en vez de hard-codear el número.
 *
 * Convención de los headers de la XIAO ESP32-S3 (DIP-14, vista superior con
 * USB-C hacia arriba):
 *
 *     ┌───────────────┐
 *     │ 1  GPIO1  D0  │ ← ECG_OUT          (ADC1_CH0)
 *     │ 2  GPIO2  D1  │ ← ECG_SDN
 *     │ 3  GPIO3  D2  │ ← BLK_PWM          (LEDC)
 *     │ 4  GPIO4  D3  │ ← SPI_DC
 *     │ 5  GPIO5  D4  │ ← I2C_SDA
 *     │ 6  GPIO6  D5  │ ← I2C_SCL
 *     │ 7  GPIO43 D6  │ ← BTN_NEXT         (era U0TXD, requiere USB-CDC)
 *     │ 8  GPIO44 D7  │ ← SPI_CS           (era U0RXD, requiere USB-CDC)
 *     │ 9  GPIO7  D8  │ ← SPI_SCK
 *     │10  GPIO8  D9  │ ← BTN_SELECT
 *     │11  GPIO9  D10 │ ← SPI_MOSI
 *     │12  3V3        │
 *     │13  GND        │
 *     │14  VBUS       │  (no conectado en el carrier)
 *     └───────────────┘
 *
 * NOTA: La consola debe ir por USB-Serial-JTAG
 *       (CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y) porque GPIO43/44 están
 *       ocupados por botón y CS del display.
 *
 * NOTA: BMI160 INT1 NO está cableada en este carrier (J5 pin 6 = NC).
 *       El HAR debe leer la FIFO por polling (sin IRQ de hardware).
 */
#ifndef SUPACLOCK_PINMAP_H
#define SUPACLOCK_PINMAP_H

/*
 * Selector de board:
 *   -D SUPACLOCK_BOARD_C3=1  → ESP32-C3 SuperMini, banco de captura de IMU
 *                              para entrenar el HAR (semana del 2026-05-23).
 *   (default)                → XIAO ESP32-S3 sobre el carrier v1.
 */

#if defined(SUPACLOCK_BOARD_C3)

/* ──────────────────── ESP32-C3 SuperMini (bench HAR capture) ────────────────────
 * Sólo el BMI160 está conectado vía I2C. Display / ECG / botones quedan
 * deshabilitados; los drivers que referencien SPI / ADC / BTN no se compilan
 * en el env capture_c3.
 */
#define SUPA_PIN_I2C_SDA        8       /* GPIO8 = SDA por default en C3 */
#define SUPA_PIN_I2C_SCL        9       /* GPIO9 = SCL por default en C3 */

#define SUPA_PIN_BMI160_INT1    (-1)    /* No usamos IRQ en bench */

/* Resto deshabilitado en C3 (los envs de capture no los compilan). */
#define SUPA_PIN_BTN_NEXT       (-1)
#define SUPA_PIN_BTN_SELECT     (-1)
#define SUPA_PIN_SPI_MOSI       (-1)
#define SUPA_PIN_SPI_SCK        (-1)
#define SUPA_PIN_SPI_CS         (-1)
#define SUPA_PIN_SPI_DC         (-1)
#define SUPA_PIN_LCD_RST        (-1)
#define SUPA_PIN_LCD_BLK        (-1)
#define SUPA_PIN_ECG_OUT        (-1)
#define SUPA_PIN_ECG_SDN        (-1)
#define SUPA_PIN_ECG_LO_PLUS    (-1)
#define SUPA_PIN_ECG_LO_MINUS   (-1)
#define SUPA_ADC_CHANNEL_ECG    ADC_CHANNEL_0
#define SUPA_ADC_UNIT_ECG       ADC_UNIT_1

#else  /* ──────────────── XIAO ESP32-S3 carrier v1 (default) ──────────────── */

/* Convención de los headers de la XIAO ESP32-S3 (DIP-14, vista superior con
 * USB-C hacia arriba):
 *
 *     ┌───────────────┐
 *     │ 1  GPIO1  D0  │ ← ECG_OUT          (ADC1_CH0)
 *     │ 2  GPIO2  D1  │ ← ECG_SDN
 *     │ 3  GPIO3  D2  │ ← BLK_PWM          (LEDC)
 *     │ 4  GPIO4  D3  │ ← SPI_DC
 *     │ 5  GPIO5  D4  │ ← I2C_SDA
 *     │ 6  GPIO6  D5  │ ← I2C_SCL
 *     │ 7  GPIO43 D6  │ ← BTN_NEXT         (era U0TXD, requiere USB-CDC)
 *     │ 8  GPIO44 D7  │ ← SPI_CS           (era U0RXD, requiere USB-CDC)
 *     │ 9  GPIO7  D8  │ ← SPI_SCK
 *     │10  GPIO8  D9  │ ← BTN_SELECT
 *     │11  GPIO9  D10 │ ← SPI_MOSI
 *     │12  3V3        │
 *     │13  GND        │
 *     │14  VBUS       │  (no conectado en el carrier)
 *     └───────────────┘
 *
 * NOTA: Consola debe ir por USB-Serial-JTAG porque GPIO43/44 están ocupados.
 * NOTA: BMI160 INT1 NO está cableada en este carrier (J5 pin 6 = NC).
 */

/* ───── I2C compartido (BMI160, MAX30102, MAX30205, MAX17048) ───── */
#define SUPA_PIN_I2C_SDA        5
#define SUPA_PIN_I2C_SCL        6

/* ───── SPI para ST7789 1.69" ───── */
#define SUPA_PIN_SPI_MOSI       9
#define SUPA_PIN_SPI_SCK        7
#define SUPA_PIN_SPI_CS         44
#define SUPA_PIN_SPI_DC         4
#define SUPA_PIN_LCD_RST        (-1)   /* no cableado → reset por software */
#define SUPA_PIN_LCD_BLK        3      /* LEDC PWM */

/* ───── AD8232 ECG ───── */
#define SUPA_PIN_ECG_OUT        1      /* ADC1_CH0 (S3) */
#define SUPA_PIN_ECG_SDN        2
#define SUPA_PIN_ECG_LO_PLUS    (-1)
#define SUPA_PIN_ECG_LO_MINUS   (-1)
#define SUPA_ADC_CHANNEL_ECG    ADC_CHANNEL_0
#define SUPA_ADC_UNIT_ECG       ADC_UNIT_1

/* ───── Botones ───── */
#define SUPA_PIN_BTN_NEXT       43
#define SUPA_PIN_BTN_SELECT     8

/* ───── IMU INT1 (BMI160) ───── */
#define SUPA_PIN_BMI160_INT1    (-1)   /* no cableada en carrier v1 */

#endif /* SUPACLOCK_BOARD_C3 */

#endif /* SUPACLOCK_PINMAP_H */

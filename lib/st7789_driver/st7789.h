#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/spi_master.h"
#include "supaclock_pinmap.h"

#ifdef __cplusplus
extern "C" {
#endif

// Pin definitions (XIAO ESP32-S3 carrier v1 — ver supaclock_pinmap.h)
#define ST7789_MOSI_PIN  SUPA_PIN_SPI_MOSI
#define ST7789_SCK_PIN   SUPA_PIN_SPI_SCK
#define ST7789_CS_PIN    SUPA_PIN_SPI_CS
#define ST7789_DC_PIN    SUPA_PIN_SPI_DC
#define ST7789_RST_PIN   SUPA_PIN_LCD_RST     /* -1 → soft-reset por SWRESET */
#define ST7789_BLK_PIN   SUPA_PIN_LCD_BLK

// ST7789 Config registers
#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT  0x11
#define ST7789_NORON   0x13
#define ST7789_INVON   0x21
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_MADCTL  0x36
#define ST7789_COLMOD  0x3A

esp_err_t st7789_init(void);
void st7789_send_buffer(const uint8_t *buffer, size_t size);
void st7789_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *bitmap);
void st7789_fill_screen(uint16_t color);
void st7789_set_brightness(uint8_t percent); // 0-100

#ifdef __cplusplus
}
#endif

#endif // ST7789_H

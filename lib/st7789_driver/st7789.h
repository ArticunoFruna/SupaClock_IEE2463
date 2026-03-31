#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

// Pin definitions
#define ST7789_MOSI_PIN  6
#define ST7789_SCK_PIN   4
#define ST7789_CS_PIN    5
#define ST7789_DC_PIN    3
#define ST7789_RST_PIN   7

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

#ifdef __cplusplus
}
#endif

#endif // ST7789_H

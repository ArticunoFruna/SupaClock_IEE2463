#ifndef GC9A01_H
#define GC9A01_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/spi_master.h"
#include "supaclock_pinmap.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pines heredados del ST7789 (SPI compartido con el mismo bus del carrier). */
#define GC9A01_MOSI_PIN  SUPA_PIN_SPI_MOSI
#define GC9A01_SCK_PIN   SUPA_PIN_SPI_SCK
#define GC9A01_CS_PIN    SUPA_PIN_SPI_CS
#define GC9A01_DC_PIN    SUPA_PIN_SPI_DC
#define GC9A01_RST_PIN   SUPA_PIN_LCD_RST     /* -1 → soft-reset por SWRESET */
#define GC9A01_BLK_PIN   SUPA_PIN_LCD_BLK

/* Comandos MIPI estándar usados en el path de dibujo y sleep */
#define GC9A01_SWRESET  0x01
#define GC9A01_SLPIN    0x10
#define GC9A01_SLPOUT   0x11
#define GC9A01_INVON    0x21
#define GC9A01_DISPOFF  0x28
#define GC9A01_DISPON   0x29
#define GC9A01_CASET    0x2A
#define GC9A01_RASET    0x2B
#define GC9A01_RAMWR    0x2C
#define GC9A01_TEON     0x35
#define GC9A01_MADCTL   0x36
#define GC9A01_COLMOD   0x3A

esp_err_t gc9a01_init(void);
void gc9a01_send_buffer(const uint8_t *buffer, size_t size);
void gc9a01_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *bitmap);
void gc9a01_fill_screen(uint16_t color);
void gc9a01_set_brightness(uint8_t percent); /* 0-100 */

#ifdef __cplusplus
}
#endif

#endif /* GC9A01_H */

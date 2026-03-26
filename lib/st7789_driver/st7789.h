#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/spi_master.h"

// Reemplazar con pins reales según tabla de pines
#define ST7789_MOSI_PIN  6
#define ST7789_SCK_PIN   4
#define ST7789_CS_PIN    5
#define ST7789_DC_PIN    3
#define ST7789_RST_PIN   7

// ST7789 Registers
#define ST7789_NOP     0x00
#define ST7789_SWRESET 0x01
#define ST7789_SLPIN   0x10
#define ST7789_SLPOUT  0x11
#define ST7789_NORON   0x13
#define ST7789_INVOFF  0x20
#define ST7789_INVON   0x21
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_COLMOD  0x3A

/**
 * @brief Init SPI bus and configure ST7789
 * @return esp_err_t ESP_OK on success
 */
esp_err_t st7789_init(void);

/**
 * @brief Send a frame buffer to the SPI DMA
 * @param buffer Pointer to RGB rgb565 pixel buffer
 * @param size Size in bytes
 */
void st7789_send_buffer(const uint8_t *buffer, size_t size);

#endif // ST7789_H

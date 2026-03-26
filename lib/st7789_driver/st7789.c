#include "st7789.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static spi_device_handle_t spi;

static void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    t.user = (void*)0; // D/C needs to be set to 0
    gpio_set_level(ST7789_DC_PIN, 0);
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret==ESP_OK);
}

static void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len) {
    esp_err_t ret;
    spi_transaction_t t;
    if (len == 0) return;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    t.user = (void*)1; // D/C needs to be set to 1
    gpio_set_level(ST7789_DC_PIN, 1);
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret==ESP_OK);
}

esp_err_t st7789_init(void) {
    esp_err_t ret;

    // GPIO Config
    gpio_config_t gc = {
        .pin_bit_mask = (1ULL << ST7789_DC_PIN) | (1ULL << ST7789_RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&gc);

    // SPI Bus Config
    spi_bus_config_t buscfg = {
        .miso_io_num = -1, // No MISO
        .mosi_io_num = ST7789_MOSI_PIN,
        .sclk_io_num = ST7789_SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 240 * 2 // Screen resolution bytes
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000, 
        .mode = 0,
        .spics_io_num = ST7789_CS_PIN,
        .queue_size = 7
    };

    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

    // Hardware Reset
    gpio_set_level(ST7789_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(ST7789_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Software Init Sequence
    lcd_cmd(spi, ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));
    lcd_cmd(spi, ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(150));
    
    // Set color mode to 16-bit (RGB565)
    lcd_cmd(spi, ST7789_COLMOD);
    uint8_t colmod = 0x55;
    lcd_data(spi, &colmod, 1);

    // Turn Display On
    lcd_cmd(spi, ST7789_DISPON);

    return ESP_OK;
}

void st7789_send_buffer(const uint8_t *buffer, size_t size) {
    // Requires CASET / RASET window definition before sending.
    // Example only
    lcd_cmd(spi, ST7789_RAMWR);
    lcd_data(spi, buffer, size);
}

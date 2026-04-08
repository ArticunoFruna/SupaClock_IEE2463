#include "st7789.h"
#include "driver/gpio.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "ST7789_Driver";
static spi_device_handle_t spi;

//Función para enviar comandos a la pantalla 
static void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    t.user = (void*)0; // D/C = 0 for command
    gpio_set_level(ST7789_DC_PIN, 0);
    ret = spi_device_polling_transmit(spi, &t);
    // ESP_LOGD(TAG, "CMD: 0x%02X", cmd); // Descomentar para debug 
    assert(ret==ESP_OK);
}

//Función para enviar data a la pantalla 
static void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len) {
    esp_err_t ret;
    spi_transaction_t t;
    if (len == 0) return; //Si no enviamos nada
        memset(&t, 0, sizeof(t)); //Implica que queremos registrar algo en la memoria

    t.length = len * 8;
    t.tx_buffer = data;
    t.user = (void*)1; // D/C = 1 for data
    gpio_set_level(ST7789_DC_PIN, 1);
    //Enviamos los datos y esperamos que el dispositivo responda  
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret==ESP_OK);
}

static void set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Compensacion interna para pantallas 1.69" de 240x280 (RAM interna de 320)
    uint16_t y_offset = 20; //Sumamos un offset dado que la RAM es de 320x320
    y0 += y_offset;
    y1 += y_offset;

    lcd_cmd(spi, ST7789_CASET);
    // Limites del eje x 
    uint8_t caset_data[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    lcd_data(spi, caset_data, 4);
    lcd_cmd(spi, ST7789_RASET);

    //Limites del eje y
    uint8_t raset_data[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    lcd_data(spi, raset_data, 4);

    lcd_cmd(spi, ST7789_RAMWR);
}

esp_err_t st7789_init(void) {
    esp_err_t ret;
    ESP_LOGI(TAG, "Inicializando pines y SPI para ST7789");

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
        // Limitar tamaño de transf de DMA
        .max_transfer_sz = 240 * 280 * 2 + 8
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 80 * 1000 * 1000,  // 80 MHz para maxima velocidad
        .mode = 0,                           // CPOL=0, CPHA=0 (Mode 0 normal para ST7789)
        .spics_io_num = ST7789_CS_PIN,
        .queue_size = 7
    };

    //Inicializamos el SPI indicando el puerto que controla el bus, su config y la dirección de la DMA
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando bus SPI: %s", esp_err_to_name(ret));
    }
    ESP_ERROR_CHECK(ret);

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error añadiendo disp. SPI: %s", esp_err_to_name(ret));
    }
    ESP_ERROR_CHECK(ret);
/*------------------------------------------- INCIALIZACIÓN DE LA PANTALLA -------------------------------------------*/
    // Hardware Reset
    ESP_LOGI(TAG, "Ejecutando Hardware Reset");
    gpio_set_level(ST7789_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(ST7789_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Software Init Sequence
    ESP_LOGI(TAG, "Enviando comandos de inicializacion...");
    lcd_cmd(spi, ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));
    
    lcd_cmd(spi, ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(150));
    
    // Memory Data Access Control (Orientation and RGB order)
    lcd_cmd(spi, ST7789_MADCTL);
    uint8_t madctl = 0x00; // Normal, RGB Order
    lcd_data(spi, &madctl, 1);

    // Set color mode to 16-bit (RGB565)
    lcd_cmd(spi, ST7789_COLMOD);
    uint8_t colmod = 0x55;
    lcd_data(spi, &colmod, 1);

    // Inversion ON (Critical for IPS)
    ESP_LOGI(TAG, "Fijando modo Inversion ON y Normal ON");
    lcd_cmd(spi, ST7789_INVON); 
    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_cmd(spi, ST7789_NORON);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Turn Display On
    ESP_LOGI(TAG, "Comando DISPON...");
    lcd_cmd(spi, ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Driver ST7789 Init finalizado OK");
    return ESP_OK;
}

// Envío de buffer a la pantalla
void st7789_send_buffer(const uint8_t *buffer, size_t size) {
    set_addr_window(0, 0, 239, 279);
    
    // Fragmentar el buffer en bloques de 19.2 KB (40 lineas)
    // Esto evita que el DMA colapse o que no haya memoria contigua para bounce-buffers con Flash
    size_t chunk_size = 240 * 40 * 2;
    size_t offset = 0;
    
    while (offset < size) { //Limitamos la memoria
        size_t current_chunk = (size - offset > chunk_size) ? chunk_size : (size - offset);
        lcd_data(spi, buffer + offset, current_chunk);
        offset += current_chunk;
    }
}

void st7789_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *bitmap) {
    if (x >= 240 || y >= 280 || w == 0 || h == 0) return;
    
    uint16_t xE = x + w - 1;
    uint16_t yE = y + h - 1;
    
    uint16_t draw_w = w;
    uint16_t draw_h = h;

    if (xE >= 240) {
        xE = 239;
        draw_w = 240 - x;
    }
    if (yE >= 280) {
        yE = 279;
        draw_h = 280 - y;
    }
    
    set_addr_window(x, y, xE, yE);
    
    // Si el bloque cabe perfectamente en la pantalla, enviar todo el bloque de una (Max Performance)
    if (draw_w == w && draw_h == h) {
        size_t send_size = w * h * 2;
        lcd_data(spi, (const uint8_t*)bitmap, send_size);
    } 
    // Si se corta en el borde de la pantalla, enviar fila por fila evadiendo el "Ghosting / Wrapping"
    else {
        for (int row = 0; row < draw_h; row++) {
            // Saltamos a la fila correspondiente usando el Ancho original (w) del bitmap en memoria
            const uint16_t *row_ptr = bitmap + (row * w);
            lcd_data(spi, (const uint8_t*)row_ptr, draw_w * 2);
        }
    }
}


void st7789_fill_screen(uint16_t color) {
    ESP_LOGI(TAG, "Pintando color: 0x%04X", color);
    set_addr_window(0, 0, 239, 279);
    
    uint8_t color_hi = color >> 8;
    uint8_t color_lo = color & 0xFF;
    
    // Enviamos por pedazos mas grandes (40 lienas x 240 pixeles) para velocidad
    size_t chunk_pixels = 240 * 40;
    size_t chunk_bytes = chunk_pixels * 2;
    
    // Usar heap
    uint8_t *buffer = malloc(chunk_bytes);
    if (!buffer) {
        ESP_LOGE(TAG, "No hay memoria en heap para llenar buffer");
        return;
    }
    
    for (int i = 0; i < chunk_bytes; i += 2) {
        buffer[i] = color_hi;
        buffer[i + 1] = color_lo;
    }
    
    int chunks = (240 * 280) / chunk_pixels;
    for (int i = 0; i < chunks; i++) {
        lcd_data(spi, buffer, chunk_bytes);
    }
    
    free(buffer);
}

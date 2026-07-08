#ifdef ENV_TEST_LCD

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "gc9a01.h"

static const char *TAG = "Test_LCD";

/* Colores RGB565 en el orden del wire (LV_COLOR_16_SWAP = 1 en la app real
 * — acá empujamos bytes directo por SPI a través de gc9a01_fill_screen que
 * ya escribe hi/lo en el orden correcto para el panel). */
static const struct {
    const char *name;
    uint16_t    val;
} COLORS[] = {
    {"ROJO",     0xF800},
    {"VERDE",    0x07E0},
    {"AZUL",     0x001F},
    {"BLANCO",   0xFFFF},
    {"NEGRO",    0x0000},
    {"AMARILLO", 0xFFE0},
    {"CYAN",     0x07FF},
    {"MAGENTA",  0xF81F},
};
#define N_COLORS (sizeof(COLORS) / sizeof(COLORS[0]))

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP_LOGI(TAG, "==============================");
    ESP_LOGI(TAG, " test_lcd — GC9A01 solid fill ");
    ESP_LOGI(TAG, "==============================");

    /* 1) Init del driver: SPI + LEDC + secuencia vendor GC9A01. */
    if (gc9a01_init() != ESP_OK) {
        ESP_LOGE(TAG, "gc9a01_init falló — abortando");
        return;
    }

    /* 2) Backlight al 100%. Esto internamente hace SLPOUT + DISPON, así que
     * si algo falla en el pipeline SPI/init veremos que el panel no
     * enciende antes de llegar al primer fill. */
    gc9a01_set_brightness(100);
    ESP_LOGI(TAG, "Backlight ON — la pantalla debería estar iluminada aunque el buffer sea 0.");

    /* 3) Ciclo infinito de colores sólidos. Cada uno se queda 1.5s antes de
     * cambiar, así el ojo tiene tiempo de confirmar. Si el panel se queda
     * negro y logs siguen imprimiendo, el driver hasta acá está OK pero
     * el CASET/RASET/RAMWR no está pintando (chequear MADCTL/INVON). */
    size_t i = 0;
    while (1) {
        ESP_LOGI(TAG, "Fill %s (0x%04X)", COLORS[i].name, COLORS[i].val);
        gc9a01_fill_screen(COLORS[i].val);
        vTaskDelay(pdMS_TO_TICKS(1500));
        i = (i + 1) % N_COLORS;
    }
}

#endif /* ENV_TEST_LCD */

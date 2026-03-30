#ifdef ENV_TEST_DISPLAY

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
// #include "st7789.h"

static const char *TAG = "Test_ST7789";

void app_main(void) {
    ESP_LOGI(TAG, "Test Pantalla LCD (ST7789)");
    ESP_LOGI(TAG, "Init SPI Bus para Display...");
    ESP_LOGI(TAG, "Imprimiendo un rectángulo verde...");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

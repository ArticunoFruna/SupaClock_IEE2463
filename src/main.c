#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#ifdef ENV_MAIN_APP
static const char *TAG = "SupaClock_Main";

void app_main(void) {
    ESP_LOGI(TAG, "=== INICIANDO SUPACLOCK OS ===");
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
#endif

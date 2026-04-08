#ifdef ENV_TEST_ECG

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "Test_AD8232"; 

void ecg_task(void *pvParameters) {
    ESP_LOGI(TAG, "Init Circuito Front-end ECG AD8232...");
    while (1) {
        ESP_LOGI(TAG, "Leyendo ECG a 100Hz... (Dummy)");
        vTaskDelay(pdMS_TO_TICKS(10)); // ~100 Hz
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Test Módulo ADC/ECG (AD8232)");
    xTaskCreate(ecg_task, "EcgTask", 4096, NULL, 5, NULL);
}
#endif

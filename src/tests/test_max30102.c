#ifdef ENV_TEST_SPO2

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_bus.h"
// #include "max30102.h"


static const char *TAG = "Test_MAX30102";

void spo2_task(void *pvParameters) {
    ESP_LOGI(TAG, "Init MAX30102...");
    while (1) {
        ESP_LOGI(TAG, "Leyendo Oximetría... (Dummy)");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Test Módulo MAX30102 (Oxígeno)");
    if (i2c_master_init() == ESP_OK) {
        xTaskCreate(spo2_task, "SpO2Task", 4096, NULL, 5, NULL);
    }
}
#endif

#ifdef ENV_TEST_IMU

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_bus.h"
// #include "bmi160.h"

static const char *TAG = "Test_BMI160";

void bmi160_task(void *pvParameters) {
    ESP_LOGI(TAG, "Init BMI160...");
    while (1) {
        ESP_LOGI(TAG, "Leyendo IMU... (Dummy)");
        vTaskDelay(pdMS_TO_TICKS(100)); // Simulando 10Hz
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Test Módulo BMI160");
    if (i2c_master_init() == ESP_OK) {
        xTaskCreate(bmi160_task, "BmiTask", 4096, NULL, 5, NULL);
    }
}
#endif

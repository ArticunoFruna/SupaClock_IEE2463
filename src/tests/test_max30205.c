#ifdef ENV_TEST_TEMP

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "max30205.h"

static const char *TAG = "SupaClock_Main";

void temp_sensor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Initializing MAX30205 Temperature Sensor...");
    vTaskDelay(pdMS_TO_TICKS(100));

    if (max30205_init() == ESP_OK) {
        ESP_LOGI(TAG, "MAX30205 Initialized Successfully!");
    } else {
        ESP_LOGE(TAG, "Failed to initialize MAX30205. Check wiring.");
    }

    float temp_c = 0.0;
    while (1) {
        if (max30205_read_temperature(&temp_c) == ESP_OK) {
            ESP_LOGI(TAG, "Body Temperature: %.2f °C", temp_c);
        } else {
            ESP_LOGW(TAG, "Failed to read temperature");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "--- TEST MAX30205 Booting... ---");
    if (i2c_master_init() == ESP_OK) {
        xTaskCreate(temp_sensor_task, "TempSensorTask", 4096, NULL, 5, NULL);
    }
}
#endif

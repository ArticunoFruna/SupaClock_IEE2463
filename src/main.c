#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "max30205.h"

static const char *TAG = "SupaClock_Main";

void temp_sensor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Initializing MAX30205 Temperature Sensor...");
    
    // Allow sensor to power up fully
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
        
        // Muestreo a 1 Hz (Modo Rendimiento Normal)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "--- SupaClock Booting... ---");
    ESP_LOGI(TAG, "Free Heap at Boot: %lu bytes", esp_get_free_heap_size());

    // 1. Inicializar el bus I2C Maestro (GPIO 8 y 9 por defecto en la lib)
    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C Master Bus!");
        return;
    }
    ESP_LOGI(TAG, "I2C Bus Ready.");

    // 2. Crear la tarea para la telemetría del MAX30205
    // Asignamos 4 KB (es un stack size seguro para floats y logs de EPS-IDF) y prioridad media (5)
    xTaskCreate(temp_sensor_task, "TempSensorTask", 4096, NULL, 5, NULL);
    
    // main task can finish here or do other things; FreeRTOS keeps running the spawned task
    ESP_LOGI(TAG, "Scheduler running...");
}

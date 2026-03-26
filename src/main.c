#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "SupaClock";

// Declaración de manejadores (Handles)
SemaphoreHandle_t i2c_bus_mutex = NULL;
QueueHandle_t telemetry_queue = NULL;

void task_acquisition_handler(void *pvParameters) {
    while (1) {
        // Muestreo de sensores I2C y ADC por DMA (ECG)
        // Ejemplo: Bloqueando el Mutex para usar el bus I2C
        if (xSemaphoreTake(i2c_bus_mutex, portMAX_DELAY) == pdTRUE) {
            // Leer MAX30102, BMI160, etc...
            xSemaphoreGive(i2c_bus_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // ~100 Hz
    }
}

void task_processing_handler(void *pvParameters) {
    while (1) {
        // Procesamiento Edge AI (TinyML) y algoritmo podómetro
        vTaskDelay(pdMS_TO_TICKS(50)); // ~20 Hz
    }
}

void task_gui_handler(void *pvParameters) {
    while (1) {
        // Renderizado LCD (Dashboard, Bio, ECG, Menú) y lectura de botones
        vTaskDelay(pdMS_TO_TICKS(50)); // ~20 FPS 
    }
}

void task_ble_handler(void *pvParameters) {
    while (1) {
        // Enviar a la app móvil por Bluetooth Low Energy 
        vTaskDelay(pdMS_TO_TICKS(1000)); // ~1 Hz telemetría normal
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=============== SupaClock FW v0.1 ===============");

    // Inicialización de IPC (Mutexes y Colas)
    i2c_bus_mutex = xSemaphoreCreateMutex();
    if (i2c_bus_mutex == NULL) {
        ESP_LOGE(TAG, "Error al crear el Mutex del bus I2C");
    }

    telemetry_queue = xQueueCreate(10, sizeof(uint32_t)); // Placeholder payload
    if (telemetry_queue == NULL) {
        ESP_LOGE(TAG, "Error al crear la cola de telemetría");
    }

    ESP_LOGI(TAG, "Creando tareas de RTOS...");

    // 1. Tarea de Adquisición (Prioridad Alta)
    xTaskCreatePinnedToCore(task_acquisition_handler, "task_acquisition", 4096, NULL, 5, NULL, 0);

    // 2. Tarea de Procesamiento Edge (Prioridad Media)
    xTaskCreatePinnedToCore(task_processing_handler, "task_processing", 8192, NULL, 3, NULL, 0);

    // 3. Tarea de Interfaz Gráfica (Prioridad Media-Baja)
    xTaskCreatePinnedToCore(task_gui_handler, "task_gui", 4096, NULL, 2, NULL, 0);

    // 4. Tarea de Comunicación BLE (Prioridad Baja)
    xTaskCreatePinnedToCore(task_ble_handler, "task_ble", 4096, NULL, 1, NULL, 0);

    ESP_LOGI(TAG, "Sistema base cargado exitosamente.");
}
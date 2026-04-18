// Reemplazo completo de modulos para Single-Core y MAX30205
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "st7789.h"
#include "i2c_bus.h"
#include "max17048.h"
#include "bmi160.h"
#include "ble_telemetry.h"


#ifdef ENV_MAIN_APP
static const char *TAG = "SupaClock_Main";

void app_main(void) {
    ESP_LOGI(TAG, "=== INICIANDO SUPACLOCK FIRMWARE (PLACEHOLDER) ===");
    ESP_LOGI(TAG, "Por ahora, ejecuta los tests individuales (ej. test_general)");
}
#endif

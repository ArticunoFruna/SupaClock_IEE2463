#ifdef ENV_TEST_HAR

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "i2c_bus.h"
#include "bmi160.h"
#include "har_cnn1d.h"

static const char *TAG = "test_har";

static const char *state_name(har_state_t s) {
    switch (s) {
        case HAR_STATE_RESTING: return "RESTING";
        case HAR_STATE_WALKING: return "WALKING";
        case HAR_STATE_RUNNING: return "RUNNING";
        default:                return "UNKNOWN";
    }
}

static void on_result(const har_result_t *r, void *user) {
    (void)user;
    ESP_LOGI(TAG, "→ %-8s conf=%.2f  p={%.2f,%.2f,%.2f}%s",
             state_name(r->state), r->confidence,
             r->probs[0], r->probs[1], r->probs[2],
             r->fall_event ? "  ⚠ FALL DETECTED" : "");
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP_LOGI(TAG, "=== HAR CNN-1D (running/walking/resting + fall event) ===");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(i2c_master_init());
    if (bmi160_init() != ESP_OK) {
        ESP_LOGE(TAG, "BMI160 no responde — abort");
        return;
    }

    if (har_cnn1d_init(on_result, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "HAR init fallo (¿PSRAM?)");
        return;
    }
    ESP_LOGI(TAG, "HAR arrancado. Arena usada: %u B", (unsigned)har_cnn1d_arena_used());
}

#endif

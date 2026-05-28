#include "app_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stddef.h>

static SemaphoreHandle_t s_sensor_mutex = NULL;
static shared_sensor_data_t s_sensor_data = {0};
static bool s_imu_ble_tx_enabled = true;

void app_state_init(void) {
    if (s_sensor_mutex == NULL) {
        s_sensor_mutex = xSemaphoreCreateMutex();
    }
}

shared_sensor_data_t *app_state_lock(uint32_t timeout_ms) {
    if (s_sensor_mutex == NULL) {
        return NULL;
    }
    if (xSemaphoreTake(s_sensor_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        return &s_sensor_data;
    }
    return NULL;
}

void app_state_unlock(void) {
    if (s_sensor_mutex != NULL) {
        xSemaphoreGive(s_sensor_mutex);
    }
}

bool app_state_imu_tx_enabled(void) {
    return s_imu_ble_tx_enabled;
}

void app_state_set_imu_tx_enabled(bool enabled) {
    s_imu_ble_tx_enabled = enabled;
}

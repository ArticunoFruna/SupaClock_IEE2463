#ifndef BLE_TELEMETRY_H
#define BLE_TELEMETRY_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Initialize BLE Server and GAP/GATT roles for SupaClock
 *        (Implementation depends on CONFIG_BT_NIMBLE_ENABLED)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_telemetry_init(void);

/**
 * @brief Send payload via BLE notification
 * @param data Array of 32-bit values (e.g., [heart_rate, step_count, ecg_sample])
 * @param count Number of elements in data array
 * @return esp_err_t ESP_OK if sent successfully
 */
esp_err_t ble_telemetry_send(uint32_t *data, size_t count);

#endif // BLE_TELEMETRY_H

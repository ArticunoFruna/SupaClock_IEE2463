#ifndef BLE_TELEMETRY_H
#define BLE_TELEMETRY_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Paquete de sensores lentos (temperatura, batería, pasos)
 * Enviado a ~1 Hz por la característica 0xFF02
 */
typedef struct __attribute__((packed)) {
    int16_t  temperature_x100;  /**< Temperatura corporal °C × 100 */
    uint16_t steps_hw;          /**< BMI160 HW step counter        */
    uint32_t steps_sw;          /**< Software step counter         */
    uint16_t battery_mv;        /**< Voltaje batería en mV         */
    uint8_t  battery_soc;       /**< SOC en %                      */
} ble_sensor_packet_t;          /* 11 bytes total                  */

/**
 * @brief Initialize BLE Server and GAP/GATT roles for SupaClock using NimBLE
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_telemetry_init(void);

/**
 * @brief Send IMU payload via BLE notification (UUID 0xFF01, alta frecuencia)
 * @param data Array of 16-bit values (ax, ay, az, gx, gy, gz)
 * @param length Size in bytes of the array payload (expecting 12 bytes)
 * @return esp_err_t ESP_OK if sent successfully
 */
esp_err_t ble_telemetry_send(int16_t *data, size_t length);

/**
 * @brief Send slow sensor data via BLE notification (UUID 0xFF02, baja frecuencia)
 * @param pkt Pointer to sensor packet (temp, steps, battery)
 * @return esp_err_t ESP_OK if sent successfully
 */
esp_err_t ble_telemetry_send_sensors(const ble_sensor_packet_t *pkt);

#endif // BLE_TELEMETRY_H

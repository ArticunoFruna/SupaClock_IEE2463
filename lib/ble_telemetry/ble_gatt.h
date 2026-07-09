#ifndef BLE_GATT_H
#define BLE_GATT_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* UUIDs de la superficie GATT del SupaClock (mismo esquema que la app Flutter). */
#define BLE_UUID_SVC_TELEMETRY  0xFF00
#define BLE_UUID_CHR_IMU        0xFF01
#define BLE_UUID_CHR_AGG        0xFF02
#define BLE_UUID_CHR_ECG        0xFF03
#define BLE_UUID_CHR_CMD        0xFF04

/* Registra los servicios GATT. Llamar tras ble_svc_gatt_init(). */
esp_err_t ble_gatt_init(void);

/* Getters de los value handles (para ble_gatts_notify_custom). */
uint16_t ble_gatt_handle_imu(void);
uint16_t ble_gatt_handle_agg(void);
uint16_t ble_gatt_handle_ecg(void);
uint16_t ble_gatt_handle_cmd(void);

/* Habilita/deshabilita el flag de "cliente subscrito" por característica.
 * Llamado desde el dispatcher GAP en BLE_GAP_EVENT_SUBSCRIBE. */
void ble_gatt_set_subscribe(uint16_t attr_handle, bool enabled);

/* Limpia todas las subscripciones (llamar en disconnect). */
void ble_gatt_clear_subscriptions(void);

/* True si el peer está actualmente subscrito a la característica dada por su UUID16. */
bool ble_gatt_can_notify(uint16_t uuid16);

#endif /* BLE_GATT_H */

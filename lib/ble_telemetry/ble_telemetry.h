#ifndef BLE_TELEMETRY_H
#define BLE_TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/* ════════════════════════════════════════════════════════════════
 *  Características BLE (UUIDs 16-bit dentro del servicio 0xFF00)
 *
 *  0xFF01  IMU 6-DOF — 12 bytes por notificación, ritmo según modo
 *  0xFF02  Telemetría agregada TLV (HR, SpO2, temp, batería, eventos)
 *  0xFF03  ECG streaming (chunks)
 *  0xFF04  Comandos RX (1 byte)
 * ════════════════════════════════════════════════════════════════ */

/* ─────────── Tipos TLV para característica 0xFF02 ─────────── */
/* Cada record TLV: uint8_t type; uint8_t len; uint8_t data[len]
 * Todos los offsets en el data son little-endian. */

#define BLE_TLV_TYPE_HR          0x01  /**< 4 B: u16 delta_ms; u8 bpm; u8 quality       */
#define BLE_TLV_TYPE_SPO2        0x02  /**< 4 B: u16 delta_ms; u8 pct; u8 quality       */
#define BLE_TLV_TYPE_TEMP        0x03  /**< 4 B: u16 delta_ms; i16 temp_x100            */
#define BLE_TLV_TYPE_BAT         0x04  /**< 5 B: u16 delta_ms; u16 mv;     u8 soc       */
#define BLE_TLV_TYPE_STEPS       0x05  /**< 4 B: u32 total_steps                        */
#define BLE_TLV_TYPE_MODE_EVT    0x06  /**< 1 B: u8 new_mode (0=SPORT 1=NORM 2=SAVER)   */
#define BLE_TLV_TYPE_SPOT_RESULT 0x07  /**< 6 B: u8 bpm; u8 spo2; u16 dur_ms;
                                            u8 quality; u8 aborted                     */
#define BLE_TLV_TYPE_HAR_STATE   0x08  /**< 1 B: u8 state (0=reposo 1=caminar
                                            2=correr 3=escaleras) consolidado HAR      */
#define BLE_TLV_TYPE_PED_DBG     0x10  /**< 13 B debug pedómetro (quitar tras calibrar):
                                            u16 amp; u16 prom_x10; u8 ratio_x100;
                                            u8 pkhz_x10; u8 cad_x10; u8 gate; u8 cons;
                                            u32 steps                                  */

/**
 * @brief Header del paquete agregado.
 *
 * El paquete completo es: header + payload_len bytes de records TLV.
 * Tamaño máximo recomendado: < 200 B (deja margen sobre MTU típico de 247).
 */
typedef struct __attribute__((packed)) {
    uint32_t boot_ts_ms;    /**< ms desde boot del header (base para deltas) */
    uint8_t  power_mode;    /**< Modo activo al momento del flush            */
    uint8_t  payload_len;   /**< Bytes de records TLV que siguen             */
} ble_agg_header_t;

/**
 * @brief Inicializa stack BLE NimBLE + GATT + advertising.
 *
 * También crea internamente el buffer/queue de agregación. Debe llamarse
 * después de nvs_flash_init() y antes de cualquier ble_tx_push().
 */
esp_err_t ble_telemetry_init(void);

/**
 * @brief Envía un paquete IMU (12 B raw int16: ax,ay,az,gx,gy,gz).
 *
 * Camino directo, NO pasa por buffer. Llamar desde imu_task con la
 * cadencia del modo activo (50 Hz / 25 Hz / 12.5 Hz).
 */
esp_err_t ble_telemetry_send_imu(int16_t *data, size_t length);

/* Alias histórico — mantener para no romper código de tests existentes. */
esp_err_t ble_telemetry_send(int16_t *data, size_t length);

/**
 * @brief Envía burst de muestras ECG (chunks de N int16).
 *
 * Camino directo, NO pasa por buffer. Streaming en vivo.
 */
esp_err_t ble_telemetry_send_ecg(int16_t *data, size_t length);

/**
 * @brief Apila un record TLV en el buffer agregado (no bloquea).
 *
 * El consumidor (ble_tx_task del firmware) decide cuándo flushea según
 * el perfil de energía. Si el buffer queda lleno, fuerza un flush
 * implícito (con modo desconocido = 0xFF) antes de apilar; se asume
 * que el siguiente flush manual emitirá el modo correcto.
 *
 * @param flush_now_mode si != 0xFF, fuerza flush inmediato tras apilar
 *                       usando este valor de power_mode en el header
 *                       (útil para resultados de SPOT o cambios de modo).
 *                       Pasar 0xFF para no forzar flush.
 *
 * @return ESP_OK si se apiló, ESP_ERR_INVALID_ARG si data_len > 250.
 */
esp_err_t ble_tx_push(uint8_t type, const void *data, uint8_t data_len,
                      uint8_t flush_now_mode);

/**
 * @brief Flushea el buffer agregado: emite la notificación 0xFF02 con
 *        header + records TLV pendientes y vacía el buffer.
 *
 * @param power_mode_val Valor del campo power_mode del header (lo conoce el caller).
 *
 * Llamar desde ble_tx_task según el período del perfil. No-op si el
 * buffer está vacío.
 */
esp_err_t ble_tx_flush(uint8_t power_mode_val);

/* ─────────── Modo ECG (comando bidireccional) ─────────── */

bool ble_telemetry_is_ecg_mode_active(void);
void ble_telemetry_set_ecg_mode(bool enable);

/* ─────────── Bonding / Pairing ─────────── */

/** True si hay al menos un peer bondeado guardado en NVS. */
bool ble_telemetry_is_paired(void);

/** Borra todos los bonds en NVS. Se puede invocar en runtime. */
void ble_telemetry_erase_bonds(void);

/**
 * Registra un callback para el comando SYNC_TIME (opcode 0x02).
 * El callback recibe el timestamp unix (segundos, u32 LE) enviado por la app.
 * Pasar NULL desregistra.
 */
void ble_telemetry_set_time_sync_cb(void (*cb)(uint32_t unix_ts));

#endif /* BLE_TELEMETRY_H */

#ifndef BLE_CMD_H
#define BLE_CMD_H

#include <stdint.h>
#include <stddef.h>

/* Opcodes del canal 0xFF04.
 *
 * Formato del WRITE:
 *   [opcode:1][len:1][payload:len]
 *
 * Retrocompatibilidad: si el WRITE trae exactamente 1 byte, se interpreta
 * como opcode sin payload (formato viejo, sólo 0x00/0x01 legacy).
 */
#define BLE_CMD_STOP_ECG    0x00
#define BLE_CMD_START_ECG   0x01
#define BLE_CMD_SYNC_TIME   0x02  /* payload: u32 unix_ts LE */
#define BLE_CMD_UNPAIR_ALL  0x03
#define BLE_CMD_REQ_BAT     0x04

typedef struct {
    void (*on_start_ecg)(void);
    void (*on_stop_ecg)(void);
    void (*on_sync_time)(uint32_t unix_ts);
    void (*on_unpair)(void);
    void (*on_req_bat)(void);
} ble_cmd_callbacks_t;

/* Registra los callbacks para cada opcode. Pasar NULL en un slot lo deja no-op. */
void ble_cmd_register(const ble_cmd_callbacks_t *cbs);

/* Decodifica una escritura completa del CMD channel y dispara el callback. */
void ble_cmd_dispatch(const uint8_t *data, uint16_t len);

#endif /* BLE_CMD_H */

#include "ble_cmd.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "BLE_CMD";

static ble_cmd_callbacks_t s_cbs;

void ble_cmd_register(const ble_cmd_callbacks_t *cbs) {
    if (cbs) s_cbs = *cbs;
    else memset(&s_cbs, 0, sizeof(s_cbs));
}

static void handle(uint8_t opcode, const uint8_t *payload, uint16_t payload_len) {
    switch (opcode) {
        case BLE_CMD_START_ECG:
            ESP_LOGI(TAG, "CMD START_ECG");
            if (s_cbs.on_start_ecg) s_cbs.on_start_ecg();
            break;
        case BLE_CMD_STOP_ECG:
            ESP_LOGI(TAG, "CMD STOP_ECG");
            if (s_cbs.on_stop_ecg) s_cbs.on_stop_ecg();
            break;
        case BLE_CMD_SYNC_TIME:
            if (payload_len != 4) {
                ESP_LOGW(TAG, "SYNC_TIME payload len=%u (esperaba 4)", payload_len);
                return;
            }
            uint32_t ts;
            memcpy(&ts, payload, 4);
            ESP_LOGI(TAG, "CMD SYNC_TIME ts=%u", (unsigned)ts);
            if (s_cbs.on_sync_time) s_cbs.on_sync_time(ts);
            break;
        case BLE_CMD_UNPAIR_ALL:
            ESP_LOGI(TAG, "CMD UNPAIR_ALL");
            if (s_cbs.on_unpair) s_cbs.on_unpair();
            break;
        case BLE_CMD_REQ_BAT:
            ESP_LOGI(TAG, "CMD REQ_BAT");
            if (s_cbs.on_req_bat) s_cbs.on_req_bat();
            break;
        default:
            ESP_LOGW(TAG, "Opcode desconocido 0x%02X", opcode);
            break;
    }
}

void ble_cmd_dispatch(const uint8_t *data, uint16_t len) {
    if (!data || len == 0) return;

    /* Retrocompat: escritura de 1 byte legacy = opcode sin payload. */
    if (len == 1) {
        handle(data[0], NULL, 0);
        return;
    }

    /* Formato nuevo: [opcode][len][payload...] */
    uint8_t opcode = data[0];
    uint8_t plen   = data[1];
    if ((uint16_t)plen + 2 > len) {
        ESP_LOGW(TAG, "Longitud invalida opcode=0x%02X plen=%u write_len=%u",
                 opcode, plen, len);
        return;
    }
    handle(opcode, plen > 0 ? &data[2] : NULL, plen);
}

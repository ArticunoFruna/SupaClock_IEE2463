#include "ble_telemetry.h"
#include "ble_bond.h"
#include "ble_cmd.h"
#include "ble_gap.h"
#include "ble_gatt.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE_TELEMETRY";

#define BLE_APPEARANCE_WATCH  0x00C1

/* ════════════════════════════════════════════════════════════════
 *  Estado runtime
 * ════════════════════════════════════════════════════════════════ */
static bool s_ecg_mode_active = false;

static void (*s_time_sync_cb)(uint32_t) = NULL;

/* Buffer de agregación TLV. */
#define AGG_BUF_MAX 200
static uint8_t s_agg_buf[AGG_BUF_MAX];
static uint8_t s_agg_len = 0;
static uint32_t s_agg_base_ts_ms = 0;
static SemaphoreHandle_t s_agg_mtx = NULL;

/* ════════════════════════════════════════════════════════════════
 *  Comandos entrantes -> callbacks
 * ════════════════════════════════════════════════════════════════ */
static void cmd_start_ecg(void) {
    s_ecg_mode_active = true;
    ESP_LOGI(TAG, "ECG_MODE INICIADO (via BLE cmd)");
}
static void cmd_stop_ecg(void) {
    s_ecg_mode_active = false;
    ESP_LOGI(TAG, "ECG_MODE DETENIDO (via BLE cmd)");
}
static void cmd_sync_time(uint32_t ts) {
    if (s_time_sync_cb) s_time_sync_cb(ts);
    else ESP_LOGW(TAG, "SYNC_TIME sin callback registrado (ts=%u)", (unsigned)ts);
}
static void cmd_unpair(void) {
    ble_bond_erase_all();
}
static void cmd_req_bat(void) {
    /* No-op por ahora: el sampler de batería del firmware flushea periodicamente. */
}

/* ════════════════════════════════════════════════════════════════
 *  Host task
 * ════════════════════════════════════════════════════════════════ */
static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ════════════════════════════════════════════════════════════════
 *  API pública — init
 * ════════════════════════════════════════════════════════════════ */
esp_err_t ble_telemetry_init(void) {
    ESP_LOGI(TAG, "Init BLE (NimBLE + bonding NVS Just Works)...");

    s_agg_mtx = xSemaphoreCreateMutex();
    if (!s_agg_mtx) return ESP_ERR_NO_MEM;
    s_agg_len = 0;
    s_agg_base_ts_ms = 0;

    esp_err_t rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init rc=%d", rc);
        return rc;
    }

    /* Bonding + SM (registra ble_store_config_init dentro). */
    ble_bond_init();

    /* Servicios base + tabla GATT.
     * NOTA: ble_svc_gap_init() setea el device_name al valor del Kconfig
     * (default "nimble"). Por eso supa_gap_init va DESPUÉS: sobreescribe el
     * nombre y appearance con los nuestros. */
    ble_svc_gap_init();
    ble_svc_gatt_init();
    if (ble_gatt_init() != ESP_OK) return ESP_FAIL;

    /* GAP: nombre, appearance, callbacks host, arranca adv fast en on_sync. */
    supa_gap_init("SupaClock_BLE", BLE_APPEARANCE_WATCH);

    /* Callbacks del canal de comandos. */
    ble_cmd_callbacks_t cbs = {
        .on_start_ecg = cmd_start_ecg,
        .on_stop_ecg  = cmd_stop_ecg,
        .on_sync_time = cmd_sync_time,
        .on_unpair    = cmd_unpair,
        .on_req_bat   = cmd_req_bat,
    };
    ble_cmd_register(&cbs);

    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE stack listo. Paired=%d", ble_bond_is_paired());
    return ESP_OK;
}

/* ════════════════════════════════════════════════════════════════
 *  Envíos directos IMU / ECG
 * ════════════════════════════════════════════════════════════════ */
esp_err_t ble_telemetry_send_imu(int16_t *data, size_t length) {
    if (s_ecg_mode_active) return ESP_OK;
    uint16_t conn = supa_gap_get_conn_handle();
    if (conn == BLE_HS_CONN_HANDLE_NONE) return ESP_FAIL;
    if (!ble_gatt_can_notify(BLE_UUID_CHR_IMU)) return ESP_ERR_INVALID_STATE;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
    if (!om) return ESP_FAIL;
    int rc = ble_gatts_notify_custom(conn, ble_gatt_handle_imu(), om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t ble_telemetry_send(int16_t *data, size_t length) {
    return ble_telemetry_send_imu(data, length);
}

esp_err_t ble_telemetry_send_ecg(int16_t *data, size_t length) {
    uint16_t conn = supa_gap_get_conn_handle();
    if (conn == BLE_HS_CONN_HANDLE_NONE) return ESP_FAIL;
    if (!ble_gatt_can_notify(BLE_UUID_CHR_ECG)) return ESP_ERR_INVALID_STATE;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
    if (!om) return ESP_FAIL;
    int rc = ble_gatts_notify_custom(conn, ble_gatt_handle_ecg(), om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

bool ble_telemetry_is_ecg_mode_active(void) { return s_ecg_mode_active; }

void ble_telemetry_set_ecg_mode(bool enable) {
    s_ecg_mode_active = enable;
    ESP_LOGI(TAG, "ECG_MODE %s (via firmware)", enable ? "INICIADO" : "DETENIDO");
}

/* ════════════════════════════════════════════════════════════════
 *  Buffer TLV agregado (0xFF02)
 * ════════════════════════════════════════════════════════════════ */
static esp_err_t agg_emit_locked(uint8_t power_mode_val) {
    if (s_agg_len == 0) return ESP_OK;

    uint16_t conn = supa_gap_get_conn_handle();
    if (conn == BLE_HS_CONN_HANDLE_NONE ||
        !ble_gatt_can_notify(BLE_UUID_CHR_AGG) ||
        s_ecg_mode_active) {
        /* Sin cliente subscrito o en modo ECG: drop para no acumular. */
        s_agg_len = 0;
        s_agg_base_ts_ms = 0;
        return ESP_OK;
    }

    uint8_t pkt[sizeof(ble_agg_header_t) + AGG_BUF_MAX];
    ble_agg_header_t hdr = {
        .boot_ts_ms  = s_agg_base_ts_ms,
        .power_mode  = power_mode_val,
        .payload_len = s_agg_len,
    };
    memcpy(pkt, &hdr, sizeof(hdr));
    memcpy(pkt + sizeof(hdr), s_agg_buf, s_agg_len);
    size_t total = sizeof(hdr) + s_agg_len;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(pkt, total);
    if (om) {
        ble_gatts_notify_custom(conn, ble_gatt_handle_agg(), om);
    }

    s_agg_len = 0;
    s_agg_base_ts_ms = 0;
    return ESP_OK;
}

esp_err_t ble_tx_push(uint8_t type, const void *data, uint8_t data_len,
                      uint8_t flush_now_mode) {
    if (data_len > 250) return ESP_ERR_INVALID_ARG;
    if (!s_agg_mtx) return ESP_ERR_INVALID_STATE;

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    xSemaphoreTake(s_agg_mtx, portMAX_DELAY);

    if (s_agg_len == 0) s_agg_base_ts_ms = now_ms;

    size_t needed = (size_t)2 + data_len;
    if ((size_t)s_agg_len + needed > AGG_BUF_MAX) {
        agg_emit_locked(0xFF);
        s_agg_base_ts_ms = now_ms;
    }

    s_agg_buf[s_agg_len++] = type;
    s_agg_buf[s_agg_len++] = data_len;
    if (data_len > 0 && data) {
        memcpy(&s_agg_buf[s_agg_len], data, data_len);
        s_agg_len += data_len;
    }

    if (flush_now_mode != 0xFF) {
        agg_emit_locked(flush_now_mode);
    }

    xSemaphoreGive(s_agg_mtx);
    return ESP_OK;
}

esp_err_t ble_tx_flush(uint8_t power_mode_val) {
    if (!s_agg_mtx) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_agg_mtx, portMAX_DELAY);
    agg_emit_locked(power_mode_val);
    xSemaphoreGive(s_agg_mtx);
    return ESP_OK;
}

/* ════════════════════════════════════════════════════════════════
 *  Bonding / pairing
 * ════════════════════════════════════════════════════════════════ */
bool ble_telemetry_is_paired(void) { return ble_bond_is_paired(); }
void ble_telemetry_erase_bonds(void) { ble_bond_erase_all(); }
void ble_telemetry_set_time_sync_cb(void (*cb)(uint32_t)) { s_time_sync_cb = cb; }

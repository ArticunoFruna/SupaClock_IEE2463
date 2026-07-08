#include "ble_gatt.h"
#include "ble_cmd.h"
#include "esp_log.h"

#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE_GATT";

static uint16_t s_imu_handle;
static uint16_t s_agg_handle;
static uint16_t s_ecg_handle;
static uint16_t s_cmd_handle;

/* Flags booleanos de subscripción por característica notify. */
static bool s_sub_imu;
static bool s_sub_agg;
static bool s_sub_ecg;

static int chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def s_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_UUID_SVC_TELEMETRY),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHR_IMU),
                .access_cb = chr_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_imu_handle,
            },
            {
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHR_AGG),
                .access_cb = chr_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_agg_handle,
            },
            {
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHR_ECG),
                .access_cb = chr_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_ecg_handle,
            },
            {
                .uuid = BLE_UUID16_DECLARE(BLE_UUID_CHR_CMD),
                .access_cb = chr_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &s_cmd_handle,
            },
            { 0 }
        },
    },
    { 0 },
};

static int chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR && attr_handle == s_cmd_handle) {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        if (om_len == 0 || om_len > 32) return 0;

        uint8_t buf[32];
        if (os_mbuf_copydata(ctxt->om, 0, om_len, buf) != 0) return 0;
        ble_cmd_dispatch(buf, om_len);
    }
    return 0;
}

esp_err_t ble_gatt_init(void) {
    if (ble_gatts_count_cfg(s_svcs) != 0) return ESP_FAIL;
    if (ble_gatts_add_svcs(s_svcs) != 0) return ESP_FAIL;
    ESP_LOGI(TAG, "GATT: svc 0x%04X | IMU(0x%04X) AGG(0x%04X) ECG(0x%04X) CMD(0x%04X)",
             BLE_UUID_SVC_TELEMETRY, BLE_UUID_CHR_IMU, BLE_UUID_CHR_AGG,
             BLE_UUID_CHR_ECG, BLE_UUID_CHR_CMD);
    return ESP_OK;
}

uint16_t ble_gatt_handle_imu(void) { return s_imu_handle; }
uint16_t ble_gatt_handle_agg(void) { return s_agg_handle; }
uint16_t ble_gatt_handle_ecg(void) { return s_ecg_handle; }
uint16_t ble_gatt_handle_cmd(void) { return s_cmd_handle; }

void ble_gatt_set_subscribe(uint16_t attr_handle, bool enabled) {
    if (attr_handle == s_imu_handle) s_sub_imu = enabled;
    else if (attr_handle == s_agg_handle) s_sub_agg = enabled;
    else if (attr_handle == s_ecg_handle) s_sub_ecg = enabled;
    else return;
    ESP_LOGI(TAG, "CCCD handle=0x%04X notify=%d", attr_handle, enabled ? 1 : 0);
}

void ble_gatt_clear_subscriptions(void) {
    s_sub_imu = false;
    s_sub_agg = false;
    s_sub_ecg = false;
}

bool ble_gatt_can_notify(uint16_t uuid16) {
    switch (uuid16) {
        case BLE_UUID_CHR_IMU: return s_sub_imu;
        case BLE_UUID_CHR_AGG: return s_sub_agg;
        case BLE_UUID_CHR_ECG: return s_sub_ecg;
        default: return false;
    }
}

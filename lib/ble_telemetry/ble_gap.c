#include "ble_gap.h"
#include "ble_gatt.h"
#include "esp_log.h"
#include <string.h>

#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "BLE_GAP";

/* Duración del modo fast antes de caer a slow (ms). */
#define ADV_FAST_DURATION_MS  30000

/* Intervalos en unidades de 0.625 ms. */
#define ADV_FAST_ITVL_MIN   160   /* 100 ms */
#define ADV_FAST_ITVL_MAX   480   /* 300 ms */
#define ADV_SLOW_ITVL_MIN   1600  /* 1000 ms */
#define ADV_SLOW_ITVL_MAX   1920  /* 1200 ms */

static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_effective_mtu = 0;
static uint16_t s_appearance = 0;

static int gap_event_cb(struct ble_gap_event *event, void *arg);

static void start_adv(bool fast) {
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};
    struct ble_hs_adv_fields rsp = {0};
    uint8_t own_addr_type;
    int rc;

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto rc=%d", rc);
        return;
    }

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.appearance = s_appearance;
    fields.appearance_is_present = 1;

    const char *name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields rc=%d", rc);
        return;
    }

    rsp.uuids16 = (ble_uuid16_t[]) { BLE_UUID16_INIT(0xFF00) };
    rsp.num_uuids16 = 1;
    rsp.uuids16_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&rsp);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_rsp_set_fields rc=%d", rc);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min  = fast ? ADV_FAST_ITVL_MIN : ADV_SLOW_ITVL_MIN;
    adv_params.itvl_max  = fast ? ADV_FAST_ITVL_MAX : ADV_SLOW_ITVL_MAX;

    int32_t duration = fast ? ADV_FAST_DURATION_MS : BLE_HS_FOREVER;

    rc = ble_gap_adv_start(own_addr_type, NULL, duration, &adv_params,
                            gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start rc=%d (fast=%d own=%d)", rc, fast, own_addr_type);
    } else {
        ESP_LOGI(TAG, "Advertising %s: itvl=%d-%d units, name=\"%s\"",
                 fast ? "FAST" : "SLOW",
                 adv_params.itvl_min, adv_params.itvl_max, name);
    }
}

void supa_gap_start_adv_fast(void) { start_adv(true); }
void supa_gap_start_adv_slow(void) { start_adv(false); }

static int gap_event_cb(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                s_conn_handle = event->connect.conn_handle;
                ESP_LOGI(TAG, "Conectado (handle=0x%04X)", s_conn_handle);
                /* Iniciamos el intercambio de MTU. La app puede pedir 247. */
                ble_gattc_exchange_mtu(s_conn_handle, NULL, NULL);
            } else {
                ESP_LOGW(TAG, "Fallo conexion status=%d, reanudando adv", event->connect.status);
                supa_gap_start_adv_fast();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Desconectado reason=%d, reanudando adv fast",
                     event->disconnect.reason);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_effective_mtu = 0;
            ble_gatt_clear_subscriptions();
            supa_gap_start_adv_fast();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            /* Timeout del fast adv: pasamos a slow forever. */
            ESP_LOGI(TAG, "Adv complete reason=%d -> slow", event->adv_complete.reason);
            supa_gap_start_adv_slow();
            break;

        case BLE_GAP_EVENT_MTU:
            s_effective_mtu = event->mtu.value;
            ESP_LOGI(TAG, "MTU negociado = %u", s_effective_mtu);
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ble_gatt_set_subscribe(event->subscribe.attr_handle,
                                   event->subscribe.cur_notify != 0);
            break;

        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.status == 0) {
                struct ble_gap_conn_desc desc;
                if (ble_gap_conn_find(event->enc_change.conn_handle, &desc) == 0) {
                    ESP_LOGI(TAG, "Cifrado OK, bonded=%d encrypted=%d authenticated=%d",
                             desc.sec_state.bonded, desc.sec_state.encrypted,
                             desc.sec_state.authenticated);
                }
            } else {
                ESP_LOGW(TAG, "Cifrado fallo status=%d", event->enc_change.status);
            }
            break;

        case BLE_GAP_EVENT_REPEAT_PAIRING: {
            struct ble_gap_conn_desc desc;
            if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
                ble_store_util_delete_peer(&desc.peer_id_addr);
            }
            return BLE_GAP_REPEAT_PAIRING_RETRY;
        }
    }
    return 0;
}

static void on_reset(int reason) {
    ESP_LOGE(TAG, "BLE reset reason=%d", reason);
}

static void on_sync(void) {
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ensure_addr rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "Stack sincronizado; arrancando adv fast");
    supa_gap_start_adv_fast();
}

esp_err_t supa_gap_init(const char *device_name, uint16_t appearance) {
    s_appearance = appearance;

    ble_hs_cfg.reset_cb        = on_reset;
    ble_hs_cfg.sync_cb         = on_sync;
    ble_hs_cfg.gatts_register_cb = NULL;

    ble_svc_gap_device_name_set(device_name);
    ble_svc_gap_device_appearance_set(appearance);
    return ESP_OK;
}

uint16_t supa_gap_get_conn_handle(void) { return s_conn_handle; }
uint16_t supa_gap_get_mtu(void) { return s_effective_mtu; }

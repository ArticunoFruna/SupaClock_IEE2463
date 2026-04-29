#include "ble_telemetry.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

/* NimBLE Includes */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE_TELEMETRY";

static uint16_t conn_handle_active = BLE_HS_CONN_HANDLE_NONE;
static uint16_t imu_chr_val_handle;     /* 0xFF01 — IMU 6-DOF      */
static uint16_t agg_chr_val_handle;     /* 0xFF02 — telemetría TLV */
static uint16_t ecg_chr_val_handle;     /* 0xFF03 — ECG streaming  */
static uint16_t cmd_chr_val_handle;     /* 0xFF04 — comandos RX    */

static bool ecg_mode_active = false;

#define IMU_SVC_UUID       0xFF00
#define IMU_CHR_UUID       0xFF01
#define AGG_CHR_UUID       0xFF02
#define ECG_CHR_UUID       0xFF03
#define CMD_CHR_UUID       0xFF04

#define BLE_APPEARANCE_WATCH  0x00C1

/* ════════════════════════════════════════════════════════════════
 *  Buffer de agregación TLV
 * ════════════════════════════════════════════════════════════════ */

#define AGG_BUF_MAX 200  /* deja margen sobre MTU típico de 247 */

static uint8_t s_agg_buf[AGG_BUF_MAX];
static uint8_t s_agg_len = 0;             /* bytes ocupados de payload */
static uint32_t s_agg_base_ts_ms = 0;     /* timestamp base del primer record */
static SemaphoreHandle_t s_agg_mtx = NULL;

/* Forward declaraciones */
static int chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ble_telemetry_gap_event(struct ble_gap_event *event, void *arg);
static esp_err_t agg_emit_locked(uint8_t power_mode_val);

/* ════════════════════════════════════════════════════════════════
 *  GATT Table
 * ════════════════════════════════════════════════════════════════ */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(IMU_SVC_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(IMU_CHR_UUID),
                .access_cb = chr_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &imu_chr_val_handle,
            },
            {
                .uuid = BLE_UUID16_DECLARE(AGG_CHR_UUID),
                .access_cb = chr_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &agg_chr_val_handle,
            },
            {
                .uuid = BLE_UUID16_DECLARE(ECG_CHR_UUID),
                .access_cb = chr_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &ecg_chr_val_handle,
            },
            {
                .uuid = BLE_UUID16_DECLARE(CMD_CHR_UUID),
                .access_cb = chr_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &cmd_chr_val_handle,
            },
            { 0 }
        },
    },
    { 0 },
};

static int chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (attr_handle == cmd_chr_val_handle) {
            uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
            if (om_len > 0) {
                uint8_t cmd_val;
                int rc = os_mbuf_copydata(ctxt->om, 0, 1, &cmd_val);
                if (rc == 0) {
                    if (cmd_val == 0x01) {
                        ecg_mode_active = true;
                        ESP_LOGI(TAG, "Comando recibido: INICIAR ECG_MODE");
                    } else if (cmd_val == 0x00) {
                        ecg_mode_active = false;
                        ESP_LOGI(TAG, "Comando recibido: DETENER ECG_MODE");
                    }
                }
            }
        }
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════
 *  Advertising / GAP
 * ════════════════════════════════════════════════════════════════ */
static void ble_telemetry_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields rsp_fields;
    const char *name;

    memset(&fields, 0, sizeof fields);
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.appearance = BLE_APPEARANCE_WATCH;
    fields.appearance_is_present = 1;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    memset(&rsp_fields, 0, sizeof rsp_fields);
    rsp_fields.uuids16 = (ble_uuid16_t[]) { BLE_UUID16_INIT(IMU_SVC_UUID) };
    rsp_fields.num_uuids16 = 1;
    rsp_fields.uuids16_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp_fields);

    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params,
                      ble_telemetry_gap_event, NULL);
}

static int ble_telemetry_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Bluetooth Conectado!");
                conn_handle_active = event->connect.conn_handle;
                ble_gap_security_initiate(conn_handle_active);
            } else {
                ESP_LOGW(TAG, "Fallo conexion BLE, reenviando publicidad...");
                ble_telemetry_advertise();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Bluetooth Desconectado! Volviendo a advertir...");
            conn_handle_active = BLE_HS_CONN_HANDLE_NONE;
            ble_telemetry_advertise();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "GATT Subscribe (handle: 0x%04X)",
                     event->subscribe.attr_handle);
            break;

        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.status == 0) {
                ESP_LOGI(TAG, "Enlace cifrado exitosamente (bonded)");
            } else {
                ESP_LOGW(TAG, "Cifrado fallido: status=%d", event->enc_change.status);
            }
            break;

        case BLE_GAP_EVENT_REPEAT_PAIRING: {
            struct ble_gap_conn_desc desc;
            ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            ble_store_util_delete_peer(&desc.peer_id_addr);
            return BLE_GAP_REPEAT_PAIRING_RETRY;
        }
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════
 *  Host callbacks
 * ════════════════════════════════════════════════════════════════ */
static void ble_telemetry_on_reset(int reason) {
    ESP_LOGE(TAG, "BLE Reset, razon: %d", reason);
}

static void ble_telemetry_on_sync(void) {
    ble_telemetry_advertise();
}

static void ble_telemetry_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ════════════════════════════════════════════════════════════════
 *  API Pública — init + envíos directos
 * ════════════════════════════════════════════════════════════════ */

esp_err_t ble_telemetry_init(void) {
    ESP_LOGI(TAG, "Inicializando stack BLE (NimBLE) con 4 características...");

    s_agg_mtx = xSemaphoreCreateMutex();
    if (!s_agg_mtx) return ESP_ERR_NO_MEM;
    s_agg_len = 0;
    s_agg_base_ts_ms = 0;

    esp_err_t rc = nimble_port_init();
    if (rc != ESP_OK) return rc;

    ble_hs_cfg.reset_cb = ble_telemetry_on_reset;
    ble_hs_cfg.sync_cb = ble_telemetry_on_sync;
    ble_hs_cfg.gatts_register_cb = NULL;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Sin pairing (compat BlueZ + tests) */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;
    ble_hs_cfg.sm_our_key_dist = 0;
    ble_hs_cfg.sm_their_key_dist = 0;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    if (ble_gatts_count_cfg(gatt_svr_svcs) != 0) return ESP_FAIL;
    if (ble_gatts_add_svcs(gatt_svr_svcs) != 0) return ESP_FAIL;

    ble_svc_gap_device_name_set("SupaClock_BLE");
    ble_svc_gap_device_appearance_set(BLE_APPEARANCE_WATCH);

    nimble_port_freertos_init(ble_telemetry_host_task);

    ESP_LOGI(TAG, "BLE: Servicio 0x%04X | IMU(0x%04X) Agg(0x%04X) ECG(0x%04X) Cmd(0x%04X)",
             IMU_SVC_UUID, IMU_CHR_UUID, AGG_CHR_UUID, ECG_CHR_UUID, CMD_CHR_UUID);
    return ESP_OK;
}

esp_err_t ble_telemetry_send_imu(int16_t *data, size_t length) {
    if (ecg_mode_active) return ESP_OK;
    if (conn_handle_active == BLE_HS_CONN_HANDLE_NONE) return ESP_FAIL;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
    if (!om) return ESP_FAIL;
    int rc = ble_gatts_notify_custom(conn_handle_active, imu_chr_val_handle, om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t ble_telemetry_send(int16_t *data, size_t length) {
    return ble_telemetry_send_imu(data, length);
}

esp_err_t ble_telemetry_send_ecg(int16_t *data, size_t length) {
    if (conn_handle_active == BLE_HS_CONN_HANDLE_NONE) return ESP_FAIL;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
    if (!om) return ESP_FAIL;
    int rc = ble_gatts_notify_custom(conn_handle_active, ecg_chr_val_handle, om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

bool ble_telemetry_is_ecg_mode_active(void) { return ecg_mode_active; }

void ble_telemetry_set_ecg_mode(bool enable) {
    ecg_mode_active = enable;
    ESP_LOGI(TAG, "ECG_MODE %s (vía firmware)", enable ? "INICIADO" : "DETENIDO");
}

/* ════════════════════════════════════════════════════════════════
 *  API agregada (TLV)
 * ════════════════════════════════════════════════════════════════ */

/* Emite el contenido del buffer y lo limpia. Llamar con mutex tomado.
 * Si no hay conexión BLE, igual limpia (evita acumular para siempre). */
static esp_err_t agg_emit_locked(uint8_t power_mode_val) {
    if (s_agg_len == 0) return ESP_OK;

    if (conn_handle_active == BLE_HS_CONN_HANDLE_NONE || ecg_mode_active) {
        /* Drop: sin cliente o estamos en ECG. Limpiamos para no crecer. */
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
        ble_gatts_notify_custom(conn_handle_active, agg_chr_val_handle, om);
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
        /* Buffer lleno: flush implícito con modo desconocido (0xFF).
         * El siguiente flush manual ya pondrá el modo correcto. */
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

#include "ble_telemetry.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// NimBLE Includes
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE_TELEMETRY";

static uint16_t conn_handle_active = BLE_HS_CONN_HANDLE_NONE;
static uint16_t imu_chr_val_handle;     /* UUID 0xFF01 — IMU 6-DOF (alta freq) */
static uint16_t sensor_chr_val_handle;  /* UUID 0xFF02 — Sensors (baja freq)   */

// Custom 16-bit UUIDs para simplificar en Bleak
#define IMU_SVC_UUID       0xFF00
#define IMU_CHR_UUID       0xFF01
#define SENSOR_CHR_UUID    0xFF02

// Forward declaracion de handlers
static int chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ble_telemetry_gap_event(struct ble_gap_event *event, void *arg);

/* ═══════════════════ GATT Table ═══════════════════ */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(IMU_SVC_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /* Característica 1: IMU 6-DOF (ax,ay,az,gx,gy,gz) — 12 bytes */
                .uuid = BLE_UUID16_DECLARE(IMU_CHR_UUID),
                .access_cb = chr_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &imu_chr_val_handle,
            },
            {
                /* Característica 2: Sensores lentos (temp,steps,battery) — 11 bytes */
                .uuid = BLE_UUID16_DECLARE(SENSOR_CHR_UUID),
                .access_cb = chr_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &sensor_chr_val_handle,
            },
            {
                0, // No more characteristics
            }
        },
    },
    {
        0, // No more services
    },
};

static int chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    // Las lecturas READ devuelven vacío; usamos NOTIFY para todo
    return 0; 
}

/* ═══════════════════ Advertising ═══════════════════ */
static void ble_telemetry_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;

    memset(&fields, 0, sizeof fields);
    
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    
    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, ble_telemetry_gap_event, NULL);
}

/* ═══════════════════ GAP Events ═══════════════════ */
static int ble_telemetry_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Bluetooth Conectado!");
                conn_handle_active = event->connect.conn_handle;
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
            ESP_LOGI(TAG, "GATT Subscribe event (handle: 0x%04X)", event->subscribe.attr_handle);
            break;
    }
    return 0;
}

/* ═══════════════════ Host callbacks ═══════════════════ */
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

/* ═══════════════════ API Pública ═══════════════════ */

esp_err_t ble_telemetry_init(void) {
    ESP_LOGI(TAG, "Inicializando stack BLE (NimBLE) con 2 características...");
    
    esp_err_t rc = nimble_port_init();
    if (rc != ESP_OK) return rc;

    ble_hs_cfg.reset_cb = ble_telemetry_on_reset;
    ble_hs_cfg.sync_cb = ble_telemetry_on_sync;
    ble_hs_cfg.gatts_register_cb = NULL;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    
    if (ble_gatts_count_cfg(gatt_svr_svcs) != 0) return ESP_FAIL;
    if (ble_gatts_add_svcs(gatt_svr_svcs) != 0) return ESP_FAIL;

    ble_svc_gap_device_name_set("SupaClock_BLE");

    nimble_port_freertos_init(ble_telemetry_host_task);

    ESP_LOGI(TAG, "BLE: Servicio 0x%04X con chr IMU(0x%04X) + Sensors(0x%04X)",
             IMU_SVC_UUID, IMU_CHR_UUID, SENSOR_CHR_UUID);
    return ESP_OK;
}

esp_err_t ble_telemetry_send(int16_t *data, size_t length) {
    if (conn_handle_active != BLE_HS_CONN_HANDLE_NONE) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
        if (om) {
            int rc = ble_gatts_notify_custom(conn_handle_active, imu_chr_val_handle, om);
            if (rc == 0) {
                return ESP_OK;
            }
        }
    }
    return ESP_FAIL;
}

esp_err_t ble_telemetry_send_sensors(const ble_sensor_packet_t *pkt) {
    if (pkt == NULL) return ESP_ERR_INVALID_ARG;

    if (conn_handle_active != BLE_HS_CONN_HANDLE_NONE) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(pkt, sizeof(ble_sensor_packet_t));
        if (om) {
            int rc = ble_gatts_notify_custom(conn_handle_active, sensor_chr_val_handle, om);
            if (rc == 0) {
                return ESP_OK;
            }
        }
    }
    return ESP_FAIL;
}

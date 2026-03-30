#ifdef ENV_TEST_BLE

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"

// Includes de NimBLE
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

// Almacenamiento de claves (Bonding)
#include "host/ble_store.h"

static const char *TAG = "SupaClock_BLE";

// ============ Handles de Características ============
uint16_t hr_handle;
uint16_t temp_handle;
uint16_t steps_handle;
uint16_t active_conn_handle = BLE_HS_CONN_HANDLE_NONE;

// ============ Callbacks de Acceso GATT ============
static int ble_devinfo_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    const char *str = (const char *)arg;
    os_mbuf_append(ctxt->om, str, strlen(str));
    return 0;
}

static int ble_telemetry_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return 0; // Notificaciones solamente
}

// ============ Tabla de Servicios GATT ============
static const struct ble_gatt_svc_def gatt_svcs[] = {
    // --- Servicio 1: Device Information (0x180A) ---
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180A),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {   // Manufacturer Name
                .uuid = BLE_UUID16_DECLARE(0x2A29),
                .access_cb = ble_devinfo_access,
                .arg = (void *)"ArticunoFruna",
                .flags = BLE_GATT_CHR_F_READ,
            },
            {   // Model Number
                .uuid = BLE_UUID16_DECLARE(0x2A24),
                .access_cb = ble_devinfo_access,
                .arg = (void *)"SupaClock-C3",
                .flags = BLE_GATT_CHR_F_READ,
            },
            {0}
        }
    },
    // --- Servicio 2: SupaClock Telemetry ---
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {   // HR & SpO2
                .uuid = BLE_UUID16_DECLARE(0x0001),
                .access_cb = ble_telemetry_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &hr_handle,
            },
            {   // Temperatura
                .uuid = BLE_UUID16_DECLARE(0x0002),
                .access_cb = ble_telemetry_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &temp_handle,
            },
            {   // Pasos
                .uuid = BLE_UUID16_DECLARE(0x0003),
                .access_cb = ble_telemetry_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &steps_handle,
            },
            {0}
        }
    },
    {0}
};

// ============ Tarea del Corazón Sintético ============
void dummy_telemetry_task(void *pvParameters) {
    uint8_t dummy_hr = 80;
    uint8_t dummy_spo2 = 98;
    int16_t dummy_temp = 3650;
    uint32_t dummy_steps = 1500;

    while(1) {
        if (active_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            dummy_hr += (esp_random() % 3) - 1;
            if(dummy_hr < 60) dummy_hr = 60;
            if(dummy_hr > 120) dummy_hr = 120;
            dummy_temp += (esp_random() % 3) - 1;
            dummy_steps += 1;

            uint8_t hr_payload[4] = {dummy_hr, dummy_spo2, 1, 0};
            struct os_mbuf *om_hr = ble_hs_mbuf_from_flat(hr_payload, 4);
            ble_gatts_notify_custom(active_conn_handle, hr_handle, om_hr);

            struct os_mbuf *om_temp = ble_hs_mbuf_from_flat(&dummy_temp, 2);
            ble_gatts_notify_custom(active_conn_handle, temp_handle, om_temp);

            struct os_mbuf *om_steps = ble_hs_mbuf_from_flat(&dummy_steps, 4);
            ble_gatts_notify_custom(active_conn_handle, steps_handle, om_steps);

            ESP_LOGD(TAG, "Notif sent: HR=%d T=%.2f Steps=%lu", 
                     dummy_hr, dummy_temp/100.0, (unsigned long)dummy_steps);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ============ Eventos GAP ============
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Conexión establecida");
            active_conn_handle = event->connect.conn_handle;
        } else {
            ESP_LOGE(TAG, "Error de conexión: %d", event->connect.status);
            extern void ble_app_on_sync(void);
            ble_app_on_sync();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Desconectado. Reactivando publicidad...");
        active_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        extern void ble_app_on_sync(void);
        ble_app_on_sync();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Publicidad completada");
        extern void ble_app_on_sync(void);
        ble_app_on_sync();
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

// ============ Publicidad (Dividida) ============
void ble_app_on_sync(void) {
    int rc;
    uint8_t own_addr_type;
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error indicando tipo de dirección: %d", rc);
        return;
    }

    // 1. Datos de Publicidad (Principal): Flags + Appearance + Service UUID
    struct ble_hs_adv_fields adv_fields;
    memset(&adv_fields, 0, sizeof(adv_fields));

    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.appearance = 0x00C1; // Wristwatch
    adv_fields.appearance_is_present = 1;

    adv_fields.uuids128 = (ble_uuid128_t[]){ BLE_UUID128_INIT(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                             0x00, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12) };
    adv_fields.num_uuids128 = 1;
    adv_fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error configurando adv_fields: %d", rc);
        return;
    }

    // 2. Scan Response: Nombre del dispositivo
    struct ble_hs_adv_fields rsp_fields;
    memset(&rsp_fields, 0, sizeof(rsp_fields));
    rsp_fields.name = (uint8_t *)"SupaClock";
    rsp_fields.name_len = 9;
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error configurando scan_rsp: %d", rc);
        return;
    }

    // Arrancar publicidad
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error iniciando publicidad: %d", rc);
    } else {
        ESP_LOGI(TAG, "Publicitando SupaClock (Appearance: Wristwatch)");
    }
}

void host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// ============ Inicialización ============
void app_main(void) {
    ESP_LOGI(TAG, "Iniciando SupaClock BLE corrigiendo visibilidad...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    nimble_port_init();

    // Importante: Inicializar configuración de almacenamiento para Bonding
    // En ESP-IDF, ble_store_config_init() registra automáticamente los callbacks de NVS
    void ble_store_config_init(void);
    ble_store_config_init();
    
    // Configurar identidad
    ble_hs_util_ensure_addr(0);
    ble_svc_gap_device_name_set("SupaClock");

    // Servicios base
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Nuestros servicios
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);

    // Seguridad
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;

    nimble_port_freertos_init(host_task);
    xTaskCreate(dummy_telemetry_task, "MockTask", 4096, NULL, 5, NULL);
}
#endif

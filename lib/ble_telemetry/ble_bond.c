#include "ble_bond.h"
#include "esp_log.h"
#include <string.h>

#include "host/ble_hs.h"
#include "host/ble_store.h"

/* store_config vive en el componente bt. No hay header público expuesto,
 * declaramos el prototipo tal cual test_ble.c lo hace. */
void ble_store_config_init(void);

static const char *TAG = "BLE_BOND";

esp_err_t ble_bond_init(void) {
    /* Just Works: sin passkey, sin display, sin MITM. */
    ble_hs_cfg.sm_io_cap         = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding        = 1;
    ble_hs_cfg.sm_sc             = 1;
    ble_hs_cfg.sm_mitm           = 0;
    ble_hs_cfg.sm_our_key_dist   = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.store_status_cb   = ble_store_util_status_rr;

    /* Registra los callbacks NVS del store por defecto de NimBLE.
     * A partir de aquí los bonds se persisten automáticamente. */
    ble_store_config_init();

    ESP_LOGI(TAG, "Bonding Just Works activo (SC=1, MITM=0, NVS persist)");
    return ESP_OK;
}

bool ble_bond_is_paired(void) {
    int count = 0;
    if (ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC, &count) != 0) return false;
    return count > 0;
}

bool ble_bond_get_last_peer(ble_addr_t *out) {
    if (!out) return false;
    ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS];
    int num = 0;
    if (ble_store_util_bonded_peers(peers, &num,
                                    CONFIG_BT_NIMBLE_MAX_BONDS) != 0) {
        return false;
    }
    if (num <= 0) return false;
    memcpy(out, &peers[num - 1], sizeof(*out));
    return true;
}

void ble_bond_erase_all(void) {
    int rc = ble_store_clear();
    if (rc == 0) {
        ESP_LOGI(TAG, "Bonds borrados de NVS");
    } else {
        ESP_LOGW(TAG, "ble_store_clear rc=%d", rc);
    }
}

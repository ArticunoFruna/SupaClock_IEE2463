#ifndef BLE_BOND_H
#define BLE_BOND_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "host/ble_hs.h"

/*
 * Configuración de bonding Just Works persistente en NVS.
 *
 * Requiere en sdkconfig:
 *   CONFIG_BT_NIMBLE_NVS_PERSIST=y
 *   CONFIG_BT_NIMBLE_SM_SC=y
 *   CONFIG_BT_NIMBLE_HANDLE_REPEAT_PAIRING_DELETION=y
 *
 * Debe llamarse DESPUÉS de nimble_port_init() y ANTES de ble_svc_gap_init().
 */
esp_err_t ble_bond_init(void);

/* True si hay al menos un peer bondeado en NVS. */
bool ble_bond_is_paired(void);

/* Copia la dirección del último peer bondeado en *out. Devuelve false si no hay. */
bool ble_bond_get_last_peer(ble_addr_t *out);

/* Borra TODOS los bonds guardados en NVS. Se puede llamar en runtime. */
void ble_bond_erase_all(void);

#endif /* BLE_BOND_H */

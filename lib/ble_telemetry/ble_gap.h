#ifndef BLE_GAP_H
#define BLE_GAP_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "host/ble_hs.h"

/*
 * Setea host callbacks (sync_cb, reset_cb) y guarda nombre / appearance.
 * Debe llamarse tras ble_svc_gap_init(). NO arranca advertising: eso lo
 * hace el sync_cb interno cuando el stack termina de arrancar.
 */
esp_err_t supa_gap_init(const char *device_name, uint16_t appearance);

/* Fuerza el arranque de advertising fast (100-300 ms, 30 s de duración,
 * undirected general). Al terminar sin conexión cae a slow automáticamente. */
void supa_gap_start_adv_fast(void);

/* Advertising lento sin límite (1000-1200 ms, undirected general). */
void supa_gap_start_adv_slow(void);

/* Handle de la conexión activa, o BLE_HS_CONN_HANDLE_NONE. */
uint16_t supa_gap_get_conn_handle(void);

/* MTU efectivo negociado con el peer (0 si no hubo negociación). */
uint16_t supa_gap_get_mtu(void);

#endif /* BLE_GAP_H */

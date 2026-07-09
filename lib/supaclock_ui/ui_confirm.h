/**
 * @file ui_confirm.h
 * @brief Helper para modales de confirmación en la UI.
 */
#ifndef UI_CONFIRM_H
#define UI_CONFIRM_H

#include "lvgl.h"

typedef void (*ui_confirm_cb_t)(void);

/**
 * @brief Abre un modal centrado de confirmación sobre el screen activo.
 * @param msg Mensaje descriptivo (ej: "¿Resetear pasos?").
 * @param ok_cb Callback que se ejecuta si se presiona OK.
 */
void ui_confirm_open(const char *msg, ui_confirm_cb_t ok_cb);

#endif /* UI_CONFIRM_H */

#ifndef SUPACLOCK_UI_H
#define SUPACLOCK_UI_H

#include <stdint.h>
#include "gpio_buttons.h"

typedef struct {
    void (*on_power_off)(void);
    void (*on_battery_reset)(void);
} ui_actions_t;

/**
 * @brief Inicializa la UI, crea todas las pantallas e inicializa el driver de LVGL.
 */
void ui_init(void);

/**
 * @brief Configura las acciones de hardware que la UI puede disparar (apagado, reset de batería).
 */
void ui_set_actions(const ui_actions_t *actions);

/**
 * @brief Envía eventos de botones para ser procesados por la lógica de navegación de la UI.
 */
void ui_handle_button(btn_event_t ev);

/**
 * @brief Marca actividad de touch (llamado por el read_cb del indev cuando
 *        detecta un press). Sirve para que el gui_task refresque el contador
 *        de auto-off y despierte el backlight.
 */
void ui_notify_touch_activity(void);

/**
 * @brief Lee el flag de actividad de touch de forma atómica y lo limpia.
 * @return true si hubo actividad desde la última llamada.
 */
bool ui_take_and_clear_touch_activity(void);

/**
 * @brief Ejecuta el loop periódico de refresco de datos y dibujo de LVGL.
 * @return Tiempo recomendado de delay en milisegundos para el siguiente tick.
 */
uint32_t ui_tick(void);

#endif // SUPACLOCK_UI_H

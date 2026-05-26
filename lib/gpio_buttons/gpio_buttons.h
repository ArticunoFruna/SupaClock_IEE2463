#ifndef GPIO_BUTTONS_H
#define GPIO_BUTTONS_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include "supaclock_pinmap.h"

/* ══════════════════ Pines (XIAO ESP32-S3 — carrier v1) ══════════════════
 * BTN_NEXT  = GPIO43  (era U0TXD; consola se va a USB-Serial-JTAG)
 * BTN_SELECT = GPIO8
 * Ver supaclock_pinmap.h para el mapa completo.
 */
#define BTN_NEXT_PIN    SUPA_PIN_BTN_NEXT
#define BTN_SELECT_PIN  SUPA_PIN_BTN_SELECT

/* INT1 del BMI160 — no cableado en el carrier v1 (HAR usa polling). */
#define BMI160_INT1_PIN SUPA_PIN_BMI160_INT1

/* ══════════════════ Eventos ══════════════════ */
typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_NEXT_SHORT,
    BTN_EVENT_NEXT_LONG,
    BTN_EVENT_SELECT_SHORT,
    BTN_EVENT_SELECT_LONG,
} btn_event_t;

/**
 * @brief Configurar GPIO 10/21 como entradas con pull-up (activo bajo).
 * @return ESP_OK si todo OK.
 */
esp_err_t gpio_buttons_init(void);

/**
 * @brief Poll del estado de los botones. Llamar periódicamente desde
 *        la GUI task (~ cada 30 ms).
 *
 * Debounce interno (2 lecturas consecutivas), distingue pulsación corta
 * y larga (>600 ms). Por llamada devuelve UN evento a lo sumo (cola corta);
 * si ocurren dos eventos en el mismo tick, se serializan en las siguientes
 * llamadas.
 */
btn_event_t gpio_buttons_poll(void);

#endif // GPIO_BUTTONS_H

#ifndef GPIO_BUTTONS_H
#define GPIO_BUTTONS_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/* ══════════════════ Pines (ESP32-C3 SuperMini) ══════════════════
 * Usamos los dos únicos GPIOs libres que NO colisionan con UART0:
 *   GPIO 10 = BTN_NEXT    (avanza pantalla / scrollea menú)
 *   GPIO 1 = BTN_SELECT  (acción contextual: toggle ECG / activar ítem)
 * GPIO 20 queda reservado (UART0 RX). */
#define BTN_NEXT_PIN    10
#define BTN_SELECT_PIN  1

/* Pin del IMU INT1 (BMI160) — no es botón pero lo inicializamos aquí */
#define BMI160_INT1_PIN -1

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

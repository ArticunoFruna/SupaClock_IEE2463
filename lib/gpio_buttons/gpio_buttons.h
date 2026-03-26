#ifndef GPIO_BUTTONS_H
#define GPIO_BUTTONS_H

#include "esp_err.h"
#include <stdint.h>

// Pines de botones (ESP32-C3)
#define BTN_SEL_PIN   10
#define BTN_UP_PIN    20
#define BTN_DOWN_PIN  21

#define BMI160_INT1_PIN 1

typedef enum {
    BTN_EVENT_NONE = ...
} btn_event_t;

/**
 * @brief Initialize general GPIO for buttons and IMU int
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gpio_buttons_init(void);

#endif // GPIO_BUTTONS_H

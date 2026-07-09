#ifndef CST816S_H
#define CST816S_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Gesture codes del datasheet CST816S */
#define CST816S_GESTURE_NONE        0x00
#define CST816S_GESTURE_SWIPE_UP    0x01
#define CST816S_GESTURE_SWIPE_DOWN  0x02
#define CST816S_GESTURE_SWIPE_LEFT  0x03
#define CST816S_GESTURE_SWIPE_RIGHT 0x04
#define CST816S_GESTURE_SINGLE_TAP  0x05
#define CST816S_GESTURE_DOUBLE_TAP  0x0B
#define CST816S_GESTURE_LONG_PRESS  0x0C

typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
    uint8_t gesture;
} cst816s_touch_t;

/**
 * @brief Inicializa el CST816S sobre el bus I2C existente (`i2c_bus`).
 *        Configura RST y INT, pulsea reset y registra ISR falling-edge.
 * @param int_pin GPIO de la línea INT (falling-edge en un touch nuevo).
 * @param rst_pin GPIO de la línea RST (active-low). Pasar -1 si no cableado.
 */
esp_err_t cst816s_init(int int_pin, int rst_pin);

/**
 * @brief Devuelve el último estado conocido del touch.
 *        Si hubo INT desde la última llamada, hace la lectura I2C y refresca
 *        el estado. Entre INTs devuelve el último snapshot cacheado — así
 *        LVGL ve pressed sostenido mientras el dedo esté abajo.
 * @return true siempre (la lectura nunca bloquea).
 */
bool cst816s_read(cst816s_touch_t *out);

/**
 * @brief Fuerza al CST816S a entrar en su modo de más bajo consumo (Shutdown)
 *        manteniendo la línea de Reset en nivel bajo.
 */
void cst816s_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* CST816S_H */

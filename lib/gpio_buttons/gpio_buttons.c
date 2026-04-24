#include "gpio_buttons.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "GPIO_BUTTONS";

#define LONG_PRESS_US   (600 * 1000)   /* 600 ms */
#define QUEUE_DEPTH     4

typedef struct {
    int      pin;
    bool     pressed;          /* estado actual tras debounce */
    bool     long_fired;       /* ya emitimos evento LONG en esta pulsación */
    int      last_raw;         /* última lectura cruda */
    int      stable_raw;       /* estado estable (tras debounce) */
    int64_t  press_start_us;
    btn_event_t ev_short;
    btn_event_t ev_long;
} btn_state_t;

static btn_state_t s_next = {
    .pin = BTN_NEXT_PIN, .ev_short = BTN_EVENT_NEXT_SHORT, .ev_long = BTN_EVENT_NEXT_LONG,
    .stable_raw = 1, .last_raw = 1,
};
static btn_state_t s_select = {
    .pin = BTN_SELECT_PIN, .ev_short = BTN_EVENT_SELECT_SHORT, .ev_long = BTN_EVENT_SELECT_LONG,
    .stable_raw = 1, .last_raw = 1,
};

/* Cola simple de eventos para no perderlos si caen dos en un mismo tick */
static btn_event_t s_queue[QUEUE_DEPTH];
static uint8_t     s_q_head = 0, s_q_tail = 0;

static void q_push(btn_event_t ev) {
    uint8_t next = (s_q_tail + 1) % QUEUE_DEPTH;
    if (next == s_q_head) return; /* cola llena → descartar */
    s_queue[s_q_tail] = ev;
    s_q_tail = next;
}

static btn_event_t q_pop(void) {
    if (s_q_head == s_q_tail) return BTN_EVENT_NONE;
    btn_event_t ev = s_queue[s_q_head];
    s_q_head = (s_q_head + 1) % QUEUE_DEPTH;
    return ev;
}

esp_err_t gpio_buttons_init(void) {
    gpio_config_t btn_cfg = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BTN_NEXT_PIN) | (1ULL << BTN_SELECT_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    esp_err_t err = gpio_config(&btn_cfg);
    if (err != ESP_OK) return err;

    /* BMI160 INT1 — solo entrada, no usamos ISR todavía */
    gpio_config_t int_cfg = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BMI160_INT1_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&int_cfg);

    ESP_LOGI(TAG, "Botones listos: NEXT=GPIO%d  SELECT=GPIO%d (activo-bajo, pull-up)",
             BTN_NEXT_PIN, BTN_SELECT_PIN);
    return ESP_OK;
}

/* Debounce + detección de corto/largo por botón. */
static void update_button(btn_state_t *b, int64_t now_us) {
    int raw = gpio_get_level(b->pin);

    /* Debounce por doble-lectura: solo confirmamos cambio si dos lecturas
     * seguidas coinciden y difieren del estado estable. */
    if (raw == b->last_raw && raw != b->stable_raw) {
        b->stable_raw = raw;

        if (raw == 0) {
            /* Flanco descendente — inicio de pulsación */
            b->pressed = true;
            b->long_fired = false;
            b->press_start_us = now_us;
        } else {
            /* Flanco ascendente — soltamos */
            if (b->pressed && !b->long_fired) {
                /* Soltado antes del umbral de long → es corto */
                q_push(b->ev_short);
            }
            b->pressed = false;
        }
    }
    b->last_raw = raw;

    /* Disparo de LONG mientras sigue presionado */
    if (b->pressed && !b->long_fired &&
        (now_us - b->press_start_us) >= LONG_PRESS_US) {
        b->long_fired = true;
        q_push(b->ev_long);
    }
}

btn_event_t gpio_buttons_poll(void) {
    int64_t now = esp_timer_get_time();
    update_button(&s_next, now);
    update_button(&s_select, now);
    return q_pop();
}

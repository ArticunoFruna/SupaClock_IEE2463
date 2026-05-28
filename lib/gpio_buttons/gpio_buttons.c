#include "gpio_buttons.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"

static const char *TAG = "GPIO_BUTTONS";

#define LONG_PRESS_US   (600 * 1000)   /* 600 ms */
#define DEBOUNCE_MAX    4              /* Límite del integrador para el promedio (ej. 4 * 10ms = 40ms) */
#define QUEUE_DEPTH     8

typedef struct {
    int      pin;
    bool     pressed;          /* estado actual tras debounce */
    bool     long_fired;       /* ya emitimos evento LONG en esta pulsación */
    int      integrator;       /* integrador para el debounce por promedio */
    int      stable_raw;       /* estado estable (tras debounce) */
    int64_t  press_start_us;
    btn_event_t ev_short;
    btn_event_t ev_long;
} btn_state_t;

static btn_state_t s_next = {
    .pin = BTN_NEXT_PIN, .ev_short = BTN_EVENT_NEXT_SHORT, .ev_long = BTN_EVENT_NEXT_LONG,
    .stable_raw = 1, .integrator = DEBOUNCE_MAX,
};
static btn_state_t s_select = {
    .pin = BTN_SELECT_PIN, .ev_short = BTN_EVENT_SELECT_SHORT, .ev_long = BTN_EVENT_SELECT_LONG,
    .stable_raw = 1, .integrator = DEBOUNCE_MAX,
};

/* Cola simple de eventos para no perderlos si caen dos en un mismo tick */
static btn_event_t s_queue[QUEUE_DEPTH];
static volatile uint8_t s_q_head = 0, s_q_tail = 0;

static void q_push(btn_event_t ev) {
    uint8_t next = (s_q_tail + 1) % QUEUE_DEPTH;
    if (next != s_q_head) { /* no descartamos si no está llena */
        s_queue[s_q_tail] = ev;
        s_q_tail = next;
    }
}

static btn_event_t q_pop(void) {
    if (s_q_head != s_q_tail) {
        btn_event_t ev = s_queue[s_q_head];
        s_q_head = (s_q_head + 1) % QUEUE_DEPTH;
        return ev;
    }
    return BTN_EVENT_NONE;
}

static void btn_timer_cb(void *arg);
static esp_timer_handle_t s_btn_timer = NULL;

esp_err_t gpio_buttons_init(void) {
    /* En XIAO ESP32-S3 BTN_NEXT=GPIO43 (U0TXD) y SPI_CS=GPIO44 (U0RXD).
     * Si la consola se quedó en UART0, esos pines no están como GPIO.
     * Forzamos el IO MUX a GPIO antes de configurarlos.
     * (Inocuo si la consola ya está en USB-Serial-JTAG.) */
    esp_rom_gpio_pad_select_gpio(BTN_NEXT_PIN);
    esp_rom_gpio_connect_out_signal(BTN_NEXT_PIN, SIG_GPIO_OUT_IDX, false, false);

    gpio_config_t btn_cfg = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BTN_NEXT_PIN) | (1ULL << BTN_SELECT_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    esp_err_t err = gpio_config(&btn_cfg);
    if (err != ESP_OK) return err;

    /* BMI160 INT1 — no cableada en el carrier v1 (BMI160_INT1_PIN = -1).
     * Si en una rev futura se cablea, descomentar y poner el pin real. */
#if BMI160_INT1_PIN >= 0
    gpio_config_t int_cfg = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BMI160_INT1_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&int_cfg);
#endif

    esp_timer_create_args_t timer_args = {
        .callback = &btn_timer_cb,
        .name = "btn_debounce_timer",
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .skip_unhandled_events = true,
    };
    esp_err_t tmr_err = esp_timer_create(&timer_args, &s_btn_timer);
    if (tmr_err == ESP_OK) {
        esp_timer_start_periodic(s_btn_timer, 10 * 1000); // 10ms
    } else {
        ESP_LOGE(TAG, "Failed to create button timer");
        return tmr_err;
    }

    ESP_LOGI(TAG, "Botones listos: NEXT=GPIO%d  SELECT=GPIO%d (activo-bajo, pull-up)",
             BTN_NEXT_PIN, BTN_SELECT_PIN);
    return ESP_OK;
}

/* Debounce robusto mediante un integrador (promedio móvil) + detección de corto/largo. */
static void update_button(btn_state_t *b, int64_t now_us) {
    int raw = gpio_get_level(b->pin);

    /* Integrador: suma si es 1, resta si es 0 */
    if (raw == 1) {
        if (b->integrator < DEBOUNCE_MAX) b->integrator++;
    } else {
        if (b->integrator > 0) b->integrator--;
    }

    /* Evaluamos el estado estable basándonos en el integrador */
    int new_stable = b->stable_raw;
    if (b->integrator == 0) {
        new_stable = 0; // Estado estable bajo (presionado)
    } else if (b->integrator >= DEBOUNCE_MAX) {
        new_stable = 1; // Estado estable alto (suelto)
    }

    if (new_stable != b->stable_raw) {
        b->stable_raw = new_stable;

        if (new_stable == 0) {
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

    /* Disparo de LONG mientras sigue presionado */
    if (b->pressed && !b->long_fired &&
        (now_us - b->press_start_us) >= LONG_PRESS_US) {
        b->long_fired = true;
        q_push(b->ev_long);
    }
}
static void btn_timer_cb(void *arg) {
    int64_t now = esp_timer_get_time();
    update_button(&s_next, now);
    update_button(&s_select, now);
}

btn_event_t gpio_buttons_poll(void) {
    return q_pop();
}

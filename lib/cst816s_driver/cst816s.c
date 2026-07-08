#include "cst816s.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "supaclock_pinmap.h"
#include <string.h>

static const char *TAG = "CST816S";

static SemaphoreHandle_t s_irq_sem = NULL;
static int s_int_pin = -1;
static int s_rst_pin = -1;

/* Última lectura cacheada. LVGL lee a 60 Hz y mantenemos "pressed" hasta
 * que llegue la próxima INT — típico patrón para controladores capacitivos
 * que solo emiten INT en cambios de estado. */
static cst816s_touch_t s_last = {0};
static bool s_initialized = false;

static void IRAM_ATTR cst816s_isr(void *arg) {
    (void)arg;
    BaseType_t hp_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_irq_sem, &hp_task_woken);
    if (hp_task_woken == pdTRUE) portYIELD_FROM_ISR();
}

static void hw_reset(void) {
    if (s_rst_pin < 0) return;
    gpio_set_level(s_rst_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(s_rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

esp_err_t cst816s_init(int int_pin, int rst_pin) {
    s_int_pin = int_pin;
    s_rst_pin = rst_pin;

    /* RST como salida (idle high) */
    if (rst_pin >= 0) {
        gpio_config_t rst_cfg = {
            .pin_bit_mask = 1ULL << rst_pin,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&rst_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "gpio_config RST failed: %s", esp_err_to_name(err));
            return err;
        }
        gpio_set_level(rst_pin, 1);
    }

    /* INT como input con pull-up + falling-edge */
    if (int_pin < 0) {
        ESP_LOGE(TAG, "INT pin requerido");
        return ESP_ERR_INVALID_ARG;
    }
    gpio_config_t int_cfg = {
        .pin_bit_mask = 1ULL << int_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    esp_err_t err = gpio_config(&int_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config INT failed: %s", esp_err_to_name(err));
        return err;
    }

    s_irq_sem = xSemaphoreCreateBinary();
    if (!s_irq_sem) return ESP_ERR_NO_MEM;

    /* Instalar el ISR service es idempotente — si ya está instalado por otro
     * driver ignoramos el error. */
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(err));
        return err;
    }
    err = gpio_isr_handler_add(int_pin, cst816s_isr, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed: %s", esp_err_to_name(err));
        return err;
    }

    hw_reset();

    /* Ping opcional al chip para detectar presencia (chip ID en reg 0xA7). */
    uint8_t chip_id = 0;
    if (i2c_read_bytes(SUPA_I2C_ADDR_TOUCH, 0xA7, &chip_id, 1) != ESP_OK) {
        ESP_LOGW(TAG, "no response en 0x%02X (touch quizá ausente)", SUPA_I2C_ADDR_TOUCH);
        /* No abortamos — el sistema sigue funcional con botones. */
    } else {
        ESP_LOGI(TAG, "chip_id=0x%02X init OK (INT=%d RST=%d)", chip_id, int_pin, rst_pin);
    }

    s_initialized = true;
    return ESP_OK;
}

bool cst816s_read(cst816s_touch_t *out) {
    if (!out) return false;
    if (!s_initialized) {
        memset(out, 0, sizeof(*out));
        return true;
    }

    /* Si hubo INT desde la última lectura, refrescamos el cache. */
    if (xSemaphoreTake(s_irq_sem, 0) == pdTRUE) {
        uint8_t buf[6];
        if (i2c_read_bytes(SUPA_I2C_ADDR_TOUCH, 0x01, buf, 6) == ESP_OK) {
            uint8_t gesture = buf[0];
            uint8_t fingers = buf[1];
            uint16_t x = ((uint16_t)(buf[2] & 0x0F) << 8) | buf[3];
            uint16_t y = ((uint16_t)(buf[4] & 0x0F) << 8) | buf[5];
            s_last.gesture = gesture;
            s_last.pressed = (fingers > 0);
            if (s_last.pressed) {
                if (x < 240) s_last.x = x;
                if (y < 240) s_last.y = y;
            }
        }
    }

    *out = s_last;
    return true;
}

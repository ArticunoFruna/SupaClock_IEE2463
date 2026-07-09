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
    ESP_LOGI(TAG, "init INT=GPIO%d RST=GPIO%d addr=0x%02X", int_pin, rst_pin, SUPA_I2C_ADDR_TOUCH);

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

    ESP_LOGI(TAG, "hw_reset (RST pulse low 10ms then high, settle 50ms)");
    hw_reset();

    /* Ping al chip: chip ID en reg 0xA7, firmware version en 0xA9. Algunos
     * clones responden 0x00 en 0xA7 pero sí en 0xA8/0xA9 — leemos varios. */
    uint8_t regs[4] = {0};
    err = i2c_read_bytes(SUPA_I2C_ADDR_TOUCH, 0xA7, regs, 4);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no response en 0x%02X: %s (touch quizá ausente o mal cableado)",
                 SUPA_I2C_ADDR_TOUCH, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "chip_id=0x%02X proj_id=0x%02X fw_ver=0x%02X 0x%02X — init OK",
                 regs[0], regs[1], regs[2], regs[3]);
    }

    /* Desactivar auto-sleep del CST816S (default entra en LPM tras 2s sin
     * toque y deja de ACKear I2C, floodeando ESP_FAIL en la UI). Registro
     * 0xFE = "DisableAutoSleep": escribir 0xFF lo mantiene siempre despierto.
     * Costo: pocos µA extra, muy tolerable para un wearable donde el touch
     * es feature principal. Si el chip está en modo LPM al iniciar (no
     * responde), reintenamos tras un delay para darle chance de despertar. */
    uint8_t disable_lpm = 0xFF;
    for (int attempt = 0; attempt < 3; attempt++) {
        err = i2c_write_bytes(SUPA_I2C_ADDR_TOUCH, 0xFE, &disable_lpm, 1);
        if (err == ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "auto-sleep deshabilitado (reg 0xFE = 0xFF)");
    } else {
        ESP_LOGW(TAG, "no se pudo escribir 0xFE (auto-sleep sigue activo)");
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

    /* Polling siempre. Antes leíamos solo cuando el ISR daba semáforo,
     * pero light-sleep + GPIO isolation puede tragarse el edge (GPIO39 no
     * es RTC en S3, así que no despierta de light sleep confiablemente).
     * A 60 Hz de LVGL son 6 bytes I2C cada 16 ms → ~1% del bus. Despreciable.
     * El semáforo del ISR queda como hint drenado, sin afectar el flujo. */
    (void)xSemaphoreTake(s_irq_sem, 0);

    uint8_t buf[6];
    if (i2c_read_bytes(SUPA_I2C_ADDR_TOUCH, 0x01, buf, 6) == ESP_OK) {
        uint8_t gesture = buf[0];
        uint8_t fingers = buf[1];
        uint16_t x = ((uint16_t)(buf[2] & 0x0F) << 8) | buf[3];
        uint16_t y = ((uint16_t)(buf[4] & 0x0F) << 8) | buf[5];
        bool was_pressed = s_last.pressed;
        s_last.gesture = gesture;
        s_last.pressed = (fingers > 0);
        if (s_last.pressed) {
            if (x < 240) s_last.x = x;
            if (y < 240) s_last.y = y;
        }
        /* Solo loguea en el flanco press↑ o release↓ para no floodear a 60 Hz.
         * También cuando hay gesture no-cero (chip decodificó tap/swipe). */
        if (s_last.pressed && !was_pressed) {
            ESP_LOGI(TAG, "press (%u,%u) gesture=0x%02X", s_last.x, s_last.y, gesture);
        } else if (!s_last.pressed && was_pressed) {
            ESP_LOGI(TAG, "release last=(%u,%u) gesture=0x%02X", s_last.x, s_last.y, gesture);
        } else if (gesture != 0 && s_last.pressed) {
            ESP_LOGI(TAG, "gesture 0x%02X @ (%u,%u)", gesture, s_last.x, s_last.y);
        }
    }

    *out = s_last;
    return true;
}

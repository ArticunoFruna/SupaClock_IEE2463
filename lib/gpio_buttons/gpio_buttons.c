#include "gpio_buttons.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "GPIO_BUTTONS";

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    // Example: send GPIO event to a queue via xQueueSendFromISR
}

esp_err_t gpio_buttons_init(void) {
    gpio_config_t btn_cfg = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL<<BTN_SEL_PIN) | (1ULL<<BTN_UP_PIN) | (1ULL<<BTN_DOWN_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE // Assuming active low logic
    };
    esp_err_t err = gpio_config(&btn_cfg);
    if(err != ESP_OK) return err;

    gpio_config_t int_cfg = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL<<BMI160_INT1_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE 
    };
    err = gpio_config(&int_cfg);
    if(err != ESP_OK) return err;

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_SEL_PIN, gpio_isr_handler, (void*) BTN_SEL_PIN);
    gpio_isr_handler_add(BTN_UP_PIN, gpio_isr_handler, (void*) BTN_UP_PIN);
    gpio_isr_handler_add(BTN_DOWN_PIN, gpio_isr_handler, (void*) BTN_DOWN_PIN);
    gpio_isr_handler_add(BMI160_INT1_PIN, gpio_isr_handler, (void*) BMI160_INT1_PIN);

    ESP_LOGI(TAG, "Hardware buttons initialized");
    return ESP_OK;
}

#include "max17048.h"
#include "i2c_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "MAX17048";

esp_err_t max17048_init(void) {
    esp_err_t err;

    // 1. Power On Reset (Despierta el chip y resetea algoritmos)
    // CMD register (0xFE) = 0x5400
    uint8_t cmd_data[2] = {0x54, 0x00};
    err = i2c_write_bytes(MAX17048_I2C_ADDR, MAX17048_REG_CMD, cmd_data, 2);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "POR write failed: %s", esp_err_to_name(err));
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Esperar que reinicie

    // 2. Quick Start (Fuerza lectura de ADC inmediata)
    // MODE register (0x06) = 0x4000
    uint8_t mode_data[2] = {0x40, 0x00};
    err = i2c_write_bytes(MAX17048_I2C_ADDR, MAX17048_REG_MODE, mode_data, 2);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Quick Start write failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "MAX17048 inicializado");
    return err;
}

esp_err_t max17048_get_voltage(uint16_t *voltage) {
    uint8_t data[2];
    esp_err_t err = i2c_read_bytes(MAX17048_I2C_ADDR, MAX17048_REG_VCELL, data, 2);
    if (err == ESP_OK) {
        uint16_t raw = (data[0] << 8) | data[1];
        // 1 raw unit = 78.125 uV -> raw * 5/64 mV
        *voltage = (uint16_t)(((uint32_t)raw * 5) / 64);
    }
    return err;
}

esp_err_t max17048_get_soc(float *soc) {
    uint8_t data[2];
    esp_err_t err = i2c_read_bytes(MAX17048_I2C_ADDR, MAX17048_REG_SOC, data, 2);
    if (err == ESP_OK) {
        *soc = data[0] + (data[1] / 256.0f);
    }
    return err;
}

#include "max17048.h"
#include "i2c_bus.h"
#include "driver/i2c.h"

esp_err_t max17048_init(void) {
    // Basic verification and reset commands could go here
    return ESP_OK;
}

esp_err_t max17048_get_voltage(uint16_t *voltage) {
    uint8_t reg = MAX17048_REG_VCELL;
    uint8_t data[2];
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, MAX17048_I2C_ADDR, &reg, 1, data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err == ESP_OK) {
        *voltage = (data[0] << 8) | data[1];
        // Conversion formulas based on datasheet
        *voltage = (*voltage) * 78125 / 1000000; // Example 78.125uV / cell
    }
    return err;
}

esp_err_t max17048_get_soc(float *soc) {
    uint8_t reg = MAX17048_REG_SOC;
    uint8_t data[2];
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, MAX17048_I2C_ADDR, &reg, 1, data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err == ESP_OK) {
        *soc = data[0] + (data[1] / 256.0f);
    }
    return err;
}

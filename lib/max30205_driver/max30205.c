#include "max30205.h"
#include "i2c_bus.h"
#include "driver/i2c.h"

esp_err_t max30205_init(void) {
    uint8_t data[2];
    esp_err_t err = ESP_OK;

    // Init configuration register if necessary
    data[0] = MAX30205_REG_CONFIGURATION;
    data[1] = 0x00;
    err |= i2c_master_write_to_device(I2C_MASTER_NUM, MAX30205_I2C_ADDR, data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    return err;
}

esp_err_t max30205_read_temperature(float *temp_c) {
    uint8_t reg = MAX30205_REG_TEMPERATURE;
    uint8_t data[2];
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, MAX30205_I2C_ADDR, &reg, 1, data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err == ESP_OK) {
        int16_t raw_temp = (data[0] << 8) | data[1];
        *temp_c = raw_temp * 0.00390625f; // Resolution 1/256
    }
    return err;
}

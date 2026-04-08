#include "bmi160.h"
#include "i2c_bus.h"
#include "driver/i2c.h"

esp_err_t bmi160_init(void) {
    uint8_t data[2];
    esp_err_t err = ESP_OK;

    // Soft reset
    data[0] = BMI160_REG_CMD;
    data[1] = 0xB6;
    err |= i2c_master_write_to_device(I2C_MASTER_NUM, BMI160_I2C_ADDR, data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    
    // Config Accel (Normal mode) and other initializations...

    return err;
}

esp_err_t bmi160_read_accel(int16_t *ax, int16_t *ay, int16_t *az) {
    uint8_t reg = BMI160_REG_DATA_ACCEL;
    uint8_t data[6];
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, BMI160_I2C_ADDR, &reg, 1, data, 6, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err == ESP_OK) {
        *ax = (int16_t)((data[1] << 8) | data[0]); 
        *ay = (int16_t)((data[3] << 8) | data[2]);
        *az = (int16_t)((data[5] << 8) | data[4]);
    }
    return err;
}

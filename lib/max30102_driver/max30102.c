#include "max30102.h"
#include "i2c_bus.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t max30102_init(void) {
    uint8_t data[2];
    esp_err_t err = ESP_OK;
    
    // Soft Reset
    data[0] = MAX30102_REG_MODE_CONFIG;
    data[1] = 0x40;
    err |= i2c_master_write_to_device(I2C_MASTER_NUM, MAX30102_I2C_ADDR, data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    // Initial config: SpO2 mode, LED intensity, etc.
    return err;
}

esp_err_t max30102_read_fifo(uint32_t *red, uint32_t *ir) {
    uint8_t reg = MAX30102_REG_FIFO_DATA;
    uint8_t data[6];
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, MAX30102_I2C_ADDR, &reg, 1, data, 6, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err == ESP_OK) {
        *red = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
        *red &= 0x03FFFF; // 18-bit data
        *ir = ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | data[5];
        *ir &= 0x03FFFF; // 18-bit data
    }
    return err;
}

esp_err_t max30102_read_temperature(float *temp) {
    uint8_t data[2];
    esp_err_t err;

    // Trigger internal Temp reading
    data[0] = MAX30102_REG_DIE_TEMP_CONFIG;
    data[1] = 0x01; // TEMP_EN bit
    err = i2c_master_write_to_device(I2C_MASTER_NUM, MAX30102_I2C_ADDR, data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err != ESP_OK) return err;

    // Wait for the conversion (takes ~29ms)
    vTaskDelay(pdMS_TO_TICKS(35));

    // Read the temp integer and fractional registers
    uint8_t reg = MAX30102_REG_DIE_TEMP_INT;
    err = i2c_master_write_read_device(I2C_MASTER_NUM, MAX30102_I2C_ADDR, &reg, 1, data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err != ESP_OK) return err;

    int8_t temp_int = (int8_t)data[0];
    uint8_t temp_frac = data[1];

    *temp = (float)temp_int + ((float)temp_frac * 0.0625f);
    return ESP_OK;
}

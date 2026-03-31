#ifndef MAX30205_H
#define MAX30205_H

#include <stdint.h>
#include "esp_err.h"

// I2C Address (dependent on A0, A1, A2 pins. Typically 0x48 for A0=A1=A2=GND)
#define MAX30205_I2C_ADDR 0x48

// MAX30205 Registers
#define MAX30205_REG_TEMPERATURE    0x00
#define MAX30205_REG_CONFIGURATION  0x01
#define MAX30205_REG_THYST          0x02
#define MAX30205_REG_TOS            0x03

// Constants
#define MAX30205_TEMP_RESOLUTION    0.00390625f
/**
 * @brief Initialize MAX30205 sensor
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max30205_init(void);

/**
 * @brief Read temperature in Celsius
 * @param temp_c Pointer to store temperature 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max30205_read_temperature(float *temp_c);

#endif // MAX30205_H

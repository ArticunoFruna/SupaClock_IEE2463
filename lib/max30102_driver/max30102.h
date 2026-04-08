#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include "esp_err.h"

#define MAX30102_I2C_ADDR 0x57

// Status Registers
#define MAX30102_REG_INT_STAT1          0x00
#define MAX30102_REG_INT_STAT2          0x01
#define MAX30102_REG_INT_EN1            0x02
#define MAX30102_REG_INT_EN2            0x03

// FIFO Registers
#define MAX30102_REG_FIFO_WR_PTR        0x04
#define MAX30102_REG_OVF_COUNTER        0x05
#define MAX30102_REG_FIFO_RD_PTR        0x06
#define MAX30102_REG_FIFO_DATA          0x07

// Configuration Registers
#define MAX30102_REG_FIFO_CONFIG        0x08
#define MAX30102_REG_MODE_CONFIG        0x09
#define MAX30102_REG_SPO2_CONFIG        0x0A

// LED Config Registers
#define MAX30102_REG_LED1_PA            0x0C
#define MAX30102_REG_LED2_PA            0x0D // IR LED
#define MAX30102_REG_MULTI_LED_CTRL1    0x11
#define MAX30102_REG_MULTI_LED_CTRL2    0x12

// Temperature Registers
#define MAX30102_REG_DIE_TEMP_INT       0x1F
#define MAX30102_REG_DIE_TEMP_FRAC      0x20
#define MAX30102_REG_DIE_TEMP_CONFIG    0x21

// Part ID Registers
#define MAX30102_REG_REV_ID             0xFE
#define MAX30102_REG_PART_ID            0xFF

/**
 * @brief Initialize MAX30102 sensor
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max30102_init(void);

/**
 * @brief Read new samples from FIFO
 * @param red Pointer to store Red LED count
 * @param ir Pointer to store IR LED count
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max30102_read_fifo(uint32_t *red, uint32_t *ir);

/**
 * @brief Read Die Temperature
 * @param temp Pointer to store Temperature in Celsius
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max30102_read_temperature(float *temp);

#endif // MAX30102_H

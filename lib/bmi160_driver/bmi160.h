#ifndef BMI160_H
#define BMI160_H

#include <stdint.h>
#include "esp_err.h"

// BMI160 typical I2C address
#define BMI160_I2C_ADDR 0x68 

// BMI160 Basic Registers
#define BMI160_REG_CHIPID       0x00
#define BMI160_REG_ERR_REG      0x02
#define BMI160_REG_PMU_STATUS   0x03
#define BMI160_REG_DATA_MAG     0x04
#define BMI160_REG_DATA_GYRO    0x0C
#define BMI160_REG_DATA_ACCEL   0x12
#define BMI160_REG_SENSORTIME   0x18
#define BMI160_REG_STATUS       0x1B
#define BMI160_REG_INT_STATUS   0x1C
#define BMI160_REG_FIFO_LENGTH  0x22
#define BMI160_REG_FIFO_DATA    0x24
#define BMI160_REG_ACCEL_CONF   0x40
#define BMI160_REG_ACCEL_RANGE  0x41
#define BMI160_REG_GYRO_CONF    0x42
#define BMI160_REG_GYRO_RANGE   0x43
#define BMI160_REG_INT_EN       0x50
#define BMI160_REG_INT_OUT_CTRL 0x53
#define BMI160_REG_CMD          0x7E

/**
 * @brief Initialize BMI160 sensor
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bmi160_init(void);

/**
 * @brief Read current accelerometer data
 * @param ax Pointer to X axis mg
 * @param ay Pointer to Y axis mg
 * @param az Pointer to Z axis mg
 * @return esp_err_t ESP_OK on success
 */
esp_err_t bmi160_read_accel(int16_t *ax, int16_t *ay, int16_t *az);

#endif // BMI160_H

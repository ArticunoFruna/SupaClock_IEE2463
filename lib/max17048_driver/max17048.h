#ifndef MAX17048_H
#define MAX17048_H

#include <stdint.h>
#include "esp_err.h"

#define MAX17048_I2C_ADDR 0x36

// Registers
#define MAX17048_REG_VCELL   0x02
#define MAX17048_REG_SOC     0x04
#define MAX17048_REG_MODE    0x06
#define MAX17048_REG_VERSION 0x08
#define MAX17048_REG_HIBRT   0x0A
#define MAX17048_REG_CONFIG  0x0C
#define MAX17048_REG_VALRT   0x14
#define MAX17048_REG_CRATE   0x16
#define MAX17048_REG_VRESET  0x18
#define MAX17048_REG_STATUS  0x1A
#define MAX17048_REG_CMD     0xFE

// Commands
#define MAX17048_CMD_POR     0x5400
#define MAX17048_CMD_QSTRT   0x4000

/**
 * @brief Initialize MAX17048 Fuel Gauge
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max17048_init(void);

/**
 * @brief Read battery voltage (VCELL)
 * @param voltage Pointer to store voltage in mV
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max17048_get_voltage(uint16_t *voltage);

/**
 * @brief Read State of Charge (SOC)
 * @param soc Pointer to store SOC in %
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max17048_get_soc(float *soc);

#endif // MAX17048_H

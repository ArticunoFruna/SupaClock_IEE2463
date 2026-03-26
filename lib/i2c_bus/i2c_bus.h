#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdint.h>
#include "esp_err.h"

#define I2C_MASTER_SCL_IO           9      /*!< GPIO number used for I2C master clock (ESP32-C3) */
#define I2C_MASTER_SDA_IO           8      /*!< GPIO number used for I2C master data  (ESP32-C3) */
#define I2C_MASTER_NUM              I2C_NUM_0 /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000 /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0      /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0      /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

/**
 * @brief Init I2C Master
 * @return esp_err_t 
 */
esp_err_t i2c_master_init(void);

#endif // I2C_BUS_H

#ifndef I2C_BUS_H
#define I2C_BUS_H

#include "esp_err.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define I2C_MASTER_SCL_IO                                                      \
  9 /*!< GPIO number used for I2C master clock (ESP32-C3) */
#define I2C_MASTER_SDA_IO                                                      \
  8 /*!< GPIO number used for I2C master data  (ESP32-C3) */
#define I2C_MASTER_NUM                                                         \
  I2C_NUM_0 /*!< I2C master i2c port number, the number of i2c peripheral      \
               interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ 400000   /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS 1000

/**
 * @brief Mutex global del bus I2C.
 * Creado en i2c_master_init(). Las funciones i2c_read_bytes/i2c_write_bytes
 * lo toman internamente. Exportado para drivers que necesiten acceso directo.
 */
extern SemaphoreHandle_t i2c_bus_mutex;

/**
 * @brief Init I2C Master
 * @return esp_err_t
 */
esp_err_t i2c_master_init(void);

/**
 * @brief Write bytes to I2C device
 * @param dev_addr the I2C address of the device
 * @param reg_addr the internal register address to write to
 * @param data pointer to the data array to write
 * @param len length of the data to write
 * @return esp_err_t ESP_OK on success
 */
esp_err_t i2c_write_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data,
                          size_t len);

/**
 * @brief Read bytes from I2C device
 * @param dev_addr the I2C address of the device
 * @param reg_addr the internal register address to read from
 * @param data pointer to array to hold read data
 * @param len number of bytes to read
 * @return esp_err_t ESP_OK on success
 */
esp_err_t i2c_read_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data,
                         size_t len);

#endif // I2C_BUS_H

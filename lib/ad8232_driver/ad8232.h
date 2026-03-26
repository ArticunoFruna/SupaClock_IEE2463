#ifndef AD8232_H
#define AD8232_H

#include <stdint.h>
#include "esp_err.h"

#define AD8232_ADC_CHANNEL 0 // ADC1_CH0 is GPIO0 on ESP32-C3

/**
 * @brief Initialize ADC for AD8232 continuous or single reads
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ad8232_init(void);

/**
 * @brief Read single raw ADC value from AD8232
 * @param value Pointer to store result in millivolts or raw format
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ad8232_read_single(int *value);

#endif // AD8232_H

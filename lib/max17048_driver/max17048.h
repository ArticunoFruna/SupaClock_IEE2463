#ifndef MAX17048_H
#define MAX17048_H

#include <stdint.h>
#include <stdbool.h>
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

// STATUS register bits
#define MAX17048_STATUS_POR_BIT 0x0100   // bit 8 del registro 0x1A (high byte bit 0)

/**
 * @brief Initialize MAX17048 Fuel Gauge.
 *        Detecta POR y, si la batería está en reposo, ejecuta Quick Start.
 *        Configura hibernate para mejorar la estabilidad bajo cargas pequeñas.
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max17048_init(void);

/**
 * @brief Force Quick Start — recalculate SOC from current voltage reading.
 * Only call when the battery is at rest (no load) or after inserting a new cell.
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max17048_quick_start(void);

/**
 * @brief Software Power-On Reset. Reinicia el chip por completo, borra el
 *        ModelGauge y deja al MAX17048 en estado de fábrica. Tras llamarlo
 *        se ejecuta Quick Start automáticamente.
 *        Úsese sólo cuando se cambia de batería o el SOC quedó inconsistente.
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max17048_reset(void);

/**
 * @brief Lee bit POR del registro STATUS.
 * @param por_detected Salida: true si POR=1 (chip arrancó desde frío)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max17048_check_por(bool *por_detected);

/**
 * @brief Limpia el bit POR del registro STATUS (escribe 0).
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max17048_clear_por(void);

/**
 * @brief Configura el registro HIBRT (0x0A) — umbrales de hibernate.
 *        En hibernate el ADC mide cada 45 s en lugar de 250 ms, lo que
 *        reduce el ruido del SOC durante consumos bajos/estables.
 * @param hib_thr  Umbral CRATE (%/hr * 0.208) para entrar en hibernate. Default 0x80.
 * @param act_thr  Umbral VCELL delta (1.25 mV) para salir de hibernate. Default 0x30.
 *        Pasar HIBRT=0xFFFF fuerza hibernate permanente; HIBRT=0x0000 lo deshabilita.
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max17048_set_hibernate(uint8_t hib_thr, uint8_t act_thr);

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

/**
 * @brief Read charge/discharge rate
 * @param crate Pointer to store rate in %/hour (positive = charging, negative = discharging)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max17048_get_crate(float *crate);

/**
 * @brief Set RCOMP value for temperature/battery compensation
 * @param rcomp_value 8-bit RCOMP value (default 0x97 for 20°C)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max17048_set_rcomp(uint8_t rcomp_value);
/**
 * @brief Configure VRESET threshold (1 LSB = 40mV).
 *        Setting this below the operational battery level avoids false PORs 
 *        due to sudden voltage drops during high current draws.
 * @param vreset_val Threshold for VRESET (e.g. 0x4B for 3.0V)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max17048_set_vreset(uint8_t vreset_val);

#endif // MAX17048_H

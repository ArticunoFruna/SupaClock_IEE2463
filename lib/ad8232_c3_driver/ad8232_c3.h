#ifndef AD8232_C3_H
#define AD8232_C3_H

#include "esp_adc/adc_continuous.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// ESP32-C3 SuperMini: ECG_OUT -> GPIO 0 = ADC1_CH0
#define AD8232_C3_ADC_CHANNEL  ADC_CHANNEL_0
#define AD8232_C3_ADC_UNIT     ADC_UNIT_1

// Pin para Shutdown (SDN). GPIO 21. Activo en BAJO (Low = Apagado, High = Encendido).
#define AD8232_C3_SDN_PIN      21

// Pines de detección de "Leads Off" (LO+ y LO-). No cableados en C3 SuperMini (-1 = deshabilitados).
#define AD8232_C3_LO_PLUS_PIN  -1
#define AD8232_C3_LO_MINUS_PIN -1

// Frecuencia de muestreo del hardware DMA (mínimo de adc_continuous en C3 es 20 kHz)
#define AD8232_C3_HW_SAMPLE_FREQ_HZ 20000

// Frecuencia de salida objetivo del ECG (500 Hz)
#define AD8232_C3_TARGET_FREQ_HZ 500

// Tamaño del frame de DMA (en bytes) que recibiremos en cada interrupción
#define AD8232_C3_READ_LEN 1024

/**
 * @brief Inicializa el módulo AD8232 para ESP32-C3. Configura los GPIOs (SDN)
 *        y prepara el ADC en modo DMA continuo.
 * @return esp_err_t ESP_OK en caso de éxito.
 */
esp_err_t ad8232_c3_init_dma(void);

/**
 * @brief Arranca el flujo de DMA del ADC.
 * @return esp_err_t ESP_OK en caso de éxito.
 */
esp_err_t ad8232_c3_start_dma(void);

/**
 * @brief Detiene el flujo de DMA del ADC.
 * @return esp_err_t ESP_OK en caso de éxito.
 */
esp_err_t ad8232_c3_stop_dma(void);

/**
 * @brief Pone el módulo AD8232 en modo de bajo consumo (Shutdown).
 */
void ad8232_c3_power_down(void);

/**
 * @brief Saca el módulo AD8232 del modo Shutdown.
 */
void ad8232_c3_power_up(void);

/**
 * @brief Comprueba si los electrodos están desconectados.
 * @return true si al menos un electrodo está desconectado, false si están bien.
 */
bool ad8232_c3_is_leads_off(void);

/**
 * @brief Obtiene el handle continuo del ADC. Útil para asociar callbacks
 *        desde la aplicación principal.
 * @return adc_continuous_handle_t El handle del ADC continuo.
 */
adc_continuous_handle_t ad8232_c3_get_adc_handle(void);

#endif // AD8232_C3_H

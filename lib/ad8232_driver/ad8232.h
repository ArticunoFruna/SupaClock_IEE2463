#ifndef AD8232_H
#define AD8232_H

#include "esp_adc/adc_continuous.h"
#include "esp_err.h"
#include <stdint.h>
#include "supaclock_pinmap.h"

// XIAO ESP32-S3: ECG_OUT → GPIO1 = ADC1_CH0 (ver supaclock_pinmap.h)
#define AD8232_ADC_CHANNEL SUPA_ADC_CHANNEL_ECG
#define AD8232_ADC_UNIT    SUPA_ADC_UNIT_ECG

// Pin para Shutdown (SDN). Activo en BAJO (Low = Apagado, High = Encendido).
#define AD8232_SDN_PIN      SUPA_PIN_ECG_SDN

// Pines de detección de "Leads Off" (LO+ y LO-). No cableados en carrier v1.
#define AD8232_LO_PLUS_PIN  SUPA_PIN_ECG_LO_PLUS
#define AD8232_LO_MINUS_PIN SUPA_PIN_ECG_LO_MINUS

// Frecuencia de muestreo del hardware DMA. En ESP32-S3 el mínimo del
// adc_continuous es 611 Hz; subimos a 20 kHz para decimar limpio.
#define AD8232_HW_SAMPLE_FREQ_HZ 20000

// Frecuencia de salida objetivo del ECG (ej. 500 Hz)
#define AD8232_TARGET_FREQ_HZ 500

// Tamaño del frame de DMA (en bytes) que recibiremos en cada interrupción
#define AD8232_READ_LEN 1024

/**
 * @brief Inicializa el módulo AD8232. Configura los GPIOs (SDN, LO+, LO-)
 *        y prepara el ADC en modo DMA continuo.
 * @return esp_err_t ESP_OK en caso de éxito.
 */
esp_err_t ad8232_init_dma(void);

/**
 * @brief Arranca el flujo de DMA del ADC.
 * @return esp_err_t ESP_OK en caso de éxito.
 */
esp_err_t ad8232_start_dma(void);

/**
 * @brief Detiene el flujo de DMA del ADC.
 * @return esp_err_t ESP_OK en caso de éxito.
 */
esp_err_t ad8232_stop_dma(void);

/**
 * @brief Pone el módulo AD8232 en modo de bajo consumo (Shutdown).
 *        Solo funciona si AD8232_SDN_PIN está definido y conectado.
 */
void ad8232_power_down(void);

/**
 * @brief Saca el módulo AD8232 del modo Shutdown.
 *        Solo funciona si AD8232_SDN_PIN está definido y conectado.
 */
void ad8232_power_up(void);

/**
 * @brief Comprueba si los electrodos están desconectados.
 *        Solo funciona si AD8232_LO_PLUS_PIN y AD8232_LO_MINUS_PIN están
 * definidos.
 * @return true si al menos un electrodo está desconectado, false si están bien.
 */
bool ad8232_is_leads_off(void);

/**
 * @brief Obtiene el handle continuo del ADC. Útil para asociar callbacks
 *        desde la aplicación principal.
 * @return adc_continuous_handle_t El handle del ADC continuo.
 */
adc_continuous_handle_t ad8232_get_adc_handle(void);

#endif // AD8232_H

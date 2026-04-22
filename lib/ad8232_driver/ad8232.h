#ifndef AD8232_H
#define AD8232_H

#include "esp_adc/adc_continuous.h"
#include "esp_err.h"
#include <stdint.h>

// ESP32-C3 ADC1 Channel 0 corresponds to GPIO 0
#define AD8232_ADC_CHANNEL ADC_CHANNEL_0
#define AD8232_ADC_UNIT ADC_UNIT_1

// Pin para Shutdown (SDN). Activo en BAJO (Low = Apagado, High = Encendido).
// Ponlo a -1 si el pin SDN está conectado físicamente a 3.3V o no se usa.
#define AD8232_SDN_PIN 2

// Pines de detección de "Leads Off" (LO+ y LO-). Activos en ALTO.
// Pon a -1 si no se conectan a un GPIO del ESP32.
#define AD8232_LO_PLUS_PIN -1
#define AD8232_LO_MINUS_PIN -1

// Frecuencia de muestreo del hardware DMA (Mínimo 20 kHz en ESP32-C3)
#define AD8232_HW_SAMPLE_FREQ_HZ 20000

// Frecuencia de salida objetivo del ECG (ej. 500 Hz)
#define AD8232_TARGET_FREQ_HZ 500

// Tamaño del frame de DMA (en bytes) que recibiremos en cada interrupción
#define AD8232_READ_LEN 256

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

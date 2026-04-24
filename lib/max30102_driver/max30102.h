/**
 * @file max30102.h
 * @brief Driver I2C para el sensor óptico MAX30102 (PPG → HR + SpO2)
 *
 * Driver liviano para ESP32-C3 con ESP-IDF.
 * Usa el bus I2C compartido de i2c_bus.h (mutex interno).
 *
 * Registros y secuencias según:
 *   Maxim Integrated — MAX30102 High-Sensitivity Pulse Oximeter and
 *   Heart-Rate Sensor for Wearable Health, Rev 1 (2018).
 *
 * Flujo de uso típico:
 *   max30102_init_hrm();
 *   loop:
 *     max30102_read_samples(samples, &n);
 *     for (i < n) max30102_process_sample(samples[i].red, samples[i].ir);
 *     max30102_get_hr(&bpm);
 *     max30102_get_spo2(&spo2);
 */
#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define MAX30102_I2C_ADDR               0x57

/* ─────────────── Status / INT ─────────────── */
#define MAX30102_REG_INT_STAT1          0x00
#define MAX30102_REG_INT_STAT2          0x01
#define MAX30102_REG_INT_EN1            0x02
#define MAX30102_REG_INT_EN2            0x03

/* ─────────────── FIFO ─────────────── */
#define MAX30102_REG_FIFO_WR_PTR        0x04
#define MAX30102_REG_OVF_COUNTER        0x05
#define MAX30102_REG_FIFO_RD_PTR        0x06
#define MAX30102_REG_FIFO_DATA          0x07

/* ─────────────── Configuración ─────────────── */
#define MAX30102_REG_FIFO_CONFIG        0x08
#define MAX30102_REG_MODE_CONFIG        0x09
#define MAX30102_REG_SPO2_CONFIG        0x0A
#define MAX30102_REG_LED1_PA            0x0C  /* Red   */
#define MAX30102_REG_LED2_PA            0x0D  /* IR    */
#define MAX30102_REG_MULTI_LED_CTRL1    0x11
#define MAX30102_REG_MULTI_LED_CTRL2    0x12

/* ─────────────── Temperatura ─────────────── */
#define MAX30102_REG_DIE_TEMP_INT       0x1F
#define MAX30102_REG_DIE_TEMP_FRAC      0x20
#define MAX30102_REG_DIE_TEMP_CONFIG    0x21

/* ─────────────── Part ID ─────────────── */
#define MAX30102_REG_REV_ID             0xFE
#define MAX30102_REG_PART_ID            0xFF
#define MAX30102_PART_ID_VALUE          0x15

/* ─────────────── Modos ─────────────── */
#define MAX30102_MODE_HR_ONLY           0x02  /* Solo IR      */
#define MAX30102_MODE_SPO2              0x03  /* Red + IR     */
#define MAX30102_MODE_RESET             0x40  /* bit RESET    */
#define MAX30102_MODE_SHDN              0x80  /* bit SHDN     */

/* ─────────────── Tipos ─────────────── */
typedef struct {
    uint32_t red;  /**< Canal Red,  18-bit */
    uint32_t ir;   /**< Canal IR,   18-bit */
} max30102_sample_t;

/* ──────────────────────── API Pública ──────────────────────── */

/**
 * @brief Inicialización legacy (configuración mínima). Se mantiene por
 *        compatibilidad con código previo; prefiera max30102_init_hrm().
 */
esp_err_t max30102_init(void);

/**
 * @brief Inicializar MAX30102 en modo SpO2 (Red + IR) para HR/SpO2.
 *
 * Config aplicada:
 *   - Soft reset + limpieza FIFO
 *   - Sample average = 4, rollover on, A_FULL = 17 muestras
 *   - ADC range 4096 nA, 100 sps, pulse width 411 us (18-bit)
 *   - LED Red/IR ≈ 7.2 mA
 *
 * @return ESP_OK si todo OK; ESP_ERR_NOT_FOUND si PART_ID no coincide.
 */
esp_err_t max30102_init_hrm(void);

/**
 * @brief Leer muestras crudas pendientes en la FIFO (Red + IR).
 * @param out    Buffer destino (tamaño ≥ max_samples).
 * @param max_samples Capacidad del buffer.
 * @param n_read Número de muestras realmente leídas.
 * @return ESP_OK en éxito.
 */
esp_err_t max30102_read_samples(max30102_sample_t *out,
                                uint8_t max_samples,
                                uint8_t *n_read);

/**
 * @brief Wrapper legacy: lee 1 muestra (3B Red + 3B IR).
 */
esp_err_t max30102_read_fifo(uint32_t *red, uint32_t *ir);

/**
 * @brief Procesar una muestra Red/IR: alimenta el detector de picos
 *        (para HR) y los acumuladores AC/DC (para SpO2).
 *
 * Debe llamarse secuencialmente para cada muestra a la frecuencia
 * efectiva del sensor (~25 Hz con sample_avg=4 @ 100 sps).
 */
void max30102_process_sample(uint32_t red, uint32_t ir);

/**
 * @brief Obtener estimación actual de frecuencia cardiaca (BPM).
 * @param bpm salida; 0 si aún no hay estimación válida / sin dedo.
 * @return ESP_OK si la estimación es válida, ESP_ERR_INVALID_STATE si no.
 */
esp_err_t max30102_get_hr(uint8_t *bpm);

/**
 * @brief Obtener estimación actual de SpO2 (%).
 * @param spo2 salida; 0 si aún no hay estimación válida.
 * @return ESP_OK si la estimación es válida, ESP_ERR_INVALID_STATE si no.
 */
esp_err_t max30102_get_spo2(uint8_t *spo2);

/**
 * @brief ¿Hay dedo / piel en contacto con el sensor?
 *        Usa el nivel DC del canal IR.
 */
bool max30102_finger_present(void);

/**
 * @brief Descartar muestras acumuladas en el FIFO sin leerlas.
 *
 * Útil cuando hay un gap grande entre init y la primera lectura (p.ej.
 * el firmware inicializa el sensor en Fase 1 y las tasks arrancan después
 * de inicializar display + BLE). Resetea los punteros WR/RD/OVF y la
 * cadena interna de picos para empezar limpio.
 */
esp_err_t max30102_flush_fifo(void);

/**
 * @brief Contador acumulado de muestras perdidas por overflow del FIFO.
 *
 * El FIFO físico guarda 32 muestras; si la task de lectura no llega a tiempo
 * el hardware descarta las muestras más antiguas. Cuando esto pasa, la
 * cadena de RR intervals deja de tener sentido (se perdió tiempo real
 * entre muestras), por lo que el driver invalida automáticamente la
 * cadena de picos.
 *
 * Este contador es útil para detectar sub-dimensionado del período de
 * polling desde la aplicación.
 */
uint32_t max30102_get_overflow_count(void);

/**
 * @brief Leer temperatura interna del die (°C). Útil para debug térmico.
 */
esp_err_t max30102_read_temperature(float *temp);

#endif // MAX30102_H

/**
 * @file max30102.h
 * @brief Driver MAX30102 (PPG → HR + SpO2) con SHDN/wake, bandpass IIR,
 *        mediana RR, motion gating y máquina de estados SPOT.
 *
 * Arquitectura:
 *   - Modo CONTINUO: muestreo a 25 Hz, bandpass 0.5-4 Hz, mediana de
 *     los últimos 8 RR para HR, mediana móvil de R para SpO2.
 *   - Modo SPOT: ventana corta dedicada (~12-30 s con early-exit por
 *     calidad), publica un único valor final con métrica de calidad.
 *   - Motion gating: la app inyecta el nivel de jerk del IMU; cuando
 *     supera el umbral, los nuevos valores se invalidan.
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
#define MAX30102_REG_LED1_PA            0x0C
#define MAX30102_REG_LED2_PA            0x0D
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
#define MAX30102_MODE_HR_ONLY           0x02
#define MAX30102_MODE_SPO2              0x03
#define MAX30102_MODE_RESET             0x40
#define MAX30102_MODE_SHDN              0x80

typedef struct {
    uint32_t red;
    uint32_t ir;
} max30102_sample_t;

/* ════════════════════════════════════════════════════════════════
 *  API base — init, lectura, processing
 * ════════════════════════════════════════════════════════════════ */

esp_err_t max30102_init(void);
esp_err_t max30102_init_hrm(void);

esp_err_t max30102_read_samples(max30102_sample_t *out, uint8_t max_samples,
                                uint8_t *n_read);
esp_err_t max30102_read_fifo(uint32_t *red, uint32_t *ir);

void max30102_process_sample(uint32_t red, uint32_t ir);

esp_err_t max30102_get_hr(uint8_t *bpm);
esp_err_t max30102_get_spo2(uint8_t *spo2);
bool max30102_finger_present(void);

esp_err_t max30102_flush_fifo(void);
uint32_t max30102_get_overflow_count(void);

esp_err_t max30102_read_temperature(float *temp);

/* ════════════════════════════════════════════════════════════════
 *  Power: SHDN entre mediciones (modos NORMAL/SAVER)
 * ════════════════════════════════════════════════════════════════ */

/**
 * @brief Pone el sensor en SHDN (LEDs apagados, ~0.7 µA).
 * El estado del algoritmo se preserva pero la próxima medición debería
 * descartar las primeras muestras (settling).
 */
esp_err_t max30102_shutdown(void);

/**
 * @brief Sale de SHDN, vuelve a modo SpO2 y limpia el FIFO + estado
 * algorítmico. Llamar antes de empezar una medición tras un SHDN.
 */
esp_err_t max30102_wake(void);

/**
 * @brief ¿El sensor está actualmente activo (LEDs on)?
 */
bool max30102_is_awake(void);

/* ════════════════════════════════════════════════════════════════
 *  Motion gating
 * ════════════════════════════════════════════════════════════════ */

/**
 * @brief Inyecta el nivel de movimiento del IMU.
 *
 * jerk_score: 0..255, donde 0 = quieto y 255 = fuerte agitación.
 * El driver usa esto para invalidar muestras durante movimiento brusco.
 *
 * Llamar desde imu_task con cadencia normal del IMU.
 */
void max30102_set_motion_level(uint8_t jerk_score);

/* ════════════════════════════════════════════════════════════════
 *  SPOT — máquina de estados de medición dedicada
 * ════════════════════════════════════════════════════════════════ */

typedef enum {
    SPOT_STATE_IDLE      = 0,  /**< No hay medición spot en curso */
    SPOT_STATE_SETTLING  = 1,  /**< Descartando muestras (5 s)    */
    SPOT_STATE_MEASURING = 2,  /**< Acumulando muestras válidas    */
    SPOT_STATE_DONE      = 3,  /**< Finalizado OK, valor disponible*/
    SPOT_STATE_FAILED    = 4,  /**< Timeout sin señal usable       */
    SPOT_STATE_ABORTED   = 5,  /**< Cancelado por usuario o movimiento */
} max30102_spot_state_t;

typedef enum {
    SPOT_QUALITY_NONE = 0,
    SPOT_QUALITY_POOR = 1,
    SPOT_QUALITY_FAIR = 2,
    SPOT_QUALITY_GOOD = 3,
} max30102_spot_quality_t;

typedef struct {
    max30102_spot_state_t state;
    uint8_t  progress_pct;     /**< 0-100, basado en tiempo y calidad */
    uint8_t  bpm;              /**< Resultado HR (0 si no DONE)        */
    uint8_t  spo2;             /**< Resultado SpO2                     */
    uint16_t duration_ms;      /**< Duración real de la medición       */
    max30102_spot_quality_t quality;
} max30102_spot_status_t;

/**
 * @brief Inicia una medición SPOT (modo dedicado, max 30 s).
 *
 * El driver pasa por:
 *   SETTLING (5 s) → MEASURING → DONE (early-exit por calidad)
 *                              → FAILED (timeout sin señal)
 *                              → ABORTED (usuario o movimiento)
 *
 * Si ya hay una medición en curso, no hace nada (debe abortarse antes).
 */
esp_err_t max30102_spot_start(void);

/**
 * @brief Aborta una medición SPOT en curso.
 */
esp_err_t max30102_spot_abort(void);

/**
 * @brief Lee el estado actual del SPOT.
 */
void max30102_spot_get_status(max30102_spot_status_t *out);

#endif /* MAX30102_H */

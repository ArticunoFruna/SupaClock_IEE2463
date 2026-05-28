/**
 * @file har_cnn1d.h
 * @brief Reconocimiento de actividad humana on-device usando una CNN-1D
 *        cuantizada (INT8) sobre acelerómetro + giroscopio del BMI160.
 *
 * Entrada:
 *   - 6 canales (ax, ay, az, gx, gy, gz) en raw int16 desde bmi160_read_*.
 *   - Muestreo del IMU a 100 Hz, promediando cada 2 muestras consecutivas para
 *     alimentar el modelo a 50 Hz, con una ventana de 200 muestras (4.0 s)
 *     y 50 % de traslape (100 muestras) → 1 inferencia cada 2 segundos.
 *
 * Salida:
 *   - 4 clases de clasificación del modelo:
 *       HAR_STATE_RESTING / HAR_STATE_WALKING / HAR_STATE_RUNNING / HAR_STATE_FALL
 *
 * Memoria:
 *   - Ring buffer de 200×6 int16 = 2.4 kB en SRAM interna.
 *   - Modelo INT8 (.tflite) embebido como `extern const uint8_t har_model_tflite[]`;
 *     pesos+activaciones ≈ 58 kB. Tensor arena ≈ 128 kB. Todo cabe en SRAM,
 *     pero queda alocado en PSRAM (ver `HAR_TENSOR_ARENA_IN_PSRAM`).
 *
 * Thread model:
 *   - Una sola task (`har_task`) pinned a core 1, prio 4.
 *   - BLE / LVGL / I2C tasks viven en core 0 → la inferencia no las bloquea.
 */
#ifndef HAR_CNN1D_H
#define HAR_CNN1D_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ───────────── Parámetros del modelo ───────────── */
#define HAR_SAMPLE_RATE_HZ     100      /* Muestreo físico a 100 Hz */
#define HAR_MODEL_RATE_HZ      50       /* Frecuencia del modelo a 50 Hz (promedio de 2 muestras) */
#define HAR_WINDOW_SIZE        200      /* Ventana de 200 muestras (4.0 s @ 50 Hz) */
#define HAR_HOP_SIZE           100      /* Traslape del 50 % (100 muestras / 2.0 s) */
#define HAR_CHANNELS           6        /* ax,ay,az,gx,gy,gz */
#define HAR_NUM_CLASSES        4        /* resting / walking / running / fall */

/* Si está a 1, la tensor arena del intérprete vive en PSRAM (recomendado
 * si el modelo crece o si compartes SRAM con LVGL DMA buffers). */
#ifndef HAR_TENSOR_ARENA_IN_PSRAM
#define HAR_TENSOR_ARENA_IN_PSRAM   1
#endif

/* ───────────── Clases de salida ───────────── */
typedef enum {
    HAR_STATE_UNKNOWN = -1,
    HAR_STATE_RESTING = 0,
    HAR_STATE_WALKING = 1,
    HAR_STATE_RUNNING = 2,
    HAR_STATE_FALL = 3,
} har_state_t;

typedef struct {
    har_state_t state;
    float       confidence;   /* softmax max, 0..1 */
    float       probs[HAR_NUM_CLASSES];
    bool        fall_event;   /* true durante 1 ventana tras una caída */
    uint64_t    timestamp_us; /* esp_timer_get_time() al cierre de la ventana */
} har_result_t;

/* Callback invocado por la har_task cada vez que sale una inferencia
 * fresca. Se ejecuta desde la task del HAR — no bloquees ahí. */
typedef void (*har_result_cb_t)(const har_result_t *result, void *user);

/* ───────────── API ───────────── */

/**
 * @brief Inicializa el módulo HAR: aloca arena, carga modelo embebido,
 *        crea ring buffer y la task de inferencia (pinned a core 1).
 *
 * Llamar después de `bmi160_init()`. Internamente el HAR llama a
 * `bmi160_read_accel_gyro` desde su propia task — no necesitas otra
 * task de IMU.
 *
 * @param  cb       callback de resultado (opcional, puede ser NULL).
 * @param  cb_user  cookie pasada al callback.
 * @return ESP_OK si todo ok; ESP_ERR_NO_MEM si no hay PSRAM suficiente.
 */
esp_err_t har_cnn1d_init(har_result_cb_t cb, void *cb_user);

/**
 * @brief Suspende/reanuda la task del HAR.
 *
 * Útil cuando el reloj entra en POWER_MODE_SAVER y no quieres pagar
 * el costo de la CNN. Mientras está suspendida, la detección de caída
 * se mantiene activa (es muy barata).
 */
void har_cnn1d_pause(void);
void har_cnn1d_resume(void);

/**
 * @brief Devuelve la última inferencia conocida (lock-free, copia local).
 */
har_result_t har_cnn1d_last(void);

/**
 * @brief Devuelve estadística de uso de tensor arena (debug).
 */
size_t har_cnn1d_arena_used(void);

#ifdef __cplusplus
}
#endif

#endif /* HAR_CNN1D_H */

#ifndef STEP_ALGORITHM_H
#define STEP_ALGORITHM_H

#include <sdkconfig.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // ---- ESTADO PARA ESP32-S3 (Híbrido FFT + dominio del tiempo) ----
  // Ring buffer de la magnitud pasa-banda (sin DC) para la FFT de detección.
  float bp_mags[128];
  uint16_t sample_index;        // posición de escritura en el ring (0..127)

  // Estados de los filtros IIR (cadena pasa-banda 0.5–5 Hz)
  float prev_raw_mag;           // magnitud cruda anterior (para el pasa-altos)
  float hp_prev;                // salida pasa-altos anterior
  float lp_prev;               // salida pasa-bajos anterior (= señal pasa-banda)

  // Envolvente adaptativa (pico/valle con decaimiento) para el umbral
  float peak_env;
  float valley_env;

  // Detector temporal de pasos
  bool  above;                  // histéresis: señal por encima del umbral
  uint32_t last_step_time_ms;   // timestamp del último paso aceptado
  uint8_t  consecutive_steps;   // racha de pasos con intervalo válido
  uint8_t  provisional_steps;   // pasos buffereados antes de validar la racha

  // Gate espectral (FFT) y cadencia estimada
  bool  walk_gate;              // true mientras la FFT confirma caminata
  uint32_t walk_gate_expiry_ms; // hasta cuándo sigue válido el gate
  float cadence_hz;             // cadencia estimada por la FFT (Hz, 0 si none)
  uint32_t max_gyro_val;        // máx |gyro| en la ventana (entrada blanda)

  // Telemetría de debug (última ventana FFT) — quitar tras calibrar
  float dbg_prominence;
  float dbg_ratio;
  float dbg_peak_hz;            // frecuencia del pico dominante (toda la banda 0..25Hz)
  uint16_t dbg_fft_runs;        // contador de ejecuciones de la FFT
#else
  // ---- ESTADO PARA ESP32-C3 / ESP32 (Aritmética Entera Rapida) ----
  uint32_t filtered_mag_sq;
  uint32_t prev_filtered_mag_sq;
  uint32_t max_val;
  uint32_t min_val;
  uint32_t threshold;
  uint32_t last_step_time_ms;
  uint16_t sample_count;
  uint8_t consecutive_steps;
  uint32_t max_gyro_val;
#endif
} step_algo_state_t;

void step_algo_init(step_algo_state_t *state);

/**
 * @brief Procesa una muestra IMU y devuelve los pasos nuevos detectados.
 *
 * El muestreo se asume uniforme a 50 Hz (ver bmi160 ODR + FIFO). En el
 * ESP32-S3 corre el algoritmo híbrido: la FFT detecta caminata y estima
 * cadencia; el conteo real es por peak-detection en el dominio del tiempo.
 *
 * @param current_time_ms  Timestamp uniforme de la muestra (t0 + i*20 ms).
 * @return pasos nuevos a sumar al acumulado (0 si no hubo).
 */
uint8_t step_algo_update(step_algo_state_t *state, int16_t ax, int16_t ay,
                         int16_t az, int16_t gx, int16_t gy, int16_t gz,
                         uint32_t current_time_ms);

#endif // STEP_ALGORITHM_H

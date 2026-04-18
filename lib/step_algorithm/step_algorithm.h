#ifndef STEP_ALGORITHM_H
#define STEP_ALGORITHM_H

#include <sdkconfig.h>
#include <stdint.h>

typedef struct {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // ---- ESTADO PARA ESP32-S3 (Filtro Morfológico / FFT) ----
  // Buffer para 128 muestras (2.56 seg a 50Hz) para análisis frecuencial.
  float accel_window[256]; // 128 muestras * 2 (Complejas Re+Im) para esp-dsp
  uint16_t sample_index;
  uint32_t last_step_time_ms;
  uint8_t steps_in_queue;
  uint32_t max_gyro_val;
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
uint8_t step_algo_update(step_algo_state_t *state, int16_t ax, int16_t ay,
                         int16_t az, int16_t gx, int16_t gy, int16_t gz, uint32_t current_time_ms);

#endif // STEP_ALGORITHM_H

#include "step_algorithm.h"
#include <math.h>
#include <string.h>

// --- PARÁMETROS GLOBALES COMPARTIDOS ---
#define STEP_MIN_TIME_MS 300
#define STEP_MAX_TIME_MS 2000
#define VALID_STEPS_THRESHOLD 4

// ==============================================================================
//                    IMPLEMENTACIÓN PARA ESP32-S3 (FFT / FPU)
// ==============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32S3)

#include "esp_dsp.h"

#define FFT_WINDOW_SIZE 128

void step_algo_init(step_algo_state_t *state) {
  state->sample_index = 0;
  state->window_start_time_ms = 0;
  state->last_step_time_ms = 0;
  state->steps_in_queue = 0;
  state->max_gyro_val = 0;
  state->consecutive_walking_windows = 0;
  state->cached_steps = 0;

  // Inicializar tablas FFT genéricas
  dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
}

uint8_t step_algo_update(step_algo_state_t *state, int16_t ax, int16_t ay,
                         int16_t az, int16_t gx, int16_t gy, int16_t gz,
                         uint32_t current_time_ms, bool is_sport_mode) {
  uint8_t new_steps = 0;

  // En el S3 calculamos la magnitud lineal con math.h
  float mag = sqrtf((float)ax * ax + (float)ay * ay + (float)az * az);

  // Guardar en la posición correspondiente del buffer de magnitud real
  state->accel_mags[state->sample_index] = mag;

  if (state->sample_index == 0) {
    state->window_start_time_ms = current_time_ms;
  }

  float gyro_mag = sqrtf((float)gx * gx + (float)gy * gy + (float)gz * gz);
  if (gyro_mag > state->max_gyro_val) {
    state->max_gyro_val = (uint32_t)gyro_mag;
  }

  state->sample_index++;

  if (state->sample_index >= FFT_WINDOW_SIZE) {
    // Solo procesamos si hay movimiento angular suficiente
    if (state->max_gyro_val > 400) {
      // 1. Quitar DC Bias (Gravedad y offset base)
      float dc_bias = 0.0f;
      for (int i = 0; i < FFT_WINDOW_SIZE; i++) {
        dc_bias += state->accel_mags[i];
      }
      dc_bias /= FFT_WINDOW_SIZE;
      for (int i = 0; i < FFT_WINDOW_SIZE; i++) {
        state->accel_mags[i] -= dc_bias;
      }

      // 2. Aplicar ventana (Hann) para reducir fuga espectral "leakage"
      // Procesa exactamente 128 floats reales de accel_mags
      dsps_wind_hann_f32(state->accel_mags, FFT_WINDOW_SIZE);

      // 3. Poblar buffer complejo en el stack (128 muestras * 2 = 256 floats)
      float accel_window[FFT_WINDOW_SIZE * 2];
      for (int i = 0; i < FFT_WINDOW_SIZE; i++) {
        accel_window[i * 2] = state->accel_mags[i];
        accel_window[i * 2 + 1] = 0.0f;
      }

      // 4. FFT Radix-2 acelerada por PIE de Xtensa (S3) + bit-reversal.
      // Tras esto, accel_window[2k]/[2k+1] son las partes real/imag de X[k].
      // OJO: NO usamos dsps_cplx2reC_fc32 — esa rutina deshace el packing
      // de DOS señales reales en una FFT compleja y devuelve componentes
      // (re,im), no potencias. Para una sola señal calculamos |X[k]|² aquí.
      dsps_fft2r_fc32(accel_window, FFT_WINDOW_SIZE);
      dsps_bit_rev_fc32(accel_window, FFT_WINDOW_SIZE);

      // 5. Duración real de la ventana (depende del Fs efectivo)
      float total_duration_s = (float)(current_time_ms - state->window_start_time_ms) / 1000.0f;
      if (total_duration_s <= 0.0f) {
        total_duration_s = (float)FFT_WINDOW_SIZE / 50.0f; // fallback Fs=50Hz
      }

      // 6. Buscar pico de potencia en la banda de caminata [0.75 Hz, 2.75 Hz]
      int min_bin = (int)ceilf(0.75f * total_duration_s);
      int max_bin = (int)floorf(2.75f * total_duration_s);
      if (min_bin < 1) min_bin = 1;
      if (max_bin >= FFT_WINDOW_SIZE / 2) max_bin = FFT_WINDOW_SIZE / 2 - 1;

      float peak_power = 0.0f;
      int peak_bin = 0;
      for (int k = min_bin; k <= max_bin; k++) {
        float re = accel_window[k * 2];
        float im = accel_window[k * 2 + 1];
        float p = re * re + im * im;
        if (p > peak_power) {
          peak_power = p;
          peak_bin = k;
        }
      }

      // Umbral de potencia |X|² para 128 puntos a 50Hz / ±2g.
      // Caminata real ~1e9-1e10; reposo / falsos < 1e8.
      const float UMBRAL_FFT = 1.0e9f;
      uint8_t detected_steps = 0;

      if (peak_power > UMBRAL_FFT) {
        // Bin k = freq * T = ciclos en la ventana → 1 ciclo por paso.
        detected_steps = (uint8_t)peak_bin;

        // Con solapamiento 50% (sport mode) cada ciclo se ve en dos
        // ventanas seguidas; repartir el conteo para no contar doble.
        if (is_sport_mode) {
          detected_steps = (uint8_t)((detected_steps + 1) / 2);
        }
      }

      // 7. Filtro de histéresis temporal
      if (detected_steps > 0) {
        state->consecutive_walking_windows++;
        if (state->consecutive_walking_windows == 1) {
          // Primera ventana: guardar buffer temporal
          state->cached_steps = detected_steps;
          new_steps = 0;
        } else if (state->consecutive_walking_windows == 2) {
          // Segunda ventana consecutiva: validar caminata y descargar acumulados
          new_steps = state->cached_steps + detected_steps;
          state->cached_steps = 0;
        } else {
          // Caminata ya validada y continua
          new_steps = detected_steps;
        }
      } else {
        // No se detectó caminata, resetear histéresis
        state->consecutive_walking_windows = 0;
        state->cached_steps = 0;
      }
    } else {
      // Movimiento angular insuficiente, resetear histéresis
      state->consecutive_walking_windows = 0;
      state->cached_steps = 0;
    }

    // 8. Reiniciar o desplazar ventana (según solapamiento / modo)
    if (is_sport_mode) {
      memmove(&state->accel_mags[0], &state->accel_mags[64], 64 * sizeof(float));
      state->sample_index = 64;
      // Retroceder el timestamp inicial en 1.28 segundos (64 muestras a 50Hz)
      state->window_start_time_ms = current_time_ms - 1280;
    } else {
      state->sample_index = 0;
    }
    state->max_gyro_val = 0;
  }

  return new_steps;
}

// ==============================================================================
//               IMPLEMENTACIÓN PARA ESP32-C3 / OTROS (Aritmética Entera)
// ==============================================================================
#else

#define WINDOW_SIZE 50

void step_algo_init(step_algo_state_t *state) {
  state->filtered_mag_sq = 0;
  state->prev_filtered_mag_sq = 0;
  state->max_val = 0;
  state->min_val = 0xFFFFFFFF;
  state->threshold = 0;
  state->last_step_time_ms = 0;
  state->sample_count = 0;
  state->consecutive_steps = 0;
  state->max_gyro_val = 0;
}

// Raíz cuadrada entera rápida (sin FPU o math.h)
static uint32_t int_sqrt(uint32_t val) {
  uint32_t res = 0;
  uint32_t bit = 1UL << 30; // El bit más alto posible para uint32
  while (bit > val)
    bit >>= 2;
  while (bit != 0) {
    if (val >= res + bit) {
      val -= res + bit;
      res = (res >> 1) + bit;
    } else {
      res >>= 1;
    }
    bit >>= 2;
  }
  return res;
}

uint8_t step_algo_update(step_algo_state_t *state, int16_t ax, int16_t ay,
                         int16_t az, int16_t gx, int16_t gy, int16_t gz,
                         uint32_t current_time_ms, bool is_sport_mode) {
  (void)is_sport_mode; // Ignorado en el target C3 de bajo consumo
  uint8_t new_steps = 0;

  // Calcular la magnitud lineal real
  uint32_t sum_sq = (uint32_t)((int32_t)ax * ax) +
                    (uint32_t)((int32_t)ay * ay) + (uint32_t)((int32_t)az * az);
  uint32_t mag = int_sqrt(sum_sq);

  if (state->filtered_mag_sq == 0) {
    state->filtered_mag_sq = mag;
    state->prev_filtered_mag_sq = mag;
    state->threshold = mag;
    state->max_val = mag;
    state->min_val = mag;
  }

  // Filtro Pasa Bajos Exponencial (LPF)
  state->prev_filtered_mag_sq = state->filtered_mag_sq;
  state->filtered_mag_sq = (state->filtered_mag_sq * 3 + mag) / 4;

  uint32_t current_val = state->filtered_mag_sq;

  // Registrar máxima rotación (gyro) en este ciclo actual
  uint32_t gyro_sq = (uint32_t)((int32_t)gx * gx) +
                     (uint32_t)((int32_t)gy * gy) +
                     (uint32_t)((int32_t)gz * gz);
  uint32_t gyro_mag = int_sqrt(gyro_sq);

  if (gyro_mag > state->max_gyro_val) {
    state->max_gyro_val = gyro_mag;
  }

  // Algoritmo Min/Max dinámico
  if (state->sample_count == 0) {
    state->max_val = current_val;
    state->min_val = current_val;
  } else {
    if (current_val > state->max_val)
      state->max_val = current_val;
    if (current_val < state->min_val)
      state->min_val = current_val;
  }

  state->sample_count++;
  if (state->sample_count >= WINDOW_SIZE) {
    uint32_t diff = state->max_val - state->min_val;

    if (diff > 1500 && diff < 30000) {
      state->threshold = state->min_val + (diff / 2);
    } else {
      state->threshold = state->min_val + 4000;
    }

    state->sample_count = 0;
  }

  // Detección por Cruce por Cero (Zero Crossing) en sentido ascendente
  if (state->prev_filtered_mag_sq < state->threshold &&
      current_val >= state->threshold) {
    uint32_t delta_t = current_time_ms - state->last_step_time_ms;

    if (state->max_gyro_val > 400) {
      if (delta_t >= STEP_MIN_TIME_MS && delta_t <= STEP_MAX_TIME_MS) {
        state->consecutive_steps++;
        state->last_step_time_ms = current_time_ms;
        if (state->consecutive_steps == VALID_STEPS_THRESHOLD) {
          new_steps = VALID_STEPS_THRESHOLD;
        } else if (state->consecutive_steps > VALID_STEPS_THRESHOLD) {
          new_steps = 1;
        }
      } else if (delta_t > STEP_MAX_TIME_MS) {
        state->consecutive_steps = 1;
        state->last_step_time_ms = current_time_ms;
      }
    }

    state->max_gyro_val = 0;
  }

  return new_steps;
}

#endif

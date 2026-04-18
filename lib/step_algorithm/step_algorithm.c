#include "step_algorithm.h"
#include <math.h>

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
  state->last_step_time_ms = 0;
  state->steps_in_queue = 0;
  state->max_gyro_val = 0;

  // Inicializar tablas FFT genéricas
  dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
}

uint8_t step_algo_update(step_algo_state_t *state, int16_t ax, int16_t ay,
                         int16_t az, int16_t gx, int16_t gy, int16_t gz,
                         uint32_t current_time_ms) {
  uint8_t new_steps = 0;

  // En el S3 podemos calcular la magnitud lineal con math.h
  float mag = sqrtf((float)ax * ax + (float)ay * ay + (float)az * az);

  // Espacio Complejo: Real=mag, Imag=0.0f
  state->accel_window[state->sample_index * 2] = mag;
  state->accel_window[state->sample_index * 2 + 1] = 0.0f;

  float gyro_mag = sqrtf((float)gx * gx + (float)gy * gy + (float)gz * gz);
  if (gyro_mag > state->max_gyro_val) {
    state->max_gyro_val = (uint32_t)gyro_mag;
  }

  state->sample_index++;

  if (state->sample_index >= FFT_WINDOW_SIZE) {
    if (state->max_gyro_val > 400) {
      // 1. Quitar DC Bias (Gravedad y offset base) calculando promedio y
      // restando
      float dc_bias = 0;
      for (int i = 0; i < FFT_WINDOW_SIZE; i++) {
        dc_bias += state->accel_window[i * 2];
      }
      dc_bias /= FFT_WINDOW_SIZE;
      for (int i = 0; i < FFT_WINDOW_SIZE; i++) {
        state->accel_window[i * 2] -= dc_bias;
      }

      // 2. Aplicar ventana (Hann) para reducir fuga espectral "leakage"
      dsps_wind_hann_f32(state->accel_window, FFT_WINDOW_SIZE);

      // 3. Ejecutar FFT Radix-2
      dsps_fft2r_fc32(state->accel_window, FFT_WINDOW_SIZE);
      dsps_bit_rev_fc32(state->accel_window, FFT_WINDOW_SIZE);
      // Calcula magnitud de complejo a real.
      // dsps_cplx2reC_fc32 almacena la magnitud calculada sobre el mismo
      // buffer!
      dsps_cplx2reC_fc32(state->accel_window, FFT_WINDOW_SIZE);

      // 4. Buscar picos frecuenciales en la banda de caminata [1 Hz a 2.5 Hz]
      // Fs = 50Hz. Resolucion dF = 50/128 = 0.390625 Hz
      // Rango de Bins:
      // bin 3 = 1.17 Hz
      // bin 4 = 1.56 Hz
      // bin 5 = 1.95 Hz
      // bin 6 = 2.34 Hz
      // Exploraremos desde bin 2 (0.78 Hz) hasta bin 7 (2.73 Hz)

      float peak_power = 0.0f;
      int peak_bin = 0;

      for (int i = 2; i <= 7; i++) {
        // El output de dsps_cplx2reC_fc32 esta compactado como floats reales
        float bin_power = state->accel_window[i];
        if (bin_power > peak_power) {
          peak_power = bin_power;
          peak_bin = i;
        }
      }

      // Umbral FFT calibrado.
      // Nota: 15.0f es temporal. Para raw data ~16384 (1G) este número puede
      // variar agresivamente a 1e5 o similar, la simulación offline de python
      // nos dirá el multiplicador base a colocar.
      float UMBRAL_FFT = 28000.0f;

      if (peak_power > UMBRAL_FFT) {
        float dominant_frequency = peak_bin * 0.390625f;
        // Cálculo de Pasos: Segundos transcurridos en esta ventana * freq = N
        // pasos (128 / 50Hz) = 2.56 segs. Ej: Caminar a 2Hz son ~5 pasos
        // inferidos
        new_steps = (uint8_t)(dominant_frequency * 2.56f + 0.5f); // Redondeo
      }
    }

    // Reiniciar bloque
    state->sample_index = 0;
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
                         uint32_t current_time_ms) {
  uint8_t new_steps = 0;

  // Calcular la magnitud lineal real (el anterior usaba el cuadrado puro)
  // El uso del cuadrado (mag_sq) generaba números inmensos donde el ruido en
  // picos dominaba.
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

  // Filtro Pasa Bajos Exponencial (LPF) más suave para quitar ruidos erráticos
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

  // Algoritmo Min/Max dinámico para calcular un umbral centrado en la amplitud
  // del paso actual
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

    // Una diferencia asumiendo escala 2G (1G = 16384). Un paso razonable
    // debería generar diff > 1000
    if (diff > 300 && diff < 30000) {
      state->threshold = state->min_val + (diff / 2);
    } else {
      // Movimiento muy ligero (estático o temblor), establecemos un umbral alto
      // relativo al min para no gatillar pasos falsos
      state->threshold = state->min_val + 300;
    }

    state->sample_count = 0;
  }

  // Detección por Cruce por Cero (Zero Crossing) en sentido ascendente
  if (state->prev_filtered_mag_sq < state->threshold &&
      current_val >= state->threshold) {
    uint32_t delta_t = current_time_ms - state->last_step_time_ms;

    // Filtro adicional de falso positivo: La muñeca oscila al caminar.
    // Si la "energía rotacional" máxima desde el último paso es muy baja, es
    // probable que la aceleración venga de vibración externa translacional (Ej.
    // vehículo, tecleo brusco). Gyro threshold de ~25 dps (escala depende del
    // FSR del BMI160, asumiendo 16.4 LSB/dps -> 400).
    if (state->max_gyro_val > 100) {
      if (delta_t >= STEP_MIN_TIME_MS && delta_t <= STEP_MAX_TIME_MS) {
        state->consecutive_steps++;
        state->last_step_time_ms = current_time_ms;
        if (state->consecutive_steps == VALID_STEPS_THRESHOLD) {
          new_steps = VALID_STEPS_THRESHOLD;
        } else if (state->consecutive_steps > VALID_STEPS_THRESHOLD) {
          new_steps = 1;
        }
      } else if (delta_t > STEP_MAX_TIME_MS) {
        // Mucho tiempo sin movimiento, reiniciamos el umbral de secuencia
        // válida
        state->consecutive_steps = 1;
        state->last_step_time_ms = current_time_ms;
      }
    }

    // Reiniciamos el rastreador de energía rotacional para el próximo ciclo
    state->max_gyro_val = 0;
  }

  return new_steps;
}

#endif

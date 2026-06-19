#include "step_algorithm.h"
#include <math.h>
#include <string.h>

// --- PARÁMETROS GLOBALES COMPARTIDOS ---
#define STEP_MIN_TIME_MS 300
#define STEP_MAX_TIME_MS 2000
#define VALID_STEPS_THRESHOLD 4

// ==============================================================================
//          IMPLEMENTACIÓN PARA ESP32-S3 (Híbrido FFT + dominio del tiempo)
// ==============================================================================
//
// Muestreo fijo a 50 Hz uniforme (BMI160 ODR 50 Hz + FIFO). El conteo NO se hace
// por la FFT (eso cuantiza el conteo al índice del bin); la FFT solo DETECTA
// caminata y estima la cadencia. Los pasos se cuentan por peak-detection en el
// dominio del tiempo sobre la señal pasa-banda, con umbral adaptativo, periodo
// refractario y validación por racha con commit retroactivo.
//
#if defined(CONFIG_IDF_TARGET_ESP32S3)

#include "esp_dsp.h"
#include "esp_log.h"
static const char *STEP_TAG = "STEP_FFT";

#define FFT_WINDOW_SIZE   128
#define FFT_HOP           64          // 50% de solapamiento entre ventanas FFT
#define FS_HZ             50.0f
#define WIN_DURATION_S    (FFT_WINDOW_SIZE / FS_HZ)   // 2.56 s

// --- Cadena pasa-banda 0.5–5 Hz (IIR 1er orden cada etapa, a 50 Hz) ---
#define HP_ALPHA          0.94f       // pasa-altos ≈ 0.5 Hz (quita gravedad/DC)
#define LP_ALPHA          0.40f       // pasa-bajos ≈ 5 Hz   (quita jitter)

// --- Envolvente adaptativa y detector temporal ---
#define ENV_DECAY         0.04f       // velocidad de adaptación del umbral
#define AMP_MIN           600.0f      // amplitud pico-valle mínima (LSB ≈ 0.04 g)
#define HYST_FRAC         0.15f       // histéresis = 15% de la amplitud

// --- Gate espectral (FFT) ---
#define WALK_BAND_LO_HZ   0.70f
#define WALK_BAND_HI_HZ   3.00f
// Prominencia espectral mínima del pico de caminata respecto al piso de ruido
// medio (peak / mean). Robusto a armónicos: cada pisada es un impacto rico en
// armónicos, así que la fracción de energía en banda (inband/total) se diluye,
// pero el pico fundamental SIGUE sobresaliendo del promedio. Escala-invariante.
#define FFT_PROM_MIN      4.0f
#define FFT_RATIO_MIN     0.30f       // vía secundaria: energía concentrada en banda
#define GYRO_SOFT         500         // |gyro| LSB que ayuda a confirmar (entrada blanda)
#define WALK_GATE_MS      3500        // vigencia del gate tras la última detección

void step_algo_init(step_algo_state_t *state) {
  memset(state, 0, sizeof(*state));
  // Inicializar las tablas FFT para EXACTAMENTE N=128. CRÍTICO: la tabla de
  // twiddles (dsps_gen_w_r2_fc32) se genera para el tamaño que se pasa aquí.
  // Si se inicializa con un tamaño distinto al de la FFT que se corre (p.ej.
  // CONFIG_DSP_MAX_FFT_SIZE=4096), los factores de giro cos(2πk/4096) no
  // corresponden a cos(2πk/128) → la FFT devuelve un espectro incorrecto que
  // NO sigue la frecuencia real de la señal. Debe coincidir con FFT_WINDOW_SIZE.
  esp_err_t e = dsps_fft2r_init_fc32(NULL, FFT_WINDOW_SIZE);
  if (e != ESP_OK) {
    ESP_LOGE(STEP_TAG, "dsps_fft2r_init_fc32(%d) FALLÓ: %s (0x%x)",
             FFT_WINDOW_SIZE, esp_err_to_name(e), e);
  } else {
    ESP_LOGI(STEP_TAG, "FFT init OK (N=%d)", FFT_WINDOW_SIZE);
  }
}

// Corre la FFT sobre la ventana de 128 muestras pasa-banda y actualiza el gate
// de caminata + la cadencia estimada. No cuenta pasos.
static void fft_walk_gate(step_algo_state_t *state, uint32_t now) {
  // Ventana Hann PRECALCULADA una sola vez. OJO CRÍTICO: dsps_wind_hann_f32()
  // GENERA los coeficientes de la ventana DENTRO del array que recibe — NO
  // multiplica una señal existente. Hay que generarla aparte y multiplicar la
  // señal por ella al armar el buffer complejo (igual que el ejemplo oficial).
  static __attribute__((aligned(16))) float hann[FFT_WINDOW_SIZE];
  static bool hann_ready = false;
  if (!hann_ready) {
    dsps_wind_hann_f32(hann, FFT_WINDOW_SIZE);
    hann_ready = true;
  }

  // CRÍTICO: la FFT optimizada del ESP32-S3 (dsps_fft2r_fc32_aes3, ensamblador)
  // exige buffers alineados a 16 bytes, igual que el ejemplo/tests de esp-dsp.
  __attribute__((aligned(16))) float cbuf[FFT_WINDOW_SIZE * 2];
  for (int i = 0; i < FFT_WINDOW_SIZE; i++) {
    cbuf[i * 2]     = state->bp_mags[i] * hann[i];  // señal × ventana
    cbuf[i * 2 + 1] = 0.0f;
  }
  esp_err_t fe = dsps_fft2r_fc32(cbuf, FFT_WINDOW_SIZE);
  if (fe != ESP_OK) {
    static bool logged = false;
    if (!logged) {
      logged = true;
      ESP_LOGE(STEP_TAG, "dsps_fft2r_fc32 FALLÓ: %s (0x%x)", esp_err_to_name(fe), fe);
    }
  }
  dsps_bit_rev_fc32(cbuf, FFT_WINDOW_SIZE);

  int lo = (int)ceilf(WALK_BAND_LO_HZ * WIN_DURATION_S);   // bin 2
  int hi = (int)floorf(WALK_BAND_HI_HZ * WIN_DURATION_S);  // bin 7
  if (lo < 1) lo = 1;
  if (hi > FFT_WINDOW_SIZE / 2 - 1) hi = FFT_WINDOW_SIZE / 2 - 1;

  // Potencia por bin. IMPORTANTE: excluimos el bin 1 (0.39 Hz) del piso de ruido.
  // Los transitorios de arranque/parada de movimiento crean una rampa lenta de
  // ~2.56 s = bin 1, que domina el espectro e infla el denominador de la
  // prominencia, aplastando el pico de caminata real. El bin 1 = 23 pasos/min,
  // está por debajo de cualquier cadencia de caminata → descartarlo es correcto.
  float pw[FFT_WINDOW_SIZE / 2];
  float total = 0.0f, inband = 0.0f, peak_p = 0.0f;
  int peak_k = lo;
  int n_bins = 0;
  // Pico global (toda la banda 0..25 Hz) solo para debug: dónde está la energía
  float gpeak_p = 0.0f; int gpeak_k = 1;
  for (int k = 1; k < FFT_WINDOW_SIZE / 2; k++) {
    float re = cbuf[k * 2];
    float im = cbuf[k * 2 + 1];
    pw[k] = re * re + im * im;
    if (pw[k] > gpeak_p) { gpeak_p = pw[k]; gpeak_k = k; }
    if (k >= 2) {            // piso/total: desde bin 2 (excluye el transitorio del bin 1)
      total += pw[k];
      n_bins++;
    }
  }
  state->dbg_peak_hz = (float)gpeak_k / WIN_DURATION_S;
  state->dbg_fft_runs++;
  for (int k = lo; k <= hi; k++) {
    inband += pw[k];
    if (pw[k] > peak_p) { peak_p = pw[k]; peak_k = k; }
  }

  // Métricas escala-invariantes (no dependen de la amplitud absoluta / rango):
  //  - prominence: cuánto sobresale el pico de caminata del piso medio. Robusto
  //    a armónicos (el 2.º armónico cae fuera de banda pero es OTRO pico, no
  //    impide que el fundamental destaque). Es la métrica principal.
  //  - ratio: fracción de energía concentrada en banda (vía secundaria).
  float mean_pw = (total > 0.0f && n_bins > 0) ? (total / (float)n_bins) : 1.0f;
  float prominence = (mean_pw > 1.0f) ? (peak_p / mean_pw) : 0.0f;
  float ratio = (total > 1.0f) ? (inband / total) : 0.0f;
  float amp = state->peak_env - state->valley_env;
  state->dbg_prominence = prominence;
  state->dbg_ratio = ratio;
  bool gyro_help = (state->max_gyro_val > GYRO_SOFT);
  bool is_walk = (amp > AMP_MIN) &&
                 (prominence > FFT_PROM_MIN ||
                  ratio > FFT_RATIO_MIN ||
                  (gyro_help && prominence > FFT_PROM_MIN * 0.6f));

  if (is_walk) {
    // Interpolación parabólica del pico → cadencia fraccional (Hz)
    float cad_bin = (float)peak_k;
    if (peak_k > 1 && peak_k < FFT_WINDOW_SIZE / 2 - 1) {
      float a = pw[peak_k - 1], b = pw[peak_k], c = pw[peak_k + 1];
      float denom = a - 2.0f * b + c;
      if (fabsf(denom) > 1e-3f) {
        float d = 0.5f * (a - c) / denom;
        if (d > -1.0f && d < 1.0f) cad_bin = (float)peak_k + d;
      }
    }
    state->cadence_hz = cad_bin / WIN_DURATION_S;
    state->walk_gate = true;
    state->walk_gate_expiry_ms = now + WALK_GATE_MS;
  }
}

uint8_t step_algo_update(step_algo_state_t *state, int16_t ax, int16_t ay,
                         int16_t az, int16_t gx, int16_t gy, int16_t gz,
                         uint32_t current_time_ms) {
  uint8_t new_steps = 0;

  // ── 1. Magnitud lineal del acelerómetro ──────────────────────────────
  float mag = sqrtf((float)ax * ax + (float)ay * ay + (float)az * az);

  // ── 2. Pasa-banda IIR (pasa-altos quita gravedad, pasa-bajos quita ruido)
  float hp = HP_ALPHA * (state->hp_prev + mag - state->prev_raw_mag);
  state->hp_prev = hp;
  state->prev_raw_mag = mag;
  float bp = state->lp_prev + LP_ALPHA * (hp - state->lp_prev);
  state->lp_prev = bp;

  // ── 3. Envolvente adaptativa pico/valle (ataque rápido, decay lento) ──
  if (bp > state->peak_env)   state->peak_env = bp;
  else                        state->peak_env += (bp - state->peak_env) * ENV_DECAY;
  if (bp < state->valley_env) state->valley_env = bp;
  else                        state->valley_env += (bp - state->valley_env) * ENV_DECAY;

  float amp = state->peak_env - state->valley_env;
  float thr = 0.5f * (state->peak_env + state->valley_env);
  float hyst = HYST_FRAC * amp;

  // ── 4. Detección de paso por cruce de umbral con histéresis ──────────
  if (!state->above && bp > thr + hyst) {
    state->above = true;  // flanco ascendente → candidato a paso

    if (amp > AMP_MIN) {
      uint32_t dt = current_time_ms - state->last_step_time_ms;
      // Intervalo mínimo: 0.6× el periodo de cadencia FFT, con piso físico
      uint32_t period_ms = (state->cadence_hz > 0.1f)
                               ? (uint32_t)(1000.0f / state->cadence_hz) : 500;
      uint32_t min_int = (uint32_t)(0.6f * (float)period_ms);
      if (min_int < STEP_MIN_TIME_MS) min_int = STEP_MIN_TIME_MS;

      if (dt >= min_int && dt <= STEP_MAX_TIME_MS) {
        state->consecutive_steps++;
        state->last_step_time_ms = current_time_ms;
        bool gate = (current_time_ms < state->walk_gate_expiry_ms);

        if (state->consecutive_steps >= VALID_STEPS_THRESHOLD && gate) {
          // Caminata validada y confirmada por la FFT: descargar el buffer
          // provisional (commit retroactivo) + este paso.
          new_steps = state->provisional_steps + 1;
          state->provisional_steps = 0;
        } else {
          // Aún no validada (racha corta) o sin gate: bufferear.
          state->provisional_steps++;
        }
      } else if (dt > STEP_MAX_TIME_MS) {
        // Cadencia rota → descartar provisionales no confirmados y reiniciar
        state->consecutive_steps = 1;
        state->provisional_steps = 1;
        state->last_step_time_ms = current_time_ms;
      }
      // dt < min_int → periodo refractario: ignorar (no actualizar nada)
    }
  } else if (state->above && bp < thr - hyst) {
    state->above = false;  // flanco descendente
  }

  // ── 5. Acumular |gyro| (entrada blanda para el gate) ─────────────────
  float gyro_mag = sqrtf((float)gx * gx + (float)gy * gy + (float)gz * gz);
  if (gyro_mag > state->max_gyro_val) state->max_gyro_val = (uint32_t)gyro_mag;

  // ── 6. Guardar muestra pasa-banda y correr la FFT de detección ───────
  state->bp_mags[state->sample_index++] = bp;
  if (state->sample_index >= FFT_WINDOW_SIZE) {
    fft_walk_gate(state, current_time_ms);
    // Deslizar la ventana 50% (solapamiento determinístico)
    memmove(&state->bp_mags[0], &state->bp_mags[FFT_HOP],
            FFT_HOP * sizeof(float));
    state->sample_index = FFT_HOP;
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

#ifdef ENV_TEST_FFT_STEPS

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include "step_algorithm.h"

static const char *TAG = "TEST_FFT";

#define FFT_N   128
#define FS_HZ   50.0f

// ─────────────────────────────────────────────────────────────────────────
//  PARTE 1: Validación de la FFT pura con datos sintéticos
//  Genera un seno de frecuencia conocida a 50 Hz / 128 muestras (lo que usa
//  el pedómetro) y verifica que el pico espectral caiga en el bin correcto:
//      bin_esperado = f_hz * N / Fs = f_hz * 128 / 50 = f_hz * 2.56
// ─────────────────────────────────────────────────────────────────────────
static void fft_test_tone(float freq_hz, bool apply_hann) {
  __attribute__((aligned(16))) float win[FFT_N];
  __attribute__((aligned(16))) float cbuf[FFT_N * 2];

  // Seno puro (sin DC) a freq_hz
  for (int i = 0; i < FFT_N; i++) {
    win[i] = sinf(2.0f * (float)M_PI * freq_hz * (float)i / FS_HZ);
  }
  if (apply_hann) {
    dsps_wind_hann_f32(win, FFT_N);
  }

  for (int i = 0; i < FFT_N; i++) {
    cbuf[i * 2]     = win[i];
    cbuf[i * 2 + 1] = 0.0f;
  }

  esp_err_t fe = dsps_fft2r_fc32(cbuf, FFT_N);
  esp_err_t be = dsps_bit_rev_fc32(cbuf, FFT_N);

  float peak_p = 0.0f; int peak_k = 0;
  for (int k = 1; k < FFT_N / 2; k++) {
    float re = cbuf[k * 2], im = cbuf[k * 2 + 1];
    float p = re * re + im * im;
    if (p > peak_p) { peak_p = p; peak_k = k; }
  }

  float bin_esperado = freq_hz * (float)FFT_N / FS_HZ;
  ESP_LOGI(TAG, "  f=%.2fHz hann=%d -> pico en bin %d (%.2fHz) | esperado bin %.1f | fft_ret=%d bitrev_ret=%d",
           (double)freq_hz, apply_hann, peak_k, (double)(peak_k * FS_HZ / FFT_N),
           (double)bin_esperado, fe, be);

  // Volcado de los primeros bins para inspección
  char line[160]; int off = 0;
  off += snprintf(line + off, sizeof(line) - off, "    bins[0..10]:");
  for (int k = 0; k <= 10 && off < (int)sizeof(line) - 12; k++) {
    float re = cbuf[k * 2], im = cbuf[k * 2 + 1];
    float p = sqrtf(re * re + im * im);
    off += snprintf(line + off, sizeof(line) - off, " %.0f", (double)p);
  }
  ESP_LOGI(TAG, "%s", line);
}

// Prueba la FFT con un tamaño de init dado: 2 Hz debe caer en bin ~5.
static void fft_test_init_size(int init_size) {
  esp_err_t e = dsps_fft2r_init_fc32(NULL, init_size);
  ESP_LOGI(TAG, "--- init(N=%d) ret=%d (%s) ---", init_size, e, esp_err_to_name(e));
  if (e == ESP_OK) {
    fft_test_tone(1.0f, false);   // esperado bin ~2.6
    fft_test_tone(2.0f, false);   // esperado bin ~5.1
    fft_test_tone(3.0f, false);   // esperado bin ~7.7
  }
  dsps_fft2r_deinit_fc32();
}

static void fft_selftest(void) {
  ESP_LOGI(TAG, "==================================================");
  ESP_LOGI(TAG, "   PARTE 1: VALIDACIÓN FFT PURA (N=128 @ 50 Hz)   ");
  ESP_LOGI(TAG, "   La FFT siempre corre con N=128; variamos solo  ");
  ESP_LOGI(TAG, "   el tamaño con que se INICIALIZA la tabla.      ");
  ESP_LOGI(TAG, "==================================================");

  fft_test_init_size(128);    // init == tamaño FFT
  fft_test_init_size(1024);   // tabla pre-construida del S3
  fft_test_init_size(4096);   // CONFIG_DSP_MAX_FFT_SIZE (lo que usábamos)

  ESP_LOGI(TAG, ">> Si algún init hace que 'pico en bin' SIGA a 'esperado',");
  ESP_LOGI(TAG, ">> ese es el init correcto. Si TODOS dan bin 1, es otra cosa.");

  // Dejar inicializado en 128 para la PARTE 2
  dsps_fft2r_init_fc32(NULL, FFT_N);
}

// ─────────────────────────────────────────────────────────────────────────
//  PARTE 2: Pedómetro completo con señales sintéticas
// ─────────────────────────────────────────────────────────────────────────
static float get_noise(float amplitude) {
  static uint32_t seed = 12345;
  seed = (seed * 1103515245 + 12345) & 0x7fffffff;
  return (((float)seed / (float)0x7fffffff) - 0.5f) * 2.0f * amplitude;
}

static uint32_t feed_walk(step_algo_state_t *st, uint32_t *t_ms,
                          int n, float freq_hz, float amp_lsb, float gyro_lsb,
                          double *phase) {
  uint32_t steps = 0;
  for (int i = 0; i < n; i++) {
    *phase += 2.0 * M_PI * freq_hz * 0.02;
    int16_t az = 16384 + (int16_t)(sin(*phase) * amp_lsb) + (int16_t)get_noise(150.0f);
    int16_t gy = (int16_t)(sin(*phase) * gyro_lsb);
    steps += step_algo_update(st, 0, 0, az, 0, gy, 0, *t_ms);
    *t_ms += 20;
  }
  return steps;
}

static uint32_t feed_rest(step_algo_state_t *st, uint32_t *t_ms, int n) {
  uint32_t steps = 0;
  for (int i = 0; i < n; i++) {
    int16_t az = 16384 + (int16_t)get_noise(100.0f);
    steps += step_algo_update(st, 0, 0, az, (int16_t)get_noise(30.0f),
                              (int16_t)get_noise(30.0f), (int16_t)get_noise(30.0f), *t_ms);
    *t_ms += 20;
  }
  return steps;
}

static void report(const char *name, uint32_t got, uint32_t lo, uint32_t hi) {
  ESP_LOGI(TAG, "[%s] pasos=%lu (esperado %lu..%lu) -> %s",
           name, (unsigned long)got, (unsigned long)lo, (unsigned long)hi,
           (got >= lo && got <= hi) ? "PASS" : "FAIL");
}

static void pedometer_test(void) {
  ESP_LOGI(TAG, "==================================================");
  ESP_LOGI(TAG, "      PARTE 2: PEDÓMETRO (señales sintéticas)     ");
  ESP_LOGI(TAG, "==================================================");

  step_algo_state_t st;
  uint32_t t; double phase;

  step_algo_init(&st); t = 0;
  report("Reposo 3s", feed_rest(&st, &t, 150), 0, 0);

  step_algo_init(&st); t = 0; phase = 0.0;
  report("Caminata 1.8Hz 6s", feed_walk(&st, &t, 300, 1.8f, 3276.0f, 600.0f, &phase), 9, 12);

  step_algo_init(&st); t = 0; phase = 0.0;
  report("Caminata suave sin gyro", feed_walk(&st, &t, 300, 1.8f, 1960.0f, 0.0f, &phase), 7, 12);
}

void test_fft_steps_task(void *pvParameters) {
  fft_selftest();
  pedometer_test();
  ESP_LOGI(TAG, "============== TEST COMPLETADO ==============");
  while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}

void app_main(void) {
  vTaskDelay(pdMS_TO_TICKS(1000));
  xTaskCreate(test_fft_steps_task, "test_fft_task", 8192, NULL, 5, NULL);
}

#endif

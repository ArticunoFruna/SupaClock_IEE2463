#ifdef ENV_TEST_FFT_STEPS

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "step_algorithm.h"

static const char *TAG = "TEST_FFT_STEPS";

// Generador de ruido blanco básico (PRNG)
static float get_noise(float amplitude) {
  static uint32_t seed = 12345;
  seed = (seed * 1103515245 + 12345) & 0x7fffffff;
  return (((float)seed / (float)0x7fffffff) - 0.5f) * 2.0f * amplitude;
}

void test_fft_steps_task(void *pvParameters) {
  ESP_LOGI(TAG, "==================================================");
  ESP_LOGI(TAG, "   INICIANDO SIMULACIÓN DE PEDÓMETRO FFT (S3)    ");
  ESP_LOGI(TAG, "==================================================");

  step_algo_state_t state;
  step_algo_init(&state);

  uint32_t current_time_ms = 0;
  uint32_t cumulative_steps = 0;
  uint8_t steps_returned = 0;

  // ----------------------------------------------------
  // ESCENARIO 1: Estado de reposo / Estático (3.0 segundos)
  // Fs = 50 Hz (20ms por muestra). Total: 150 muestras.
  // ----------------------------------------------------
  ESP_LOGI(TAG, "[Escenario 1] Reposo estático - 3.0s de vibración leve (ruido)");
  for (int i = 0; i < 150; i++) {
    // 1G = 16384 LSB. Ruido de +/- 100 LSB (0.006G)
    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 16384 + (int16_t)get_noise(100.0f);
    
    // Giroscopio con ruido leve (bajo el umbral de 400)
    int16_t gx = (int16_t)get_noise(30.0f);
    int16_t gy = (int16_t)get_noise(30.0f);
    int16_t gz = (int16_t)get_noise(30.0f);

    steps_returned = step_algo_update(&state, ax, ay, az, gx, gy, gz, current_time_ms, true);
    if (steps_returned > 0) {
      cumulative_steps += steps_returned;
      ESP_LOGE(TAG, "¡ERROR! Pasos falsos detectados en reposo: %d", steps_returned);
    }

    current_time_ms += 20; // 50 Hz
  }
  ESP_LOGI(TAG, "[Escenario 1] Pasos acumulados en reposo: %lu (Esperado: 0) -> %s",
           (unsigned long)cumulative_steps, (cumulative_steps == 0) ? "PASS" : "FAIL");

  cumulative_steps = 0;

  // ----------------------------------------------------
  // ESCENARIO 2: Gesto repentino / Sacudida (1.5 segundos)
  // Un temblor rápido a 4.0 Hz. Total: 75 muestras.
  // Seguido de 2.0 segundos de reposo (100 muestras).
  // ----------------------------------------------------
  ESP_LOGI(TAG, "[Escenario 2] Sacudida rápida (1.5s a 4.0Hz) - Prueba de Histéresis");
  for (int i = 0; i < 175; i++) {
    int16_t ax = 0, ay = 0, az = 16384;
    int16_t gx = 0, gy = 0, gz = 0;

    if (i < 75) {
      // Sacudida de alta potencia
      float angle = 2.0f * M_PI * 4.0f * ((float)i * 0.02f);
      az += (int16_t)(sinf(angle) * 8000.0f); // 0.5G de amplitud
      gx = 800; // Giroscopio activo (> 400)
    } else {
      // Reposo posterior
      az += (int16_t)get_noise(50.0f);
      gx = (int16_t)get_noise(20.0f);
    }

    steps_returned = step_algo_update(&state, ax, ay, az, gx, gy, gz, current_time_ms, true);
    if (steps_returned > 0) {
      cumulative_steps += steps_returned;
      ESP_LOGW(TAG, "Gesto detectado (se descartará si es aislado): +%d pasos", steps_returned);
    }

    current_time_ms += 20;
  }
  ESP_LOGI(TAG, "[Escenario 2] Pasos acumulados en sacudida aislada: %lu (Esperado: 0) -> %s",
           (unsigned long)cumulative_steps, (cumulative_steps == 0) ? "PASS" : "FAIL");

  cumulative_steps = 0;

  // ----------------------------------------------------
  // ESCENARIO 3: Caminata constante a 1.8 Hz (6.0 segundos)
  // Caminata rítmica. Total: 300 muestras.
  // ----------------------------------------------------
  ESP_LOGI(TAG, "[Escenario 3] Caminata constante a 1.8 Hz (ritmo regular) durante 6.0s");
  
  // Reiniciar histéresis
  step_algo_init(&state);

  for (int i = 0; i < 300; i++) {
    float time_s = (float)i * 0.02f;
    float angle = 2.0f * M_PI * 1.8f * time_s; // 1.8 Hz

    // Aceleración vertical limpia: Gravedad + aceleración de impacto (0.2G de amplitud)
    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 16384 + (int16_t)(sinf(angle) * 3276.0f) + (int16_t)get_noise(200.0f);

    // Giroscopio con rotación rítmica durante el paso (> 400 LSB)
    int16_t gx = 0;
    int16_t gy = (int16_t)(sinf(angle) * 600.0f);
    int16_t gz = 0;

    steps_returned = step_algo_update(&state, ax, ay, az, gx, gy, gz, current_time_ms, true);
    if (steps_returned > 0) {
      cumulative_steps += steps_returned;
      ESP_LOGI(TAG, "¡Pasos validados! Ventana detectó +%d pasos. Total acumulado: %lu",
               steps_returned, (unsigned long)cumulative_steps);
    }

    current_time_ms += 20;
  }

  // 6.0 segundos a 1.8 Hz = 1.8 * 6 = 10.8 pasos. Esperamos alrededor de 9 a 11 pasos validados!
  ESP_LOGI(TAG, "[Escenario 3] Pasos acumulados en caminata: %lu (Esperado: 9 a 11) -> %s",
           (unsigned long)cumulative_steps, (cumulative_steps >= 9 && cumulative_steps <= 11) ? "PASS" : "FAIL");

  ESP_LOGI(TAG, "==================================================");
  ESP_LOGI(TAG, "        SIMULACIÓN DE PEDÓMETRO COMPLETADA        ");
  ESP_LOGI(TAG, "==================================================");

  while(1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void app_main(void) {
  // Retardo de inicio para la visualización del monitor de puerto serie
  vTaskDelay(pdMS_TO_TICKS(1000));
  xTaskCreate(test_fft_steps_task, "test_fft_task", 6144, NULL, 5, NULL);
}

#endif

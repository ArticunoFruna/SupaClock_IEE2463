/**
 * @file har_cnn1d.c
 * @brief Implementación del HAR CNN-1D + detector de caídas.
 *
 * Este archivo es el "engine" del módulo. Asume que existe el modelo
 * cuantizado como blob C en `har_model.cc` (símbolo extern al final).
 *
 * Para entrenar el modelo y exportar el .tflite, ver:
 *   tools/har/train_har_cnn1d.py
 *   tools/har/export_tflite_to_c.sh
 */
#include "har_cnn1d.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "bmi160.h"
#include <math.h>
#include <string.h>

static const char *TAG = "HAR";

/* ───────────── Ring buffer estático (SRAM interna) ───────────── */
/* 200 muestras × 6 canales × 2 bytes = 2.4 kB. Se queda en SRAM. */
static int16_t  s_ring[HAR_WINDOW_SIZE][HAR_CHANNELS];
static uint32_t s_write_idx;
static uint32_t s_samples_since_inference;

/* ───────────── Estado de inferencia ───────────── */
static har_result_t s_last_result = { .state = HAR_STATE_UNKNOWN };
static har_result_cb_t s_cb;
static void *s_cb_user;
static TaskHandle_t s_task_handle;
static volatile bool s_paused;

/* ───────────── Modelo embebido (provisto por har_model.cc) ─────────────
 * El blob es generado con `xxd -i model.tflite > har_model.cc`.
 * Mientras no exista, definimos un weak placeholder para que compile. */
extern const unsigned char har_model_tflite[] __attribute__((weak));
extern const unsigned int  har_model_tflite_len __attribute__((weak));

/* ───────────── Tensor arena ─────────────
 * Aumentado a 128 kB para soportar el modelo más robusto en PSRAM.
 * Si en PSRAM, se aloca con caps SPIRAM; si en SRAM, lo dejamos estático. */
#define HAR_ARENA_BYTES    (128 * 1024)
#if HAR_TENSOR_ARENA_IN_PSRAM
static uint8_t *s_arena;
#else
static uint8_t  s_arena[HAR_ARENA_BYTES] __attribute__((aligned(16)));
#endif
static size_t   s_arena_used;

/* Detección de caídas integrada directamente en la red neuronal CNN-1D (Clase 3) */

/* ───────────── Inferencia ─────────────
 * El intérprete TFLite-Micro vive en C++; aquí dejamos un *hook* que
 * `har_model_runner.cpp` implementa. Si no hay modelo cargado, devolvemos
 * un fallback heurístico basado en varianza para que el resto del sistema
 * compile y corra. */
extern bool har_runner_init(uint8_t *arena, size_t arena_bytes,
                            const unsigned char *model_blob,
                            size_t model_len, size_t *arena_used);
extern bool har_runner_run(const int16_t window[HAR_WINDOW_SIZE][HAR_CHANNELS],
                           float probs[HAR_NUM_CLASSES]) __attribute__((weak));

/* Fallback heurístico (usado mientras no haya modelo entrenado).
 * Clasifica por la varianza de |a| en la ventana para las 4 clases. */
static void heuristic_infer(float probs[HAR_NUM_CLASSES]) {
    int64_t mean_a2 = 0;
    int64_t var_a2  = 0;
    for (int i = 0; i < HAR_WINDOW_SIZE; ++i) {
        int32_t a2 = (int32_t)s_ring[i][0] * s_ring[i][0]
                   + (int32_t)s_ring[i][1] * s_ring[i][1]
                   + (int32_t)s_ring[i][2] * s_ring[i][2];
        mean_a2 += a2;
    }
    mean_a2 /= HAR_WINDOW_SIZE;
    for (int i = 0; i < HAR_WINDOW_SIZE; ++i) {
        int32_t a2 = (int32_t)s_ring[i][0] * s_ring[i][0]
                   + (int32_t)s_ring[i][1] * s_ring[i][1]
                   + (int32_t)s_ring[i][2] * s_ring[i][2];
        int64_t d = a2 - mean_a2;
        var_a2 += (d * d) >> 20;  /* normaliza para evitar overflow */
    }
    float v = (float)var_a2 / HAR_WINDOW_SIZE;
    /* Umbrales empíricos — sustituidos por la CNN cuando esté entrenada.
     * Mapeo: 0: reposo, 1: caminata, 2: trote, 3: caída. */
    if (v < 50.0f) {
        probs[0] = 0.90f; probs[1] = 0.08f; probs[2] = 0.01f; probs[3] = 0.01f;
    } else if (v < 5000.0f) {
        probs[0] = 0.05f; probs[1] = 0.85f; probs[2] = 0.08f; probs[3] = 0.02f;
    } else if (v < 20000.0f) {
        probs[0] = 0.02f; probs[1] = 0.10f; probs[2] = 0.85f; probs[3] = 0.03f;
    } else {
        probs[0] = 0.05f; probs[1] = 0.05f; probs[2] = 0.10f; probs[3] = 0.80f;
    }
}

static void run_inference_on_window(void) {
    float probs[HAR_NUM_CLASSES] = {0};
    bool ok = false;
    if (har_runner_run) {
        ok = har_runner_run((const int16_t (*)[HAR_CHANNELS])s_ring, probs);
    }
    if (!ok) {
        heuristic_infer(probs);
    }

    int argmax = 0;
    for (int i = 1; i < HAR_NUM_CLASSES; ++i) {
        if (probs[i] > probs[argmax]) argmax = i;
    }

    har_result_t r = {
        .state        = (har_state_t)argmax,
        .confidence   = probs[argmax],
        .fall_event   = (argmax == HAR_STATE_FALL),
        .timestamp_us = esp_timer_get_time(),
    };
    memcpy(r.probs, probs, sizeof(probs));
    s_last_result = r;

    if (s_cb) s_cb(&r, s_cb_user);
}

/* ───────────── Task principal ───────────── */
static void har_task(void *arg) {
    (void)arg;
    TickType_t period = pdMS_TO_TICKS(1000 / HAR_SAMPLE_RATE_HZ);
    TickType_t last = xTaskGetTickCount();

    // Acumuladores estáticos para promediar cada 2 muestras físicas (100 Hz -> 50 Hz)
    static int32_t s_acc_ax = 0, s_acc_ay = 0, s_acc_az = 0;
    static int32_t s_acc_gx = 0, s_acc_gy = 0, s_acc_gz = 0;
    static int s_acc_count = 0;

    while (1) {
        vTaskDelayUntil(&last, period);
        if (s_paused) continue;

        int16_t ax, ay, az, gx, gy, gz;
        if (bmi160_read_accel_gyro(&ax, &ay, &az, &gx, &gy, &gz) != ESP_OK) {
            continue;
        }

        // Acumular muestras físicas
        s_acc_ax += ax;
        s_acc_ay += ay;
        s_acc_az += az;
        s_acc_gx += gx;
        s_acc_gy += gy;
        s_acc_gz += gz;
        s_acc_count++;

        // Promediar y procesar cada 2 muestras físicas (frecuencia efectiva de 50 Hz)
        if (s_acc_count >= 2) {
            int16_t avg_ax = s_acc_ax / 2;
            int16_t avg_ay = s_acc_ay / 2;
            int16_t avg_az = s_acc_az / 2;
            int16_t avg_gx = s_acc_gx / 2;
            int16_t avg_gy = s_acc_gy / 2;
            int16_t avg_gz = s_acc_gz / 2;

            // Resetear acumulador
            s_acc_ax = 0; s_acc_ay = 0; s_acc_az = 0;
            s_acc_gx = 0; s_acc_gy = 0; s_acc_gz = 0;
            s_acc_count = 0;

            /* Push al ring */
            s_ring[s_write_idx][0] = avg_ax;
            s_ring[s_write_idx][1] = avg_ay;
            s_ring[s_write_idx][2] = avg_az;
            s_ring[s_write_idx][3] = avg_gx;
            s_ring[s_write_idx][4] = avg_gy;
            s_ring[s_write_idx][5] = avg_gz;
            s_write_idx = (s_write_idx + 1) % HAR_WINDOW_SIZE;
            s_samples_since_inference++;

            /* Inferencia cada HOP muestras (100 muestras / 50 Hz = 2 s) */
            if (s_samples_since_inference >= HAR_HOP_SIZE) {
                s_samples_since_inference = 0;
                run_inference_on_window();
            }
        }
    }
}

/* ───────────── API pública ───────────── */
esp_err_t har_cnn1d_init(har_result_cb_t cb, void *cb_user) {
    s_cb = cb;
    s_cb_user = cb_user;
    memset(s_ring, 0, sizeof(s_ring));
    s_write_idx = 0;
    s_samples_since_inference = 0;

#if HAR_TENSOR_ARENA_IN_PSRAM
    s_arena = (uint8_t *)heap_caps_aligned_alloc(16, HAR_ARENA_BYTES, MALLOC_CAP_SPIRAM);
    if (!s_arena) {
        ESP_LOGE(TAG, "No hay PSRAM para tensor arena (%d B)", HAR_ARENA_BYTES);
        return ESP_ERR_NO_MEM;
    }
#endif

    bool model_ok = false;
    if (&har_model_tflite != NULL && &har_model_tflite_len != NULL) {
        model_ok = har_runner_init(s_arena, HAR_ARENA_BYTES,
                                   har_model_tflite, har_model_tflite_len,
                                   &s_arena_used);
        ESP_LOGI(TAG, "Modelo TFLite %scargado (%u B used)",
                 model_ok ? "" : "NO ", (unsigned)s_arena_used);
    } else {
        ESP_LOGW(TAG, "Sin har_model.cc embebido → usando heurística por varianza");
    }

    /* Task pinned a core 1 (S3 es dual-core; core 0 lo usan BLE/LVGL). */
    BaseType_t r = xTaskCreatePinnedToCore(har_task, "har_task", 4096, NULL, 4,
                                            &s_task_handle, 1);
    return (r == pdPASS) ? ESP_OK : ESP_FAIL;
}

void har_cnn1d_pause(void)  { s_paused = true; }
void har_cnn1d_resume(void) { s_paused = false; }

har_result_t har_cnn1d_last(void) { return s_last_result; }

size_t har_cnn1d_arena_used(void) { return s_arena_used; }

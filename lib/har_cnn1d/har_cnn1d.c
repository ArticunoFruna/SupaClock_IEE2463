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

/* ───────────── Detector de caídas (heurística separada) ─────────────
 * Patrón clásico de 3 fases que la CNN no captura bien con ventanas de 2 s:
 *   1. Free-fall: |a| < 0.5 g durante > 80 ms
 *   2. Impact:    |a| > 3.0 g
 *   3. Stillness: |a| ≈ 1 g y giro < 30 dps durante > 1.5 s post-impacto
 *
 * Se alimenta con cada muestra (a 100 Hz). Costo: ~5 instrucciones/sample.
 */
typedef enum { FALL_IDLE, FALL_FREEFALL, FALL_POST_IMPACT } fall_phase_t;
static struct {
    fall_phase_t phase;
    uint32_t     freefall_count;
    uint32_t     post_count;
    bool         flag_event;     /* one-shot que setea s_last_result.fall_event */
} s_fall;

/* Para detectar caídas correctamente el BMI160 debe estar en rango ±8g
 * (3g a ±2g se clipea). LSB/g = 4096 a ±8g. La suma de 3 ejes² puede
 * llegar a ~3.2e9 → necesitamos int64_t para a2/g2. */
#define G_TO_RAW_8G(g)   ((int64_t)((g) * 4096.0f))
static inline bool fall_step(int16_t ax, int16_t ay, int16_t az,
                              int16_t gx, int16_t gy, int16_t gz) {
    int64_t a2 = (int64_t)ax * ax + (int64_t)ay * ay + (int64_t)az * az;
    int64_t g2 = (int64_t)gx * gx + (int64_t)gy * gy + (int64_t)gz * gz;
    /* Umbrales² evitan sqrt en hot path */
    const int64_t freefall_th2 = G_TO_RAW_8G(0.5f) * G_TO_RAW_8G(0.5f);
    const int64_t impact_th2   = G_TO_RAW_8G(3.0f) * G_TO_RAW_8G(3.0f);
    const int64_t rest_lo2     = G_TO_RAW_8G(0.8f) * G_TO_RAW_8G(0.8f);
    const int64_t rest_hi2     = G_TO_RAW_8G(1.2f) * G_TO_RAW_8G(1.2f);
    const int64_t gyro_quiet2  = (int64_t)(30 * 16) * (30 * 16); /* ~30 dps² scaled */

    switch (s_fall.phase) {
        case FALL_IDLE:
            if (a2 < freefall_th2) {
                s_fall.freefall_count = 1;
                s_fall.phase = FALL_FREEFALL;
            }
            break;
        case FALL_FREEFALL:
            if (a2 < freefall_th2) {
                s_fall.freefall_count++;
            } else if (a2 > impact_th2 && s_fall.freefall_count >= 8) {
                /* 8 muestras @ 100 Hz = 80 ms de free-fall mínimo */
                s_fall.phase = FALL_POST_IMPACT;
                s_fall.post_count = 0;
            } else {
                s_fall.phase = FALL_IDLE;
            }
            break;
        case FALL_POST_IMPACT:
            if (a2 > rest_lo2 && a2 < rest_hi2 && g2 < gyro_quiet2) {
                s_fall.post_count++;
                if (s_fall.post_count >= 150) {  /* 1.5 s de quietud */
                    s_fall.flag_event = true;
                    s_fall.phase = FALL_IDLE;
                    return true;
                }
            } else if (a2 > impact_th2) {
                /* nuevo impacto → resetear conteo */
                s_fall.post_count = 0;
            }
            /* timeout suave: si no llega quietud en 3 s, abortamos */
            if (s_fall.post_count == 0 &&
                ++s_fall.freefall_count > 300) {
                s_fall.phase = FALL_IDLE;
            }
            break;
    }
    return false;
}

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
 * Clasifica por la varianza de |a| en la ventana. */
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
    /* Umbrales empíricos — sustituidos por la CNN cuando esté entrenada */
    if (v < 50.0f)        { probs[0] = 0.9f; probs[1] = 0.08f; probs[2] = 0.02f; }
    else if (v < 5000.0f) { probs[0] = 0.05f; probs[1] = 0.85f; probs[2] = 0.10f; }
    else                  { probs[0] = 0.02f; probs[1] = 0.10f; probs[2] = 0.88f; }
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
        .fall_event   = s_fall.flag_event,
        .timestamp_us = esp_timer_get_time(),
    };
    memcpy(r.probs, probs, sizeof(probs));
    s_last_result = r;
    s_fall.flag_event = false;  /* consumido */

    if (s_cb) s_cb(&r, s_cb_user);
}

/* ───────────── Task principal ───────────── */
static void har_task(void *arg) {
    (void)arg;
    TickType_t period = pdMS_TO_TICKS(1000 / HAR_SAMPLE_RATE_HZ);
    TickType_t last = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&last, period);
        if (s_paused) continue;

        int16_t ax, ay, az, gx, gy, gz;
        if (bmi160_read_accel_gyro(&ax, &ay, &az, &gx, &gy, &gz) != ESP_OK) {
            continue;
        }

        /* Push al ring */
        s_ring[s_write_idx][0] = ax; s_ring[s_write_idx][1] = ay;
        s_ring[s_write_idx][2] = az; s_ring[s_write_idx][3] = gx;
        s_ring[s_write_idx][4] = gy; s_ring[s_write_idx][5] = gz;
        s_write_idx = (s_write_idx + 1) % HAR_WINDOW_SIZE;
        s_samples_since_inference++;

        /* Fall detector corre por muestra */
        fall_step(ax, ay, az, gx, gy, gz);

        /* Inferencia cada HOP muestras (1 s con HOP=100) */
        if (s_samples_since_inference >= HAR_HOP_SIZE) {
            s_samples_since_inference = 0;
            run_inference_on_window();
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
    memset(&s_fall, 0, sizeof(s_fall));

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

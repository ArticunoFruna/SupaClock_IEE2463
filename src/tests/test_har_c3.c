#ifdef ENV_TEST_HAR_C3

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "i2c_bus.h"
#include "bmi160.h"
#include "ble_telemetry.h"
#include "step_algorithm.h"
#include "har_cnn1d.h"


// ID de tipo TLV para transmitir la clasificación del estado de actividad
#define BLE_TLV_TYPE_HAR_STATE   0x08  /**< 1 B: u8 state (0=RESTING, 1=WALKING, 2=RUNNING, 3=FALL) */

static const char *TAG = "test_har_c3";
static step_algo_state_t s_step_state;
static uint32_t s_pedometer_steps = 0;

extern const unsigned char har_model_c3_tflite[];
extern const unsigned int  har_model_c3_tflite_len;

#define HAR_ARENA_BYTES (128 * 1024)
static uint8_t s_arena[HAR_ARENA_BYTES] __attribute__((aligned(16)));
static size_t s_arena_used = 0;
static bool s_tflite_ready = false;

bool har_runner_init(uint8_t *arena, size_t arena_bytes,
                     const unsigned char *model_blob,
                     size_t model_len, size_t *arena_used);
bool har_runner_run(const int16_t window[HAR_WINDOW_SIZE][6],
                    float probs[4]);

static int16_t s_ring[HAR_WINDOW_SIZE][6];
static uint32_t s_write_idx = 0;
static uint32_t s_samples_since_inference = 0;

static TaskHandle_t s_inference_task_handle = NULL;
static int16_t s_inference_window[HAR_WINDOW_SIZE][6];
static volatile bool s_inference_pending = false;


static const char *state_name(int s) {
    switch (s) {
        case 0: return "RESTING";
        case 1: return "WALKING";
        case 2: return "RUNNING";
        case 3: return "FALL";
        default:return "UNKNOWN";
    }
}

/*
static void heuristic_infer(int *state_out, float *conf_out) {
    float mean_mag = 0.0f;
    float mags[HAR_WINDOW_SIZE];

    for (int i = 0; i < HAR_WINDOW_SIZE; ++i) {
        float x = (float)s_ring[i][0] / 16384.0f;
        float y = (float)s_ring[i][1] / 16384.0f;
        float z = (float)s_ring[i][2] / 16384.0f;
        float mag = sqrtf(x * x + y * y + z * z);
        mags[i] = mag;
        mean_mag += mag;
    }
    mean_mag /= HAR_WINDOW_SIZE;

    float var_mag = 0.0f;
    for (int i = 0; i < HAR_WINDOW_SIZE; ++i) {
        float d = mags[i] - mean_mag;
        var_mag += d * d;
    }
    var_mag /= HAR_WINDOW_SIZE;

    // Filtro EMA Asimétrico:
    //   - α=0.15 al subir (varianza sube → lento, evita falsos positivos por movimientos transitorios)
    //   - α=0.40 al bajar (varianza baja → rápido, retorna a REPOSO en ~6s en vez de ~14s)
    static float s_v_ema = -1.0f;
    if (s_v_ema < 0.0f) {
        s_v_ema = var_mag;
    } else {
        float alpha = (var_mag < s_v_ema) ? 0.40f : 0.15f;
        s_v_ema = alpha * var_mag + (1.0f - alpha) * s_v_ema;
    }
    float v_smooth = s_v_ema;

    ESP_LOGI("test_har_c3", "Heuristic Var: v_raw=%.5f, v_smooth=%.5f", var_mag, v_smooth);

    int argmax = 0;
    float confidence = 0.0f;

    // Umbrales físicos calibrados en g^2:
    // RESTING: v_smooth < 0.015f (desviación estándar < 0.122g)
    // WALKING: v_smooth < 0.18f  (desviación estándar < 0.424g)
    // RUNNING: v_smooth >= 0.18f
    if (v_smooth < 0.015f) {
        argmax = 0; // REPOSO
        confidence = 0.90f;
    } else if (v_smooth < 0.18f) {
        argmax = 1; // CAMINAR
        confidence = 0.85f;
    } else {
        argmax = 2; // CORRER
        confidence = 0.85f;
    }

    *state_out = argmax;
    *conf_out = confidence;
}
*/


static void har_inference_task(void *pvParameters) {
    (void)pvParameters;
    float probs[4];


    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (s_tflite_ready) {
            memset(probs, 0, sizeof(probs));
            if (har_runner_run((const int16_t (*)[6])s_inference_window, probs)) {
                // Log mean absolute value of the window for debugging
                int32_t sum_ax = 0, sum_ay = 0, sum_az = 0;
                int32_t sum_gx = 0, sum_gy = 0, sum_gz = 0;
                for (int i = 0; i < HAR_WINDOW_SIZE; ++i) {
                    sum_ax += abs(s_inference_window[i][0]);
                    sum_ay += abs(s_inference_window[i][1]);
                    sum_az += abs(s_inference_window[i][2]);
                    sum_gx += abs(s_inference_window[i][3]);
                    sum_gy += abs(s_inference_window[i][4]);
                    sum_gz += abs(s_inference_window[i][5]);
                }
                ESP_LOGI(TAG, "Window Mean Abs: acc=[%d, %d, %d], gyro=[%d, %d, %d]",
                         (int)(sum_ax / HAR_WINDOW_SIZE), (int)(sum_ay / HAR_WINDOW_SIZE), (int)(sum_az / HAR_WINDOW_SIZE),
                         (int)(sum_gx / HAR_WINDOW_SIZE), (int)(sum_gy / HAR_WINDOW_SIZE), (int)(sum_gz / HAR_WINDOW_SIZE));

                // Obtener argmax instantáneo
                int argmax_instant = 0;
                for (int i = 1; i < 4; ++i) {
                    if (probs[i] > probs[argmax_instant]) argmax_instant = i;
                }

                static int s_consolidated_state = 0; // RESTING
                static int s_candidate_state = 0;
                static int s_consecutive_count = 0;

                if (argmax_instant == s_candidate_state) {
                    s_consecutive_count++;
                } else {
                    s_candidate_state = argmax_instant;
                    s_consecutive_count = 1;
                }

                if (s_consecutive_count >= 5) {
                    s_consolidated_state = s_candidate_state;
                }

                int state = s_consolidated_state;
                float confidence = probs[state];

                ESP_LOGI(TAG, "Instant ML HAR: %s [%.2f, %.2f, %.2f, %.2f]",
                         state_name(argmax_instant), probs[0], probs[1], probs[2], probs[3]);
                ESP_LOGI(TAG, "Consolidated Debounced HAR: %s (conf: %.2f, consecutive count: %d)",
                         state_name(state), confidence, s_consecutive_count);
                ESP_LOGI(TAG, "HAR state: %s (conf: %.2f)", state_name(state), confidence);

                // Enviar estado de HAR consolidado a través de BLE (TLV tipo 0x08, len 1)
                uint8_t state_val = (uint8_t)state;
                ble_tx_push(BLE_TLV_TYPE_HAR_STATE, &state_val, 1, 0xFF);

            } else {
                ESP_LOGE(TAG, "ML Inference failed");
            }
        }
        s_inference_pending = false;
    }
}

static void har_c3_task(void *pvParameters) {
    (void)pvParameters;
    TickType_t period = pdMS_TO_TICKS(10); // 100 Hz
    TickType_t last = xTaskGetTickCount();

    // Acumuladores para promediar cada 2 muestras físicas (100 Hz -> 50 Hz)
    static int32_t s_acc_ax = 0, s_acc_ay = 0, s_acc_az = 0;
    static int32_t s_acc_gx = 0, s_acc_gy = 0, s_acc_gz = 0;
    static int s_acc_count = 0;

    while (1) {
        vTaskDelayUntil(&last, period);

        int16_t ax, ay, az, gx, gy, gz;
        if (bmi160_read_accel_gyro(&ax, &ay, &az, &gx, &gy, &gz) != ESP_OK) {
            continue;
        }

        s_acc_ax += ax;
        s_acc_ay += ay;
        s_acc_az += az;
        s_acc_gx += gx;
        s_acc_gy += gy;
        s_acc_gz += gz;
        s_acc_count++;

        if (s_acc_count >= 2) {
            int16_t avg_ax = s_acc_ax / 2;
            int16_t avg_ay = s_acc_ay / 2;
            int16_t avg_az = s_acc_az / 2;
            int16_t avg_gx = s_acc_gx / 2;
            int16_t avg_gy = s_acc_gy / 2;
            int16_t avg_gz = s_acc_gz / 2;

            s_acc_ax = 0; s_acc_ay = 0; s_acc_az = 0;
            s_acc_gx = 0; s_acc_gy = 0; s_acc_gz = 0;
            s_acc_count = 0;

            // Correr el algoritmo FFT para detección de pasos en frecuencia (a 50 Hz)
            uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
            uint8_t steps_detected = step_algo_update(&s_step_state, avg_ax, avg_ay, avg_az, avg_gx, avg_gy, avg_gz, now_ms, true);
            if (steps_detected > 0) {
                s_pedometer_steps += steps_detected;
            }

            // Push al ring buffer de 6 ejes
            s_ring[s_write_idx][0] = avg_ax;
            s_ring[s_write_idx][1] = avg_ay;
            s_ring[s_write_idx][2] = avg_az;
            s_ring[s_write_idx][3] = avg_gx;
            s_ring[s_write_idx][4] = avg_gy;
            s_ring[s_write_idx][5] = avg_gz;
            s_write_idx = (s_write_idx + 1) % HAR_WINDOW_SIZE;
            s_samples_since_inference++;

            static uint32_t s_total_samples = 0;
            if (s_total_samples < HAR_WINDOW_SIZE) {
                s_total_samples++;
            }

            // Enviar lectura cruda filtrada de 6 ejes (50 Hz) a la app móvil (UUID 0xFF01)
            int16_t imu_raw[6] = { avg_ax, avg_ay, avg_az, avg_gx, avg_gy, avg_gz };
            ble_telemetry_send_imu(imu_raw, sizeof(imu_raw));

            if (s_samples_since_inference >= HAR_HOP_SIZE) {
                s_samples_since_inference = 0;
                
                if (s_total_samples < HAR_WINDOW_SIZE) {
                    uint8_t state_val = 0;
                    ble_tx_push(BLE_TLV_TYPE_HAR_STATE, &state_val, 1, 0xFF);
                } else {
                    if (!s_inference_pending && s_inference_task_handle != NULL) {
                        s_inference_pending = true;

                        // Desenrollar ring buffer de forma ordenada cronológicamente
                        for (int i = 0; i < HAR_WINDOW_SIZE; ++i) {
                            int idx = (s_write_idx + i) % HAR_WINDOW_SIZE;
                            memcpy(s_inference_window[i], s_ring[idx], sizeof(s_ring[idx]));
                        }

                        xTaskNotifyGive(s_inference_task_handle);
                    }
                }
            }
        }
    }
}

static void steps_task(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        uint32_t total_steps = s_pedometer_steps;

        // Enviar cuenta de pasos a BLE (TLV tipo 0x05, 4 bytes, uint32_t little-endian)
        uint8_t buf[4];
        buf[0] = (uint8_t)(total_steps & 0xFF);
        buf[1] = (uint8_t)((total_steps >> 8) & 0xFF);
        buf[2] = (uint8_t)((total_steps >> 16) & 0xFF);
        buf[3] = (uint8_t)((total_steps >> 24) & 0xFF);

        ble_tx_push(BLE_TLV_TYPE_STEPS, buf, 4, 0xFF);
        
        // Forzar flush asíncrono para enviar todos los datos acumulados
        ble_tx_flush(0); // 0 = normal power mode
    }
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_LOGI(TAG, "=== Entorno de Validacion Realtime HAR C3 ===");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_log_level_set("NimBLE",     ESP_LOG_WARN);
    esp_log_level_set("NimBLE_GAP", ESP_LOG_WARN);
    esp_log_level_set("BLE_GAP",    ESP_LOG_WARN);
    esp_log_level_set("BLE_GATT",   ESP_LOG_WARN);
    esp_log_level_set("BTDM_INIT",  ESP_LOG_WARN);
    esp_log_level_set("phy_init",   ESP_LOG_WARN);

    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C init fallo - abortando");
        return;
    }
    if (bmi160_init() != ESP_OK) {
        ESP_LOGE(TAG, "BMI160 init fallo");
        return;
    }

    if (ble_telemetry_init() != ESP_OK) {
        ESP_LOGE(TAG, "BLE stack init fallo");
        return;
    }
    ESP_LOGI(TAG, "BLE inicializado. Buscando conexion de app Flutter...");

    // Inicializar el algoritmo de podómetro FFT
    step_algo_init(&s_step_state);

    // Inicializar TFLite (enlazado de forma fuerte)
    ESP_LOGI(TAG, "Inicializando TensorFlow Lite Micro con modelo de %u bytes...", har_model_c3_tflite_len);
    ESP_LOGI(TAG, "Heap libre antes de TFLite init: %u bytes", (unsigned)esp_get_free_heap_size());
    s_tflite_ready = har_runner_init(s_arena, HAR_ARENA_BYTES, har_model_c3_tflite, har_model_c3_tflite_len, &s_arena_used);
    ESP_LOGI(TAG, "TFLite Init %s (Arena usada: %u bytes)", s_tflite_ready ? "EXITOSO" : "FALLIDO", (unsigned)s_arena_used);
    ESP_LOGI(TAG, "Heap libre después de TFLite init: %u bytes", (unsigned)esp_get_free_heap_size());

    // Lanzamos la tarea de inferencia de ML asíncrona (prioridad 3, menor que la de muestreo)
    xTaskCreate(har_inference_task, "har_inference_task", 4096, NULL, 3, &s_inference_task_handle);

    // Lanzamos la tarea de procesamiento HAR (aumentamos el stack a 6144 para acomodar la FFT)
    xTaskCreate(har_c3_task, "har_c3_task", 6144, NULL, 4, NULL);

    // Lanzamos la tarea de lectura del contador de pasos
    xTaskCreate(steps_task, "steps_task", 2048, NULL, 5, NULL);
}

#endif

/**
 * @file har_model_runner.cpp
 * @brief Adaptador entre el ring buffer del HAR (C) y el intérprete TFLite-Micro
 *        (C++). Implementa el modelo dynamic-range del S3 (float I/O) generado
 *        por `tools/train_har_cnn.py`.
 *
 * Interfaz declarada débil en `har_cnn1d.c`:
 *   - `har_runner_init(arena, arena_bytes, model_blob, model_len, arena_used)`
 *   - `har_runner_run(window[200][6], probs[4])`
 *
 * Compatibilidad de tipos de I/O:
 *   - Modelo dynamic-range (`Optimize.DEFAULT`, weights INT8, activations FLOAT):
 *     tensor I/O es float32. Es el default de `train_har_cnn.py`.
 *   - Modelo INT8 puro (opción avanzada usada por `train_har_cnn_c3.py`): tensor
 *     I/O es int8. El runner detecta el tipo con `input->type` y aplica quant/
 *     dequant con los `scale`/`zero_point` que trae el .tflite.
 *
 * Ambas rutas están soportadas para poder swappear entre modelos sin recompilar
 * este archivo.
 */

#include "har_cnn1d.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "esp_log.h"

#include <cmath>
#include <cstdint>
#include <cstddef>

namespace {

constexpr const char *TAG = "HAR_RT";

/* Interprete + tensores cacheados tras Init. */
tflite::MicroInterpreter *s_interpreter = nullptr;
TfLiteTensor             *s_input       = nullptr;
TfLiteTensor             *s_output      = nullptr;
bool                       s_ready       = false;

/* Estado persistente del último init — se lee desde el log periódico de
 * probs en har_cnn1d.c para saber por qué el runner no arrancó cuando los
 * logs del boot se perdieron por el buffer USB CDC. */
char s_init_status[80] = "no init call";

/* Op resolver estático — solo instanciamos los ops que la arquitectura del
 * modelo requiere (`train_har_cnn.py`: Conv1D → MaxPool → GAP → Dense → Softmax).
 * Keras exporta Conv1D como Conv2D con kernel [1, k]; GAP como Mean; Dense como
 * FullyConnected. Reshape puede aparecer implícitamente en las transformaciones
 * de shape. Quantize/Dequantize aparecen si el modelo tiene INT8 puro. */
constexpr int kOpsCount = 15;
tflite::MicroMutableOpResolver<kOpsCount> *s_resolver_ptr = nullptr;

}  // namespace

extern "C" {

const char *har_runner_last_status(void) {
    return s_init_status;
}

bool har_runner_init(uint8_t *arena, size_t arena_bytes,
                     const unsigned char *model_blob,
                     size_t model_len, size_t *arena_used) {
    if (arena_used) *arena_used = 0;

    if (!arena || !model_blob || model_len == 0) {
        snprintf(s_init_status, sizeof(s_init_status),
                 "invalid args arena=%p blob=%p len=%u",
                 arena, model_blob, (unsigned)model_len);
        ESP_LOGE(TAG, "%s", s_init_status);
        return false;
    }

    const tflite::Model *model = tflite::GetModel(model_blob);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        snprintf(s_init_status, sizeof(s_init_status),
                 "schema mismatch %lu vs %u",
                 (unsigned long)model->version(), TFLITE_SCHEMA_VERSION);
        ESP_LOGE(TAG, "%s", s_init_status);
        return false;
    }

    /* Op resolver singleton — construido on first call. Cada AddX puede
     * fallar si se pasa la capacidad del template — chequeamos y logueamos. */
    static tflite::MicroMutableOpResolver<kOpsCount> resolver;
    s_resolver_ptr = &resolver;
    #define TRY_ADD(op) do { \
        TfLiteStatus s = resolver.Add##op(); \
        if (s != kTfLiteOk) { \
            snprintf(s_init_status, sizeof(s_init_status), \
                     "Add" #op " fallo (%d)", (int)s); \
            ESP_LOGE(TAG, "%s", s_init_status); \
            return false; \
        } \
    } while (0)
    TRY_ADD(Conv2D);
    TRY_ADD(MaxPool2D);
    TRY_ADD(Mean);              /* GlobalAveragePooling1D */
    TRY_ADD(FullyConnected);    /* Dense */
    TRY_ADD(Softmax);
    TRY_ADD(Reshape);           /* Keras inserta reshape entre GAP y Dense */
    TRY_ADD(ExpandDims);        /* Keras Conv1D wraps 3D→4D via ExpandDims */
    TRY_ADD(Relu);              /* activation dentro de Conv o Dense */
    TRY_ADD(Quantize);          /* solo si modelo INT8 con IO float */
    TRY_ADD(Dequantize);        /* idem */
    #undef TRY_ADD

    static tflite::MicroInterpreter interp(model, resolver, arena, arena_bytes);
    s_interpreter = &interp;

    TfLiteStatus alloc = s_interpreter->AllocateTensors();
    if (alloc != kTfLiteOk) {
        snprintf(s_init_status, sizeof(s_init_status),
                 "AllocateTensors fallo (%d) arena=%uKB",
                 (int)alloc, (unsigned)(arena_bytes / 1024));
        ESP_LOGE(TAG, "%s", s_init_status);
        return false;
    }

    s_input  = s_interpreter->input(0);
    s_output = s_interpreter->output(0);
    if (!s_input || !s_output) {
        snprintf(s_init_status, sizeof(s_init_status), "input/output nulos");
        ESP_LOGE(TAG, "%s", s_init_status);
        return false;
    }

    /* Verificación de shape: esperamos (batch=1, 200, 6) para input. Keras
     * puede exportar como (1, 200, 6) o (1, 200, 6, 1) dependiendo del Conv2D
     * shim — soportamos ambas. */
    int in_dims = s_input->dims->size;
    int in_time = (in_dims >= 2) ? s_input->dims->data[1] : 0;
    int in_ch   = (in_dims >= 3) ? s_input->dims->data[2] : 0;
    ESP_LOGI(TAG, "input dims=%d shape=[%d,%d,%d,%d] type=%d",
             in_dims,
             (in_dims > 0) ? s_input->dims->data[0] : 0,
             in_time, in_ch,
             (in_dims > 3) ? s_input->dims->data[3] : 0,
             (int)s_input->type);
    ESP_LOGI(TAG, "output shape=[%d,%d] type=%d",
             (s_output->dims->size > 0) ? s_output->dims->data[0] : 0,
             (s_output->dims->size > 1) ? s_output->dims->data[1] : 0,
             (int)s_output->type);

    if (in_time != HAR_WINDOW_SIZE || in_ch != HAR_CHANNELS) {
        snprintf(s_init_status, sizeof(s_init_status),
                 "shape mismatch got [_,%d,%d,_]", in_time, in_ch);
        ESP_LOGE(TAG, "%s", s_init_status);
        return false;
    }

    size_t used = s_interpreter->arena_used_bytes();
    if (arena_used) *arena_used = used;
    snprintf(s_init_status, sizeof(s_init_status),
             "OK arena=%uKB used=%uKB in=%d out=%d",
             (unsigned)(arena_bytes / 1024), (unsigned)(used / 1024),
             (int)s_input->type, (int)s_output->type);
    ESP_LOGI(TAG, "%s", s_init_status);

    s_ready = true;
    return true;
}

bool har_runner_run(const int16_t window[HAR_WINDOW_SIZE][HAR_CHANNELS],
                    float probs[HAR_NUM_CLASSES]) {
    if (!s_ready || !s_interpreter || !s_input || !s_output) return false;

    /* ── Copia + normalización del ring al tensor de input ── */
    if (s_input->type == kTfLiteFloat32) {
        float *dst = s_input->data.f;
        for (int i = 0; i < HAR_WINDOW_SIZE; ++i) {
            for (int c = 0; c < HAR_CHANNELS; ++c) {
                dst[i * HAR_CHANNELS + c] = (float)window[i][c] / 32768.0f;
            }
        }
    } else if (s_input->type == kTfLiteInt8) {
        const float scale = s_input->params.scale;
        const int   zp    = s_input->params.zero_point;
        int8_t *dst = s_input->data.int8;
        for (int i = 0; i < HAR_WINDOW_SIZE; ++i) {
            for (int c = 0; c < HAR_CHANNELS; ++c) {
                float v = (float)window[i][c] / 32768.0f;
                int q = (int)lrintf(v / scale) + zp;
                if (q < -128) q = -128;
                if (q >  127) q =  127;
                dst[i * HAR_CHANNELS + c] = (int8_t)q;
            }
        }
    } else {
        ESP_LOGW(TAG, "input type %d no soportado", (int)s_input->type);
        return false;
    }

    /* ── Invoke ── */
    if (s_interpreter->Invoke() != kTfLiteOk) {
        return false;
    }

    /* ── Salida a probs float ── */
    if (s_output->type == kTfLiteFloat32) {
        const float *src = s_output->data.f;
        for (int k = 0; k < HAR_NUM_CLASSES; ++k) probs[k] = src[k];
    } else if (s_output->type == kTfLiteInt8) {
        const float scale = s_output->params.scale;
        const int   zp    = s_output->params.zero_point;
        const int8_t *src = s_output->data.int8;
        for (int k = 0; k < HAR_NUM_CLASSES; ++k) {
            probs[k] = ((int)src[k] - zp) * scale;
        }
    } else {
        return false;
    }

    return true;
}

}  // extern "C"

/**
 * @file har_model_runner.cpp
 * @brief Adaptador delgado entre el ring buffer del HAR (C) y el intérprete
 *        TFLite-Micro (C++).
 *
 * Lo dejamos como "stub" hasta que se añada el componente
 * `esp-tflite-micro` (managed component / IDF component manager).
 *
 * Cuando lo agregues:
 *   idf.py add-dependency "espressif/esp-tflite-micro"
 * y descomenta los `#include` de TensorFlow Lite Micro.
 *
 * El modelo (`har_model.cc`, generado con `xxd -i har_model.tflite > har_model.cc`)
 * debe colocarse junto a este archivo. Quedará linkeado como blob extern.
 */

#include "har_cnn1d.h"
#include <stdint.h>
#include <stddef.h>

// #include "tensorflow/lite/micro/micro_interpreter.h"
// #include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
// #include "tensorflow/lite/schema/schema_generated.h"

extern "C" {

/* Devuelve true cuando el modelo se cargó y el intérprete quedó listo.
 * Mientras no se enganche TFLite-Micro, devolvemos false → el módulo HAR
 * cae al fallback heurístico (varianza). */
bool har_runner_init(uint8_t *arena, size_t arena_bytes,
                     const unsigned char *model_blob,
                     size_t model_len, size_t *arena_used) {
    (void)arena; (void)arena_bytes;
    (void)model_blob; (void)model_len;
    if (arena_used) *arena_used = 0;
    return false;
}

/* Cuando el intérprete esté disponible, esta función:
 *   1. Copia/quant. la ventana int16 a tensor INT8 (zero_point, scale).
 *   2. interpreter->Invoke()
 *   3. Lee logits, aplica softmax → probs.
 *
 * Mientras tanto NO se define (no es weak-resolved en este TU): har_cnn1d.c
 * marca el extern como weak, así que si no la implementas, queda a null y
 * el HAR usa la heurística. */
// bool har_runner_run(const int16_t window[HAR_WINDOW_SIZE][HAR_CHANNELS],
//                     float probs[HAR_NUM_CLASSES]) {
//     ...
// }

}  // extern "C"

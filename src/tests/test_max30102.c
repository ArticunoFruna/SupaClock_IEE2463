#ifdef ENV_TEST_SPO2

/**
 * @file test_max30102.c
 * @brief Captura cruda red/ir del MAX30102 → BLE para análisis offline.
 *
 * Objetivo: alimentar la comparativa "antes vs después del bandpass" en la
 * presentación. Reusa el stack BLE de SupaClock pero envía las muestras
 * SIN procesar (solo lectura del FIFO + push BLE).
 *
 * Formato del paquete BLE (característica 0xFF03 = ECG, reusada como
 * canal de streaming genérico para evitar agregar un GATT nuevo):
 *   N muestras × 8 bytes [red_u32_le, ir_u32_le]
 * con N variable según cuántas haya en la FIFO al momento del polling.
 *
 * El supaclock_raw_ppg.py (en tools/) se conecta a esta característica
 * y graba el CSV con las dos columnas crudas. El offline lo procesa con
 * un bandpass IIR replicado en numpy y se generan los plots.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "i2c_bus.h"
#include "max30102.h"
#include "ble_telemetry.h"

static const char *TAG = "Test_SpO2_RAW";

#define POLL_MS         100      /* mismo que test_general */
#define SAMPLES_PER_PKT 5        /* hasta 5 × 8 B = 40 B por notify */

void raw_ppg_task(void *pv) {
    max30102_sample_t buf[32];
    uint8_t out[SAMPLES_PER_PKT * 8];

    /* Flush para descartar muestras viejas del arranque */
    max30102_flush_fifo();

    while (1) {
        uint8_t n = 0;
        if (max30102_read_samples(buf, 32, &n) == ESP_OK && n > 0) {
            /* Empacar de a SAMPLES_PER_PKT y notificar */
            uint8_t i = 0;
            while (i < n) {
                uint8_t batch = (n - i) > SAMPLES_PER_PKT ? SAMPLES_PER_PKT : (n - i);
                for (uint8_t k = 0; k < batch; k++) {
                    uint32_t r = buf[i + k].red;
                    uint32_t ir = buf[i + k].ir;
                    memcpy(&out[k * 8 + 0], &r,  4);
                    memcpy(&out[k * 8 + 4], &ir, 4);
                }
                /* Reusamos el canal ECG (0xFF03) como streaming genérico.
                 * El cliente diferencia por longitud y/o por env. */
                ble_telemetry_send_ecg((int16_t *)out, batch * 8);
                i += batch;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== TEST RAW PPG (MAX30102) ===");

    /* Bajar logs BLE, son muy ruidosos */
    esp_log_level_set("NimBLE",     ESP_LOG_WARN);
    esp_log_level_set("NimBLE_GAP", ESP_LOG_WARN);
    esp_log_level_set("BLE_GAP",    ESP_LOG_WARN);
    esp_log_level_set("BLE_GATT",   ESP_LOG_WARN);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C init falló");
        return;
    }
    if (max30102_init_hrm() != ESP_OK) {
        ESP_LOGE(TAG, "MAX30102 ausente");
        return;
    }

    if (ble_telemetry_init() != ESP_OK) {
        ESP_LOGE(TAG, "BLE Stack falló");
        return;
    }

    /* En este test el ECG mode siempre está activo para que send_ecg salga */
    ble_telemetry_set_ecg_mode(true);

    xTaskCreate(raw_ppg_task, "raw_ppg", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Streaming raw red/ir por 0xFF03 cada %d ms", POLL_MS);
}

#endif

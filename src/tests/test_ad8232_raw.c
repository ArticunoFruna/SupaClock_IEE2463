#ifdef ENV_TEST_ECG_RAW

/**
 * @file test_ad8232_raw.c
 * @brief Captura cruda del AD8232 (ADC1 continuo) → BLE para diagnosticar
 *        si los artefactos vistos en la traza de producción son SW o HW.
 *
 * Diferencias vs. el pipeline de test_general:
 *   - NO se configura esp_pm (light sleep dinámico). El sistema queda a
 *     full clock todo el tiempo → descarta los glitches por reconfig de
 *     APB durante el DMA.
 *   - NO hay promedio (sum/40) — sólo decimación 1-de-40 → si el artefacto
 *     viene del filtro boxcar, aquí no aparece.
 *   - Sólo se inicializa AD8232 + BLE; nada más se enciende (ni I2C, ni
 *     display, ni IMU), para minimizar otras fuentes de ruido.
 *
 * Mismo rate de salida (500 Hz) y misma característica BLE (0xFF03) que
 * el pipeline normal, para que el supaclock_monitor.py grabe el CSV sin
 * cambios y se puedan superponer ambas trazas.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_adc/adc_continuous.h"
#include "ad8232.h"
#include "ble_telemetry.h"

static const char *TAG = "Test_ECG_RAW";

/* Decimación pura: tomamos 1 de cada 40 muestras a 20 kHz → 500 Hz. */
#define DECIMATION         (AD8232_HW_SAMPLE_FREQ_HZ / AD8232_TARGET_FREQ_HZ)
#define BLE_CHUNK_SAMPLES  10   /* 10 × int16 = 20 B por notify, igual que producción */

void ecg_raw_task(void *pv) {
    uint8_t  dma_buf[AD8232_READ_LEN];
    uint32_t ret_num = 0;
    int16_t  ble_chunk[BLE_CHUNK_SAMPLES];
    int      chunk_idx = 0;
    int      dec_count = 0;
    bool     dma_running = false;

    while (1) {
        if (!ble_telemetry_is_ecg_mode_active()) {
            if (dma_running) { ad8232_stop_dma(); dma_running = false; }
            vTaskDelay(pdMS_TO_TICKS(500));
            chunk_idx = 0; dec_count = 0;
            continue;
        }
        if (!dma_running) {
            if (ad8232_start_dma() != ESP_OK) {
                ESP_LOGE(TAG, "ad8232_start_dma falló");
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            dma_running = true;
        }

        esp_err_t ret = adc_continuous_read(ad8232_get_adc_handle(), dma_buf,
                                            AD8232_READ_LEN, &ret_num,
                                            pdMS_TO_TICKS(100));
        if (ret != ESP_OK) continue;

        for (uint32_t i = 0; i < ret_num; i += sizeof(adc_digi_output_data_t)) {
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&dma_buf[i];
            /* Decimación pura: descartar 39 de cada 40, sin promediar. */
            if (++dec_count < DECIMATION) continue;
            dec_count = 0;

            ble_chunk[chunk_idx++] = (int16_t)p->type2.data;
            if (chunk_idx >= BLE_CHUNK_SAMPLES) {
                ble_telemetry_send_ecg(ble_chunk, sizeof(ble_chunk));
                chunk_idx = 0;
            }
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== TEST RAW ECG (AD8232 sin filtro, sin PM) ===");

    esp_log_level_set("NimBLE",     ESP_LOG_WARN);
    esp_log_level_set("NimBLE_GAP", ESP_LOG_WARN);
    esp_log_level_set("BLE_GAP",    ESP_LOG_WARN);
    esp_log_level_set("BLE_GATT",   ESP_LOG_WARN);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    if (ad8232_init_dma() != ESP_OK) {
        ESP_LOGE(TAG, "AD8232 init falló");
        return;
    }

    if (ble_telemetry_init() != ESP_OK) {
        ESP_LOGE(TAG, "BLE Stack falló");
        return;
    }

    /* Forzar modo ECG siempre activo: no hay UI ni botones que lo encienden. */
    ble_telemetry_set_ecg_mode(true);

    xTaskCreate(ecg_raw_task, "ecg_raw", 4096, NULL, 7, NULL);

    ESP_LOGI(TAG, "Streaming raw ECG por 0xFF03 a %d Hz (decim %d sin promedio)",
             AD8232_TARGET_FREQ_HZ, DECIMATION);
}

#endif

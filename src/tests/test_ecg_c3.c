#ifdef ENV_TEST_ECG_C3

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_adc/adc_continuous.h"
#include "ad8232.h"
#include "ble_telemetry.h"

static const char *TAG = "Test_ECG_C3";

/* Decimación pura a la frecuencia objetivo. Ej: 20 kHz a 500 Hz. */
#define DECIMATION         (AD8232_HW_SAMPLE_FREQ_HZ / AD8232_TARGET_FREQ_HZ)
#define BLE_CHUNK_SAMPLES  10   /* 10 × int16 = 20 B por notify, igual que producción */

static TaskHandle_t s_task_handle = NULL;
static uint8_t result_buf[AD8232_READ_LEN] = {0};

void ecg_c3_task(void *pv) {
    ESP_ERROR_CHECK(ad8232_start_dma());

    adc_continuous_handle_t adc_handle = ad8232_get_adc_handle();

    uint32_t ret_num = 0;
    int16_t  ble_chunk[BLE_CHUNK_SAMPLES];
    int      chunk_idx = 0;
    
    uint32_t sum = 0;
    int      sample_count = 0;

    while (1) {
        if (!ble_telemetry_is_ecg_mode_active()) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        esp_err_t ret = adc_continuous_read(adc_handle, result_buf,
                                            AD8232_READ_LEN, &ret_num, pdMS_TO_TICKS(100));
        if (ret == ESP_OK) {
            // Parseamos independientemente de si es ESP32-C3 o ESP-S3 (Type 1 o 2)
            static adc_continuous_data_t parsed_data[AD8232_READ_LEN / SOC_ADC_DIGI_RESULT_BYTES];
            uint32_t num_parsed_samples = 0;

            esp_err_t parse_ret = adc_continuous_parse_data(
                adc_handle, result_buf, ret_num, parsed_data, &num_parsed_samples);

            if (parse_ret == ESP_OK) {
                for (int i = 0; i < num_parsed_samples; i++) {
                    if (parsed_data[i].valid) {
                        sum += parsed_data[i].raw_data;
                        sample_count++;
                        
                        // Aplicar decimación 
                        if (sample_count >= DECIMATION) {
                            int16_t averaged_val = (int16_t)(sum / DECIMATION);
                            ble_chunk[chunk_idx++] = averaged_val;

                            if (chunk_idx >= BLE_CHUNK_SAMPLES) {
                                ble_telemetry_send_ecg(ble_chunk, sizeof(ble_chunk));
                                chunk_idx = 0;
                            }

                            sum = 0;
                            sample_count = 0;
                        }
                    }
                }
            }
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== TEST ECG C3 SUPERMINI BLE ===");
    ESP_LOGI(TAG, "Wiring: AD8232 OUT -> GPIO0 (ADC1_CH0)");
    ESP_LOGI(TAG, "Wiring: AD8232 SDN -> 3.3V (VCC)");

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
        ESP_LOGE(TAG, "BLE init falló");
        return;
    }

    /* Forzar modo ECG siempre activo y omitir el agregado de spot checks. */
    ble_telemetry_set_ecg_mode(true);

    xTaskCreate(ecg_c3_task, "ecg_c3", 8192, NULL, 7, NULL);

    ESP_LOGI(TAG, "Streaming ECG C3 por 0xFF03 a %d Hz", AD8232_TARGET_FREQ_HZ);
}

#endif

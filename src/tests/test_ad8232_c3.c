#ifdef ENV_TEST_ECG_C3

#include "ad8232_c3.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "Test_AD8232_C3";
static TaskHandle_t s_task_handle = NULL;

// Buffer para leer datos crudos del DMA
static uint8_t result_buf[AD8232_C3_READ_LEN] = {0};

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle,
                                     const adc_continuous_evt_data_t *edata,
                                     void *user_data) {
  BaseType_t mustYield = pdFALSE;
  vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
  return (mustYield == pdTRUE);
}

void ecg_c3_task(void *pvParameters) {
  ESP_LOGI(TAG, "Iniciando muestreo ECG AD8232 (salida serial USB)...");

  s_task_handle = xTaskGetCurrentTaskHandle();

  if (ad8232_c3_init_dma() != ESP_OK) {
    ESP_LOGE(TAG, "Error inicializando AD8232 C3 DMA");
    vTaskDelete(NULL);
    return;
  }

  adc_continuous_handle_t adc_handle = ad8232_c3_get_adc_handle();

  // Registrar callback de conversión completa
  adc_continuous_evt_cbs_t cbs = {
      .on_conv_done = s_conv_done_cb,
  };
  ESP_ERROR_CHECK(
      adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));

  // Arrancar el muestreo continuo
  ESP_ERROR_CHECK(ad8232_c3_start_dma());
  ESP_LOGI(TAG, "DMA iniciado: %d Hz hardware -> %d Hz salida",
           AD8232_C3_HW_SAMPLE_FREQ_HZ, AD8232_C3_TARGET_FREQ_HZ);

  uint32_t ret_num = 0;

  // Decimación: promedio tipo boxcar
  const int DECIMATION_FACTOR =
      AD8232_C3_HW_SAMPLE_FREQ_HZ / AD8232_C3_TARGET_FREQ_HZ; // 40
  uint32_t sum = 0;
  int sample_count = 0;
  uint32_t output_count = 0;

  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    while (1) {
      esp_err_t ret = adc_continuous_read(adc_handle, result_buf,
                                          AD8232_C3_READ_LEN, &ret_num, 0);
      if (ret == ESP_OK) {
        adc_continuous_data_t parsed_data[ret_num / SOC_ADC_DIGI_RESULT_BYTES];
        uint32_t num_parsed_samples = 0;

        esp_err_t parse_ret = adc_continuous_parse_data(
            adc_handle, result_buf, ret_num, parsed_data, &num_parsed_samples);

        if (parse_ret == ESP_OK) {
          for (int i = 0; i < num_parsed_samples; i++) {
            if (parsed_data[i].channel == AD8232_C3_ADC_CHANNEL) {
              sum += parsed_data[i].raw_data;
              sample_count++;

              if (sample_count >= DECIMATION_FACTOR) {
                uint32_t averaged_val = sum / DECIMATION_FACTOR;
                bool leads_off = ad8232_c3_is_leads_off();

                // Imprimir por serial USB: valor numérico simple
                if (leads_off) {
                  printf("ECG:%lu,LEADS:OFF\n", (unsigned long)averaged_val);
                } else {
                  printf("ECG:%lu\n", (unsigned long)averaged_val);
                }

                output_count++;

                // Cada 500 muestras (~1 segundo), imprimir estadística
                if (output_count % 500 == 0) {
                  ESP_LOGI(TAG, "Muestras enviadas: %lu (ultimo valor: %lu)",
                           (unsigned long)output_count,
                           (unsigned long)averaged_val);
                }

                sum = 0;
                sample_count = 0;
              }
            }
          }
        }
      } else if (ret == ESP_ERR_TIMEOUT) {
        break;
      }
    }
  }
}

void app_main(void) {
  // Delay para que el USB-JTAG se estabilice tras reset
  vTaskDelay(pdMS_TO_TICKS(2000));

  ESP_LOGI(TAG, "=== TEST ECG C3 — Serial USB (sin BLE) ===");
  ESP_LOGI(TAG, "Formato de salida: ECG:<valor_adc>");
  ESP_LOGI(TAG, "Frecuencia de muestreo: %d Hz", AD8232_C3_TARGET_FREQ_HZ);

  xTaskCreate(ecg_c3_task, "EcgC3Task", 4096, NULL, configMAX_PRIORITIES - 2,
              NULL);
}

#endif // ENV_TEST_ECG_C3

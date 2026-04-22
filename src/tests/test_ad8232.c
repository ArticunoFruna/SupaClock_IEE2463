#ifdef ENV_TEST_ECG

#include "ad8232.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "Test_AD8232";
static TaskHandle_t s_task_handle = NULL;

// Buffer para leer datos crudos del DMA
static uint8_t result_buf[AD8232_READ_LEN] = {0};

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle,
                                     const adc_continuous_evt_data_t *edata,
                                     void *user_data) {
  BaseType_t mustYield = pdFALSE;
  // Notificar a la tarea principal que hay datos listos
  vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
  return (mustYield == pdTRUE);
}

void ecg_task(void *pvParameters) {
  ESP_LOGI(TAG, "Init Circuito Front-end ECG AD8232...");

  s_task_handle = xTaskGetCurrentTaskHandle();

  if (ad8232_init_dma() != ESP_OK) {
    ESP_LOGE(TAG, "Error inicializando AD8232 DMA");
    vTaskDelete(NULL);
    return;
  }

  adc_continuous_handle_t adc_handle = ad8232_get_adc_handle();

  // Registrar callbacks
  adc_continuous_evt_cbs_t cbs = {
      .on_conv_done = s_conv_done_cb,
  };
  ESP_ERROR_CHECK(
      adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));

  // Arrancar el muestreo continuo
  ESP_ERROR_CHECK(ad8232_start_dma());
  ESP_LOGI(TAG, "Muestreo DMA iniciado. Reduciendo %d Hz a %d Hz...",
           AD8232_HW_SAMPLE_FREQ_HZ, AD8232_TARGET_FREQ_HZ);

  uint32_t ret_num = 0;

  // Decimation logic
  const int DECIMATION_FACTOR =
      AD8232_HW_SAMPLE_FREQ_HZ / AD8232_TARGET_FREQ_HZ; // 20000 / 500 = 40
  uint32_t sum = 0;
  int sample_count = 0;

  while (1) {
    // Esperar a que el DMA llene un bloque (AD8232_READ_LEN bytes)
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    while (1) {
      esp_err_t ret = adc_continuous_read(adc_handle, result_buf,
                                          AD8232_READ_LEN, &ret_num, 0);
      if (ret == ESP_OK) {
        // Parse the data
        adc_continuous_data_t parsed_data[ret_num / SOC_ADC_DIGI_RESULT_BYTES];
        uint32_t num_parsed_samples = 0;

        esp_err_t parse_ret = adc_continuous_parse_data(
            adc_handle, result_buf, ret_num, parsed_data, &num_parsed_samples);
        if (parse_ret == ESP_OK) {
          for (int i = 0; i < num_parsed_samples; i++) {
            if (parsed_data[i].valid) {
              sum += parsed_data[i].raw_data;
              sample_count++;

              // Cuando llegamos al factor de decimación, tenemos nuestra
              // muestra filtrada
              if (sample_count >= DECIMATION_FACTOR) {
                uint32_t averaged_val = sum / DECIMATION_FACTOR;

                // Opcional: Revisar si los electrodos están desconectados
                bool leads_off = ad8232_is_leads_off();

                if (leads_off) {
                  // ESP_LOGW(TAG, "LEADS OFF! Conecta los electrodos.");
                } else {
                  // Print the filtered ECG point (para Serial Plotter, por
                  // ejemplo) printf(">ECG:%lu\n", averaged_val);

                  // Dummy log para no saturar la consola si no usamos Serial
                  // Plotter En producción aquí se enviaría a una cola o se
                  // actualizaría la interfaz
                }

                sum = 0;
                sample_count = 0;
              }
            }
          }
        }
      } else if (ret == ESP_ERR_TIMEOUT) {
        // No hay más datos en el buffer interno del DMA por ahora
        break;
      }
    }
  }
}

void app_main(void) {
  ESP_LOGI(TAG, "Test Módulo ADC/ECG (AD8232)");
  // Prioridad alta para no perder datos de DMA
  xTaskCreate(ecg_task, "EcgTask", 4096, NULL, configMAX_PRIORITIES - 2, NULL);
}

#endif

#include "ad8232.h"
#include "driver/gpio.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"

static const char *TAG = "AD8232";
static adc_continuous_handle_t adc_handle = NULL;

esp_err_t ad8232_init_dma(void) {
  esp_err_t ret;

  // 1. Inicializar pines GPIO si están definidos
  if (AD8232_SDN_PIN >= 0) {
    gpio_config_t sdn_conf = {
        .pin_bit_mask = (1ULL << AD8232_SDN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&sdn_conf);
    ad8232_power_up(); // Encender por defecto
  }

  if (AD8232_LO_PLUS_PIN >= 0 || AD8232_LO_MINUS_PIN >= 0) {
    uint64_t lo_mask = 0;
    int pin_plus = AD8232_LO_PLUS_PIN;
    int pin_minus = AD8232_LO_MINUS_PIN;
    if (pin_plus >= 0)
      lo_mask |= (1ULL << pin_plus);
    if (pin_minus >= 0)
      lo_mask |= (1ULL << pin_minus);

    gpio_config_t lo_conf = {
        .pin_bit_mask = lo_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&lo_conf);
  }

  // 2. Inicializar ADC en modo continuo (DMA)
  adc_continuous_handle_cfg_t adc_config = {
      .max_store_buf_size = 2048,
      .conv_frame_size = AD8232_READ_LEN,
  };
  ret = adc_continuous_new_handle(&adc_config, &adc_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error al crear handle ADC continuo: %s",
             esp_err_to_name(ret));
    return ret;
  }

  adc_continuous_config_t dig_cfg = {
      .sample_freq_hz = AD8232_HW_SAMPLE_FREQ_HZ,
      .conv_mode = ADC_CONV_SINGLE_UNIT_1,
      .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
  };

  adc_digi_pattern_config_t adc_pattern[1] = {0};
  dig_cfg.pattern_num = 1;
  adc_pattern[0].atten = ADC_ATTEN_DB_12; // 0 - 2.5V (o 3.3V) aprox
  adc_pattern[0].channel = AD8232_ADC_CHANNEL;
  adc_pattern[0].unit = AD8232_ADC_UNIT;
  adc_pattern[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

  dig_cfg.adc_pattern = adc_pattern;
  ret = adc_continuous_config(adc_handle, &dig_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error configurando ADC continuo: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "AD8232 inicializado con ADC continuo (DMA) a %d Hz",
           AD8232_HW_SAMPLE_FREQ_HZ);
  return ESP_OK;
}

esp_err_t ad8232_start_dma(void) {
  if (adc_handle == NULL)
    return ESP_ERR_INVALID_STATE;
  return adc_continuous_start(adc_handle);
}

esp_err_t ad8232_stop_dma(void) {
  if (adc_handle == NULL)
    return ESP_ERR_INVALID_STATE;
  return adc_continuous_stop(adc_handle);
}

void ad8232_power_down(void) {
  if (AD8232_SDN_PIN >= 0) {
    gpio_set_level(AD8232_SDN_PIN, 0);
    ESP_LOGI(TAG, "AD8232 en Shutdown");
  }
}

void ad8232_power_up(void) {
  if (AD8232_SDN_PIN >= 0) {
    gpio_set_level(AD8232_SDN_PIN, 1);
    ESP_LOGI(TAG, "AD8232 Encendido");
  }
}

bool ad8232_is_leads_off(void) {
  bool leads_off = false;
  if (AD8232_LO_PLUS_PIN >= 0) {
    leads_off |= (gpio_get_level(AD8232_LO_PLUS_PIN) == 1);
  }
  if (AD8232_LO_MINUS_PIN >= 0) {
    leads_off |= (gpio_get_level(AD8232_LO_MINUS_PIN) == 1);
  }
  return leads_off;
}

adc_continuous_handle_t ad8232_get_adc_handle(void) { return adc_handle; }

#include "ad8232_c3.h"
#include "driver/gpio.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"

static const char *TAG = "AD8232_C3";
static adc_continuous_handle_t adc_handle = NULL;

esp_err_t ad8232_c3_init_dma(void) {
  // 1. Inicializar pin GPIO SDN (GPIO 21)
  if (AD8232_C3_SDN_PIN >= 0) {
    gpio_config_t sdn_conf = {
        .pin_bit_mask = (1ULL << AD8232_C3_SDN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&sdn_conf);
    ad8232_c3_power_up(); // Encender por defecto
  }

  // 2. Inicializar pines LO+ y LO- si están definidos
  if (AD8232_C3_LO_PLUS_PIN >= 0 || AD8232_C3_LO_MINUS_PIN >= 0) {
    uint64_t lo_mask = 0;
    int pin_plus = AD8232_C3_LO_PLUS_PIN;
    int pin_minus = AD8232_C3_LO_MINUS_PIN;
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

  ESP_LOGI(TAG, "AD8232 C3 inicializado (el handle DMA se creará al iniciar)");
  return ESP_OK;
}

esp_err_t ad8232_c3_start_dma(void) {
  if (adc_handle != NULL)
    return ESP_ERR_INVALID_STATE;

  adc_continuous_handle_cfg_t adc_config = {
      .max_store_buf_size = 4096,
      .conv_frame_size = AD8232_C3_READ_LEN,
  };
  esp_err_t ret = adc_continuous_new_handle(&adc_config, &adc_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error al crear handle ADC continuo: %s", esp_err_to_name(ret));
    return ret;
  }

  adc_continuous_config_t dig_cfg = {
      .sample_freq_hz = AD8232_C3_HW_SAMPLE_FREQ_HZ,
      .conv_mode = ADC_CONV_SINGLE_UNIT_1,
      // ESP32-C3 requiere obligatoriamente TYPE2 en modo continuo
      .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
  };

  adc_digi_pattern_config_t adc_pattern[1] = {0};
  dig_cfg.pattern_num = 1;
  
#if defined(ADC_ATTEN_DB_12)
  adc_pattern[0].atten = ADC_ATTEN_DB_12;
#else
  // Fallback si no está definido (C3 usa típicamente 11dB como atenuación máxima)
  adc_pattern[0].atten = ADC_ATTEN_DB_11;
#endif

  adc_pattern[0].channel = AD8232_C3_ADC_CHANNEL;
  adc_pattern[0].unit = AD8232_C3_ADC_UNIT;
  adc_pattern[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

  dig_cfg.adc_pattern = adc_pattern;
  ret = adc_continuous_config(adc_handle, &dig_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error configurando ADC continuo: %s", esp_err_to_name(ret));
    adc_continuous_deinit(adc_handle);
    adc_handle = NULL;
    return ret;
  }

  return adc_continuous_start(adc_handle);
}

esp_err_t ad8232_c3_stop_dma(void) {
  if (adc_handle == NULL)
    return ESP_ERR_INVALID_STATE;
  
  esp_err_t ret = adc_continuous_stop(adc_handle);
  adc_continuous_deinit(adc_handle);
  adc_handle = NULL;
  return ret;
}

void ad8232_c3_power_down(void) {
  if (AD8232_C3_SDN_PIN >= 0) {
    gpio_set_level(AD8232_C3_SDN_PIN, 0);
    ESP_LOGI(TAG, "AD8232 C3 en Shutdown");
  }
}

void ad8232_c3_power_up(void) {
  if (AD8232_C3_SDN_PIN >= 0) {
    gpio_set_level(AD8232_C3_SDN_PIN, 1);
    ESP_LOGI(TAG, "AD8232 C3 Encendido");
  }
}

bool ad8232_c3_is_leads_off(void) {
  bool leads_off = false;
  if (AD8232_C3_LO_PLUS_PIN >= 0) {
    leads_off |= (gpio_get_level(AD8232_C3_LO_PLUS_PIN) == 1);
  }
  if (AD8232_C3_LO_MINUS_PIN >= 0) {
    leads_off |= (gpio_get_level(AD8232_C3_LO_MINUS_PIN) == 1);
  }
  return leads_off;
}

adc_continuous_handle_t ad8232_c3_get_adc_handle(void) { return adc_handle; }

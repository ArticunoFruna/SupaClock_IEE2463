#include "ad8232.h"
#include "esp_adc/adc_oneshot.h"

static adc_oneshot_unit_handle_t adc1_handle;

esp_err_t ad8232_init(void) {
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_config1, &adc1_handle);
    if (err != ESP_OK) return err;

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    return adc_oneshot_config_channel(adc1_handle, AD8232_ADC_CHANNEL, &config);
}

esp_err_t ad8232_read_single(int *value) {
    return adc_oneshot_read(adc1_handle, AD8232_ADC_CHANNEL, value);
}

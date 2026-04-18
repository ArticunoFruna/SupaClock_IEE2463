#include "max30205.h"
#include "i2c_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG_MAX = "MAX30205";

esp_err_t max30205_init(void) {
    // El sensor requiere al menos 50ms para completar su Power-On Reset
    vTaskDelay(pdMS_TO_TICKS(100)); 
    
    // Configurar registro: modo continuo, formato normal
    uint8_t config = 0x00;
    esp_err_t err = i2c_write_bytes(MAX30205_I2C_ADDR, MAX30205_REG_CONFIGURATION, &config, 1);
    
    if (err == ESP_OK) {  
        ESP_LOGI(TAG_MAX, "MAX30205 inicializado correctamente en el bus I2C.");
    } else {
        ESP_LOGE(TAG_MAX, "Error al inicializar el MAX30205: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t max30205_read_temperature(float *temp_c) {
    uint8_t data[2];
    
    // Leer 2 bytes del registro de temperatura 
    esp_err_t err = i2c_read_bytes(MAX30205_I2C_ADDR, MAX30205_REG_TEMPERATURE, data, 2);
    
    if (err == ESP_OK) {
        // Dado que la señal está siendo recibida en modo extendido, formamos la lectura de temperatura
        // Combinar los bytes para formar el entero de 16 bits en complemento a dos 
        int16_t raw_temp = (data[0] << 8) | data[1];
        
        // --- PARCHE DE HARDWARE PARA FORMATO EXTENDIDO ---
        // Como el sensor ignora el bit D5 y arroja formato extendido, 
        // multiplicamos por el LSB (0.00390625°C) y sumamos 64°C 
        *temp_c = (raw_temp * MAX30205_TEMP_RESOLUTION) + 64.0f; 
        
        // Descomenta la siguiente línea si necesitas debuggear los valores crudos
        // ESP_LOGD(TAG_MAX, "Raw: [0x%02X, 0x%02X] -> Temp: %.2f °C", data[0], data[1], *temp_c);
    } else {
        ESP_LOGE(TAG_MAX, "Error leyendo la temperatura: %s", esp_err_to_name(err));
    }
    
    return err;
}
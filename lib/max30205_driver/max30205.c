#include "max30205.h"
#include "i2c_bus.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG_MAX = "MAX30205";

esp_err_t max30205_init(void) {
    // El sensor requiere al menos 50ms para completar su Power-On Reset [cite: 57]
    vTaskDelay(pdMS_TO_TICKS(100)); 
    
    uint8_t data[2];
    esp_err_t err = ESP_OK;

    // Aunque este módulo ignora el comando, es buena práctica enviar 
    // la secuencia completa de escritura (Registro + Dato) [cite: 371]
    data[0] = MAX30205_REG_CONFIGURATION;
    data[1] = 0x00; // Intento de forzar formato normal
    
    err = i2c_master_write_to_device(I2C_MASTER_NUM, MAX30205_I2C_ADDR, data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG_MAX, "MAX30205 inicializado correctamente en el bus I2C.");
    } else {
        ESP_LOGE(TAG_MAX, "Error al inicializar el MAX30205: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t max30205_read_temperature(float *temp_c) {
    uint8_t reg = MAX30205_REG_TEMPERATURE;
    uint8_t data[2];
    
    // Leer 2 bytes del registro de temperatura [cite: 437]
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, MAX30205_I2C_ADDR, &reg, 1, data, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    
    if (err == ESP_OK) {
        // Combinar los bytes para formar el entero de 16 bits en complemento a dos [cite: 437-438]
        int16_t raw_temp = (data[0] << 8) | data[1];
        
        // --- PARCHE DE HARDWARE PARA FORMATO EXTENDIDO ---
        // Como el sensor ignora el bit D5 y arroja formato extendido, 
        // multiplicamos por el LSB (0.00390625°C) y sumamos 64°C[cite: 521].
        *temp_c = (raw_temp * MAX30205_TEMP_RESOLUTION) + 64.0f; 
        
        // Descomenta la siguiente línea si necesitas debuggear los valores crudos
        // ESP_LOGD(TAG_MAX, "Raw: [0x%02X, 0x%02X] -> Temp: %.2f °C", data[0], data[1], *temp_c);
    } else {
        ESP_LOGE(TAG_MAX, "Error leyendo la temperatura: %s", esp_err_to_name(err));
    }
    
    return err;
}
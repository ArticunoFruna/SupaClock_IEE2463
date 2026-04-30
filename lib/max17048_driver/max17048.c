#include "max17048.h"
#include "i2c_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "MAX17048";

esp_err_t max17048_check_por(bool *por_detected) {
    if (!por_detected) return ESP_ERR_INVALID_ARG;
    uint8_t data[2] = {0};
    esp_err_t err = i2c_read_bytes(MAX17048_I2C_ADDR, MAX17048_REG_STATUS, data, 2);
    if (err == ESP_OK) {
        // STATUS[0..15]: byte alto contiene RI(POR) en bit 0
        *por_detected = (data[0] & 0x01) != 0;
    }
    return err;
}

esp_err_t max17048_clear_por(void) {
    uint8_t data[2] = {0};
    esp_err_t err = i2c_read_bytes(MAX17048_I2C_ADDR, MAX17048_REG_STATUS, data, 2);
    if (err != ESP_OK) return err;
    data[0] &= ~0x01;  // limpiar bit RI/POR
    return i2c_write_bytes(MAX17048_I2C_ADDR, MAX17048_REG_STATUS, data, 2);
}

esp_err_t max17048_set_hibernate(uint8_t hib_thr, uint8_t act_thr) {
    // HIBRT (0x0A): [HibThr (8b) | ActThr (8b)]
    uint8_t data[2] = { hib_thr, act_thr };
    esp_err_t err = i2c_write_bytes(MAX17048_I2C_ADDR, MAX17048_REG_HIBRT, data, 2);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HIBRT configurado: hib_thr=0x%02X act_thr=0x%02X", hib_thr, act_thr);
    }
    return err;
}

esp_err_t max17048_quick_start(void) {
    /*
     * Quick Start: fuerza una lectura inmediata del ADC y recalcula el SOC
     * desde cero basándose únicamente en el voltaje actual.
     * Llamar sólo con la batería en reposo o tras insertar una celda nueva.
     */
    uint8_t mode_data[2] = {0x40, 0x00};
    esp_err_t err = i2c_write_bytes(MAX17048_I2C_ADDR, MAX17048_REG_MODE, mode_data, 2);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Quick Start ejecutado — SOC será recalculado");
        vTaskDelay(pdMS_TO_TICKS(200));
    } else {
        ESP_LOGW(TAG, "Quick Start falló: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t max17048_reset(void) {
    /*
     * POR (Power-On Reset) por software:
     *   - Manda 0x5400 al CMD register (0xFE).
     *   - El chip se reinicia y NACKea durante ~10 ms (no es error real).
     *   - Tras el reboot el ModelGauge queda en estado de fábrica.
     *   - Encadenamos un Quick Start para que la primera lectura de SOC
     *     parta del voltaje actual y no de un valor arbitrario.
     */
    uint8_t cmd_data[2] = { (MAX17048_CMD_POR >> 8) & 0xFF, MAX17048_CMD_POR & 0xFF };
    esp_err_t err = i2c_write_bytes(MAX17048_I2C_ADDR, MAX17048_REG_CMD, cmd_data, 2);
    // El POR puede generar NACK; lo ignoramos y esperamos.
    vTaskDelay(pdMS_TO_TICKS(15));

    // Limpiar bit POR del STATUS y arrancar de cero
    max17048_clear_por();
    max17048_quick_start();

    ESP_LOGI(TAG, "MAX17048 reseteado por software (POR + Quick Start)");
    return err;
}

esp_err_t max17048_init(void) {
    uint8_t ver_data[2] = {0};
    esp_err_t err = i2c_read_bytes(MAX17048_I2C_ADDR, MAX17048_REG_VERSION, ver_data, 2);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MAX17048 no respondió: %s", esp_err_to_name(err));
        return err;
    }
    uint16_t version = (ver_data[0] << 8) | ver_data[1];
    ESP_LOGI(TAG, "MAX17048 detectado — Version: 0x%04X", version);

    /*
     * Si POR=1 el chip arrancó desde cero y el ModelGauge no tiene historial.
     * Hacemos Quick Start para que la primera lectura parta del voltaje actual.
     * El asume que en arranque el dispositivo está razonablemente en reposo
     * (la corriente de boot es baja comparada con WiFi/BLE en operación).
     */
    bool por = false;
    if (max17048_check_por(&por) == ESP_OK && por) {
        ESP_LOGW(TAG, "POR detectado — ejecutando Quick Start");
        max17048_quick_start();
        max17048_clear_por();
    }

    /*
     * Hibernate: reduce el ruido del SOC bajo consumos pequeños.
     *   - HibThr 0x80 (~26 %/hr): entra en hibernate cuando |CRATE| < umbral
     *   - ActThr 0x30 (~60 mV):   sale de hibernate ante un cambio brusco de VCELL
     * Defaults conservadores recomendados por la app note de Maxim.
     */
    max17048_set_hibernate(0x80, 0x30);

    /*
     * VRESET: El valor por defecto (0x96) puede causar PORs falsos en transitorios.
     * Configuramos 0x4B (75) que equivale a ~3.0V (1 LSB = 40mV), o 0x3E (~2.5V).
     * Usamos 0x4B para evitar resets en consumos pico mientras la celda esté > 3.0V.
     */
    max17048_set_vreset(0x4B);

    ESP_LOGI(TAG, "MAX17048 inicializado");
    return ESP_OK;
}

esp_err_t max17048_get_voltage(uint16_t *voltage) {
    uint8_t data[2];
    esp_err_t err = i2c_read_bytes(MAX17048_I2C_ADDR, MAX17048_REG_VCELL, data, 2);
    if (err == ESP_OK) {
        uint16_t raw = (data[0] << 8) | data[1];
        // 1 LSB = 78.125 µV → raw * 5 / 64 = mV
        *voltage = (uint16_t)(((uint32_t)raw * 5) / 64);
    }
    return err;
}

esp_err_t max17048_get_soc(float *soc) {
    uint8_t data[2];
    esp_err_t err = i2c_read_bytes(MAX17048_I2C_ADDR, MAX17048_REG_SOC, data, 2);
    if (err == ESP_OK) {
        // data[0] = entero (%), data[1] = fracción (1/256 %)
        *soc = data[0] + (data[1] / 256.0f);
    }
    return err;
}

esp_err_t max17048_get_crate(float *crate) {
    uint8_t data[2];
    esp_err_t err = i2c_read_bytes(MAX17048_I2C_ADDR, MAX17048_REG_CRATE, data, 2);
    if (err == ESP_OK) {
        // CRATE register: signed 16-bit, 1 LSB = 0.208 %/hr
        int16_t raw = (int16_t)((data[0] << 8) | data[1]);
        *crate = raw * 0.208f;
    }
    return err;
}

esp_err_t max17048_set_rcomp(uint8_t rcomp_value) {
    // CONFIG register (0x0C): [RCOMP(8bit) | SLEEP|ALSC|ALRT|ATHD(8bit)]
    uint8_t data[2];
    esp_err_t err = i2c_read_bytes(MAX17048_I2C_ADDR, MAX17048_REG_CONFIG, data, 2);
    if (err != ESP_OK) return err;

    data[0] = rcomp_value;  // sólo cambiamos el byte alto
    err = i2c_write_bytes(MAX17048_I2C_ADDR, MAX17048_REG_CONFIG, data, 2);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "RCOMP actualizado a 0x%02X", rcomp_value);
    }
    return err;
}

esp_err_t max17048_set_vreset(uint8_t vreset_val) {
    // VRESET register (0x18): [VRESET(8bit) | ID(8bit)]
    // Sólo escribimos el byte alto. 
    uint8_t data[2] = {0};
    esp_err_t err = i2c_read_bytes(MAX17048_I2C_ADDR, MAX17048_REG_VRESET, data, 2);
    if (err != ESP_OK) return err;

    data[0] = vreset_val;
    err = i2c_write_bytes(MAX17048_I2C_ADDR, MAX17048_REG_VRESET, data, 2);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "VRESET actualizado a 0x%02X", vreset_val);
    }
    return err;
}

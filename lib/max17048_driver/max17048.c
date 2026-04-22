#include "max17048.h"
#include "i2c_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "MAX17048";

esp_err_t max17048_init(void) {
    /*
     * NO hacer POR ni Quick Start en init().
     * El MAX17048 mantiene su estado interno (ModelGauge) mientras tenga
     * alimentación. Resetearlo en cada boot destruye el historial de
     * carga/descarga y genera SOC imprecisos.
     *
     * Solo verificamos que el chip responda leyendo el registro VERSION.
     */
    uint8_t ver_data[2] = {0};
    esp_err_t err = i2c_read_bytes(MAX17048_I2C_ADDR, MAX17048_REG_VERSION, ver_data, 2);

    if (err == ESP_OK) {
        uint16_t version = (ver_data[0] << 8) | ver_data[1];
        ESP_LOGI(TAG, "MAX17048 detectado — Version: 0x%04X", version);
    } else {
        ESP_LOGW(TAG, "MAX17048 no respondió: %s", esp_err_to_name(err));
        return err;
    }

    /*
     * Compensación RCOMP para temperatura ambiente (~25°C).
     * El valor por defecto 0x97 está optimizado para 20°C.
     * Fórmula del datasheet: RCOMP = RCOMP0 + (Temp - 20) * TempCoUp
     * Para 25°C con TempCoUp = -0.5%/°C: ajuste mínimo, dejamos default.
     *
     * Si en el futuro se integra un sensor de temp. ambiente, se puede
     * llamar a max17048_set_rcomp() dinámicamente.
     */

    ESP_LOGI(TAG, "MAX17048 inicializado (sin reset, ModelGauge preservado)");
    return ESP_OK;
}

esp_err_t max17048_quick_start(void) {
    /*
     * Quick Start: Fuerza una lectura inmediata del ADC y recalcula el SOC
     * desde cero basándose únicamente en el voltaje actual.
     *
     * USAR SOLO cuando:
     *   - Se inserta una batería nueva/diferente
     *   - Se sabe que la batería está en reposo (sin carga)
     *   - Se quiere forzar una re-estimación (debug)
     *
     * NO usar en cada boot, ni bajo carga pesada.
     */
    uint8_t mode_data[2] = {0x40, 0x00};
    esp_err_t err = i2c_write_bytes(MAX17048_I2C_ADDR, MAX17048_REG_MODE, mode_data, 2);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Quick Start ejecutado — SOC será recalculado");
        vTaskDelay(pdMS_TO_TICKS(200)); // Esperar a que el ADC tome la medición
    } else {
        ESP_LOGW(TAG, "Quick Start falló: %s", esp_err_to_name(err));
    }
    return err;
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
    // Leemos primero para no pisar los bits bajos
    uint8_t data[2];
    esp_err_t err = i2c_read_bytes(MAX17048_I2C_ADDR, MAX17048_REG_CONFIG, data, 2);
    if (err != ESP_OK) return err;

    data[0] = rcomp_value;  // Solo cambiamos el byte alto (RCOMP)
    err = i2c_write_bytes(MAX17048_I2C_ADDR, MAX17048_REG_CONFIG, data, 2);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "RCOMP actualizado a 0x%02X", rcomp_value);
    }
    return err;
}

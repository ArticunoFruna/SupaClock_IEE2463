/**
 * @file bmi160.c
 * @brief Implementación del driver I2C para BMI160
 *
 * Basado en:
 *   - Datasheet BMI160  (BST-BMI160-DS000, Rev 1.4)
 *   - Application Note BMI160 Step Counter  (BST-BMI160-AN002)
 *   - Bosch bmi160_support.c  (referencia de secuencias)
 *
 * Secuencia de inicialización derivada de la Sección 2.11.1
 * del datasheet: Power-On procedure. Delays mínimos según Tabla 5.
 *
 * @note Este driver utiliza las funciones i2c_read_bytes / i2c_write_bytes
 *       del módulo compartido i2c_bus.h
 */
#include "bmi160.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BMI160";

/* ═══════════════════ Helpers internos ═══════════════════════ */

/**
 * @brief Escribe un solo byte a un registro del BMI160.
 */
static esp_err_t bmi160_write_reg(uint8_t reg_addr, uint8_t value)
{
    return i2c_write_bytes(BMI160_I2C_ADDR, reg_addr, &value, 1);
}

/**
 * @brief Lee un solo byte de un registro del BMI160.
 */
static esp_err_t bmi160_read_reg(uint8_t reg_addr, uint8_t *value)
{
    return i2c_read_bytes(BMI160_I2C_ADDR, reg_addr, value, 1);
}

/**
 * @brief Lee N bytes consecutivos a partir de reg_addr.
 */
static esp_err_t bmi160_read_regs(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_read_bytes(BMI160_I2C_ADDR, reg_addr, data, len);
}

/* ════════════════════ API Pública ═══════════════════════════ */

esp_err_t bmi160_init(void)
{
    esp_err_t err;
    uint8_t chip_id = 0x00;
    uint8_t pmu_status = 0x00;

    /* ── 1. Dummy read para despertar la interfaz I2C ──────────
     * Datasheet Sección 3.1: después de power-on, el primer
     * byte I2C puede ser descartado. Se recomienda un dummy read
     * al registro 0x7F para asegurar que el sensor entre al
     * modo I2C (relevante si el pin CSB flotaba).
     */
    uint8_t dummy;
    i2c_read_bytes(BMI160_I2C_ADDR, 0x7F, &dummy, 1);
    vTaskDelay(pdMS_TO_TICKS(5));

    /* ── 2. Soft-reset ─────────────────────────────────────────
     * Escribir 0xB6 al CMD register (0x7E).
     * Después del soft-reset, el sensor necesita ~80..100 ms
     * para completar el boot (Tabla 5, t_startup).
     */
    err = bmi160_write_reg(BMI160_CMD_REG, BMI160_CMD_SOFT_RESET);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Soft-reset falló: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Dummy read después del reset (misma razón que paso 1) */
    i2c_read_bytes(BMI160_I2C_ADDR, 0x7F, &dummy, 1);
    vTaskDelay(pdMS_TO_TICKS(5));

    /* ── 3. Verificar CHIP_ID ──────────────────────────────────
     * Se espera 0xD1 en el registro 0x00.
     */
    err = bmi160_read_reg(BMI160_CHIP_ID_REG, &chip_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo leer CHIP_ID: %s", esp_err_to_name(err));
        return err;
    }
    if (chip_id != BMI160_CHIP_ID_VALUE) {
        ESP_LOGE(TAG, "CHIP_ID incorrecto: 0x%02X (esperado 0x%02X)",
                 chip_id, BMI160_CHIP_ID_VALUE);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "CHIP_ID verificado: 0x%02X ✓", chip_id);

    /* ── 4. Acelerómetro → Normal mode ─────────────────────────
     * Datasheet Tabla 5: transición suspend→normal = 3.8 ms typ.
     * Usamos 30 ms para cubrir el peor caso.
     */
    err = bmi160_write_reg(BMI160_CMD_REG, BMI160_CMD_ACC_NORMAL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Accel Normal mode falló: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(30));

    /* ── 5. Giroscopio → Normal mode ───────────────────────────
     * Datasheet Tabla 5: transición suspend→normal = 80 ms typ.
     * Usamos 100 ms para cubrir el peor caso.
     */
    err = bmi160_write_reg(BMI160_CMD_REG, BMI160_CMD_GYR_NORMAL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Gyro Normal mode falló: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    /* ── 6. Configurar Acelerómetro ────────────────────────────
     * ACC_CONF (0x40) = ODR 100 Hz, BWP Normal (avg4), sin undersampling
     *   bits [3:0] = 0x08 (ODR 100 Hz)
     *   bits [6:4] = 0x02 (BWP normal = OSR4_AVG1)
     *   bit  [7]   = 0x0  (no undersampling)
     *   → 0x28
     */
    uint8_t acc_conf = 0x28;  /* 0b0010_1000 : bwp=0x2, odr=0x8 */
    err = bmi160_write_reg(BMI160_ACC_CONF_REG, acc_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ACC_CONF falló: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(2));

    /* ACC_RANGE (0x41) = ±2g (máxima resolución: 16384 LSB/g) */
    err = bmi160_write_reg(BMI160_ACC_RANGE_REG, BMI160_ACC_RANGE_2G);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ACC_RANGE falló: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(2));

    /* ── 7. Configurar Giroscopio ──────────────────────────────
     * GYR_CONF (0x42) = ODR 100 Hz, BWP Normal
     *   bits [3:0] = 0x08 (ODR 100 Hz)
     *   bits [5:4] = 0x02 (BWP normal)
     *   → 0x28
     */
    uint8_t gyr_conf = 0x28;  /* 0b0010_1000 : bwp=0x2, odr=0x8 */
    err = bmi160_write_reg(BMI160_GYR_CONF_REG, gyr_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GYR_CONF falló: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(2));

    /* GYR_RANGE (0x43) = ±2000 °/s (máxima sensibilidad: 16.4 LSB/°/s) */
    err = bmi160_write_reg(BMI160_GYR_RANGE_REG, BMI160_GYR_RANGE_2000DPS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GYR_RANGE falló: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(2));

    /* ── 8. Verificar PMU_STATUS ───────────────────────────────
     * PMU_STATUS (0x03):
     *   bits [5:4] = acc_pmu_status  → debe ser 0x01 (normal)
     *   bits [3:2] = gyr_pmu_status  → debe ser 0x01 (normal)
     */
    err = bmi160_read_reg(BMI160_PMU_STATUS_REG, &pmu_status);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo leer PMU_STATUS: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t acc_pmu = (pmu_status & BMI160_ACC_PMU_STATUS_MASK) >> BMI160_ACC_PMU_STATUS_SHIFT;
    uint8_t gyr_pmu = (pmu_status & BMI160_GYR_PMU_STATUS_MASK) >> BMI160_GYR_PMU_STATUS_SHIFT;

    ESP_LOGI(TAG, "PMU_STATUS: 0x%02X (Acc=%d, Gyr=%d)", pmu_status, acc_pmu, gyr_pmu);

    if (acc_pmu != BMI160_PMU_NORMAL) {
        ESP_LOGW(TAG, "⚠ Acelerómetro NO en modo normal (pmu=%d)", acc_pmu);
    }
    if (gyr_pmu != BMI160_PMU_NORMAL) {
        ESP_LOGW(TAG, "⚠ Giroscopio NO en modo normal (pmu=%d)", gyr_pmu);
    }

    /* ── 9. Log de error register ──────────────────────────────
     * ERR_REG (0x02): reporta errores fatales después del reset.
     * Si es != 0x00, hay un problema de hardware.
     */
    uint8_t err_reg = 0;
    bmi160_read_reg(BMI160_ERR_REG, &err_reg);
    if (err_reg != 0x00) {
        ESP_LOGW(TAG, "ERR_REG = 0x%02X (posible error de hardware)", err_reg);
    }

    ESP_LOGI(TAG, "BMI160 inicializado correctamente ✓");
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────── */

esp_err_t bmi160_read_accel(int16_t *ax, int16_t *ay, int16_t *az)
{
    if (ax == NULL || ay == NULL || az == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Burst read de 6 bytes: ACC_X_L, ACC_X_H, ACC_Y_L, ACC_Y_H, ACC_Z_L, ACC_Z_H
     * Registros 0x12..0x17 (little-endian) */
    uint8_t buf[6];
    esp_err_t err = bmi160_read_regs(BMI160_ACC_DATA_REG, buf, 6);
    if (err != ESP_OK) {
        return err;
    }

    /* Ensamblar little-endian a int16_t (complemento a dos nativo) */
    *ax = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    *ay = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    *az = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);

    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────── */

esp_err_t bmi160_read_accel_gyro(int16_t *ax, int16_t *ay, int16_t *az,
                                  int16_t *gx, int16_t *gy, int16_t *gz)
{
    if (ax == NULL || ay == NULL || az == NULL ||
        gx == NULL || gy == NULL || gz == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Burst read de 12 bytes: GYR_X_L..GYR_Z_H, ACC_X_L..ACC_Z_H
     * Registros 0x0C..0x17 (little-endian)
     *
     * Layout en memoria:
     *   [0:1]  GYR_X   [2:3]  GYR_Y   [4:5]  GYR_Z
     *   [6:7]  ACC_X   [8:9]  ACC_Y   [10:11] ACC_Z
     */
    uint8_t buf[12];
    esp_err_t err = bmi160_read_regs(BMI160_GYR_DATA_REG, buf, 12);
    if (err != ESP_OK) {
        return err;
    }

    /* Giroscopio (primeros 6 bytes) */
    *gx = (int16_t)((uint16_t)buf[1]  << 8 | buf[0]);
    *gy = (int16_t)((uint16_t)buf[3]  << 8 | buf[2]);
    *gz = (int16_t)((uint16_t)buf[5]  << 8 | buf[4]);

    /* Acelerómetro (últimos 6 bytes) */
    *ax = (int16_t)((uint16_t)buf[7]  << 8 | buf[6]);
    *ay = (int16_t)((uint16_t)buf[9]  << 8 | buf[8]);
    *az = (int16_t)((uint16_t)buf[11] << 8 | buf[10]);

    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────── */

esp_err_t bmi160_enable_step_counter(void)
{
    esp_err_t err;

    /* ── Configurar STEP_CONF_0 (0x7A) ────────────────────────
     * Modo "Normal" según la Application Note BST-BMI160-AN002:
     *   STEP_CONF_0 = 0x15
     *
     * Desglose:
     *   min_threshold [2:0]   = 0x5  (umbral mínimo para detectar paso)
     *   steptime_min [5:3]    = 0x2  (tiempo mínimo entre pasos)
     *   step_buf_en [6]       = 0x0  (buffer deshabilitado)
     *   step_cnt_en  está en STEP_CONF_1 bit 3
     *
     * Valores alternativos documentados por Bosch:
     *   Sensitive: 0x2D  (más sensible, menos robusto)
     *   Robust:    0x1D  (menos sensible, más robusto)
     */
    err = bmi160_write_reg(BMI160_STEP_CONF_0_REG, 0x15);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "STEP_CONF_0 falló: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(2));

    /* ── Configurar STEP_CONF_1 (0x7B) ────────────────────────
     * bit 3 = step_cnt_en = 1  → Habilitar el step counter
     * bits [2:0] = min_step_buf = 0x03 (validar después de 4 pasos
     *              consecutivos antes de incrementar el conteo)
     *
     * Valor = 0x08 | 0x03 = 0x0B
     */
    err = bmi160_write_reg(BMI160_STEP_CONF_1_REG, 0x0B);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "STEP_CONF_1 falló: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(2));

    ESP_LOGI(TAG, "Step counter habilitado (modo Normal, min_buf=4)");
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────── */

esp_err_t bmi160_read_step_counter(uint16_t *step_count)
{
    if (step_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Burst read de 2 bytes: STEP_CNT_0 (0x78), STEP_CNT_1 (0x79) */
    uint8_t buf[2];
    esp_err_t err = bmi160_read_regs(BMI160_STEP_CNT_0_REG, buf, 2);
    if (err != ESP_OK) {
        *step_count = 0;
        return err;
    }

    *step_count = (uint16_t)((uint16_t)buf[1] << 8 | buf[0]);
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────── */

esp_err_t bmi160_reset_step_counter(void)
{
    esp_err_t err = bmi160_write_reg(BMI160_CMD_REG, BMI160_CMD_STEP_RESET);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Step counter reset falló: %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    return err;
}

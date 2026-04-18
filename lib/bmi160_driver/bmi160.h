/**
 * @file bmi160.h
 * @brief Driver I2C para el sensor IMU Bosch BMI160
 *
 * Driver liviano para ESP32-C3 / ESP32-S3 con ESP-IDF.
 * Utiliza el bus I2C compartido de i2c_bus.h.
 *
 * Registros y secuencias extraídos del datasheet oficial:
 *   Bosch Sensortec BMI160 — Data Sheet, BST-BMI160-DS000, Rev 1.4
 *
 * @note Este driver NO usa la API monolítica de Bosch (641 KB).
 *       Implementa solo las funciones requeridas por SupaClock.
 */
#ifndef BMI160_H
#define BMI160_H

#include <stdint.h>
#include "esp_err.h"

/* ──────────────────────── I2C Address ──────────────────────── */
/** SDO/SA0 = GND → 0x68 ; SDO/SA0 = VDD → 0x69 */
#define BMI160_I2C_ADDR         0x68

/* ──────────────────── Chip identification ──────────────────── */
#define BMI160_CHIP_ID_REG      0x00
#define BMI160_CHIP_ID_VALUE    0xD1

/* ──────────────── Error & Status registers ─────────────────── */
#define BMI160_ERR_REG          0x02
#define BMI160_PMU_STATUS_REG   0x03

/* ─────────────── Data registers (little-endian) ───────────── */
/* Gyroscope data: X[0:7], X[15:8], Y, Z  →  0x0C..0x11       */
#define BMI160_GYR_DATA_REG     0x0C
/* Accelerometer data: X[0:7], X[15:8], Y, Z →  0x12..0x17    */
#define BMI160_ACC_DATA_REG     0x12

/* ──────────────── Configuration registers ─────────────────── */
#define BMI160_ACC_CONF_REG     0x40   /**< Acc ODR / BW / US   */
#define BMI160_ACC_RANGE_REG    0x41   /**< Acc full-scale range */
#define BMI160_GYR_CONF_REG     0x42   /**< Gyro ODR / BW       */
#define BMI160_GYR_RANGE_REG    0x43   /**< Gyro full-scale range */

/* ──────────── Interrupt enable / map registers ────────────── */
#define BMI160_INT_EN_0_REG     0x50
#define BMI160_INT_EN_1_REG     0x51
#define BMI160_INT_EN_2_REG     0x52
#define BMI160_INT_MAP_0_REG    0x55
#define BMI160_INT_MAP_1_REG    0x56
#define BMI160_INT_MAP_2_REG    0x57

/* ─────────────── Step counter registers ───────────────────── */
#define BMI160_STEP_CNT_0_REG   0x78   /**< Step count [7:0]    */
#define BMI160_STEP_CNT_1_REG   0x79   /**< Step count [15:8]   */
#define BMI160_STEP_CONF_0_REG  0x7A   /**< Step det/cnt config */
#define BMI160_STEP_CONF_1_REG  0x7B   /**< bit3 = step_cnt_en  */

/* ───────────────── Command register ───────────────────────── */
#define BMI160_CMD_REG          0x7E

/* ─────────────── Command values (write to CMD) ────────────── */
#define BMI160_CMD_SOFT_RESET   0xB6
#define BMI160_CMD_ACC_NORMAL   0x11   /**< Accel → Normal mode  */
#define BMI160_CMD_ACC_LP       0x12   /**< Accel → Low-Power    */
#define BMI160_CMD_ACC_SUSPEND  0x10   /**< Accel → Suspend      */
#define BMI160_CMD_GYR_NORMAL   0x15   /**< Gyro  → Normal mode  */
#define BMI160_CMD_GYR_FAST     0x17   /**< Gyro  → Fast Start-up */
#define BMI160_CMD_GYR_SUSPEND  0x14   /**< Gyro  → Suspend      */
#define BMI160_CMD_STEP_RESET   0xB2   /**< Reset step counter   */

/* ────────── Accelerometer ODR values (ACC_CONF bits 3:0) ──── */
#define BMI160_ACC_ODR_25HZ     0x06
#define BMI160_ACC_ODR_50HZ     0x07
#define BMI160_ACC_ODR_100HZ    0x08
#define BMI160_ACC_ODR_200HZ    0x09
#define BMI160_ACC_ODR_400HZ    0x0A
#define BMI160_ACC_ODR_800HZ    0x0B

/* ─────────── Accelerometer range values ───────────────────── */
#define BMI160_ACC_RANGE_2G     0x03
#define BMI160_ACC_RANGE_4G     0x05
#define BMI160_ACC_RANGE_8G     0x08
#define BMI160_ACC_RANGE_16G    0x0C

/* ──────── Gyroscope ODR values (GYR_CONF bits 3:0) ────────── */
#define BMI160_GYR_ODR_25HZ    0x06
#define BMI160_GYR_ODR_50HZ    0x07
#define BMI160_GYR_ODR_100HZ   0x08
#define BMI160_GYR_ODR_200HZ   0x09
#define BMI160_GYR_ODR_400HZ   0x0A
#define BMI160_GYR_ODR_800HZ   0x0B
#define BMI160_GYR_ODR_1600HZ  0x0C
#define BMI160_GYR_ODR_3200HZ  0x0D

/* ─────────── Gyroscope range values ───────────────────────── */
#define BMI160_GYR_RANGE_2000DPS  0x00
#define BMI160_GYR_RANGE_1000DPS  0x01
#define BMI160_GYR_RANGE_500DPS   0x02
#define BMI160_GYR_RANGE_250DPS   0x03
#define BMI160_GYR_RANGE_125DPS   0x04

/* ──────── PMU_STATUS bit-field positions ───────────────────── */
#define BMI160_ACC_PMU_STATUS_SHIFT   4
#define BMI160_ACC_PMU_STATUS_MASK    0x30
#define BMI160_GYR_PMU_STATUS_SHIFT   2
#define BMI160_GYR_PMU_STATUS_MASK    0x0C
#define BMI160_PMU_NORMAL             0x01

/* ═══════════════════════ API Pública ══════════════════════════ */

/**
 * @brief Inicializa el BMI160 por I2C.
 *
 * Secuencia:
 *   1. Dummy read (despertar interfaz I2C)
 *   2. Soft-reset
 *   3. Verificar CHIP_ID == 0xD1
 *   4. Accel → Normal mode, ODR 100 Hz, rango ±2 g
 *   5. Gyro  → Normal mode, ODR 100 Hz, rango ±2000 °/s
 *   6. Verificar PMU_STATUS
 *
 * @return ESP_OK si exitoso, ESP_FAIL o error I2C si falla.
 */
esp_err_t bmi160_init(void);

/**
 * @brief Lee datos crudos del acelerómetro (3 ejes).
 *
 * @param[out] ax  Aceleración eje X (raw, 16-bit signed)
 * @param[out] ay  Aceleración eje Y
 * @param[out] az  Aceleración eje Z
 * @return ESP_OK si exitoso.
 */
esp_err_t bmi160_read_accel(int16_t *ax, int16_t *ay, int16_t *az);

/**
 * @brief Lee datos crudos de acelerómetro + giroscopio (6 ejes).
 *
 * Lectura burst de 12 bytes (GYR_X..ACC_Z) desde 0x0C.
 *
 * @param[out] ax,ay,az  Acelerómetro (raw int16)
 * @param[out] gx,gy,gz  Giroscopio (raw int16)
 * @return ESP_OK si exitoso.
 */
esp_err_t bmi160_read_accel_gyro(int16_t *ax, int16_t *ay, int16_t *az,
                                  int16_t *gx, int16_t *gy, int16_t *gz);

/**
 * @brief Habilita el step counter por hardware del BMI160.
 *
 * Configura STEP_CONF_0/1 en modo "normal" y habilita el conteo.
 *
 * @return ESP_OK si exitoso.
 */
esp_err_t bmi160_enable_step_counter(void);

/**
 * @brief Lee el conteo de pasos acumulado del step counter.
 *
 * @param[out] step_count Puntero donde se almacena el conteo (0..65535).
 * @return ESP_OK si exitoso.
 */
esp_err_t bmi160_read_step_counter(uint16_t *step_count);

/**
 * @brief Resetea el conteo de pasos a 0.
 *
 * @return ESP_OK si exitoso.
 */
esp_err_t bmi160_reset_step_counter(void);

#endif /* BMI160_H */

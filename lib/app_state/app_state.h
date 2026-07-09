#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    uint32_t steps_sw;
    float    temperature_c;
    uint16_t battery_mv;
    float    battery_soc;
    float    battery_crate;      /* %/hora — positivo = cargando, negativo = descargando */
    bool     battery_charging;   /* true si crate > CHARGE_THR (histéresis) */
    uint8_t  hr_bpm;
    uint8_t  spo2_pct;
    bool     finger_present;

    /* Estado HAR consolidado (0=reposo 1=caminar 2=correr 3=escaleras).
     * Lo escribe on_har_result() desde la har_task; lo lee la UI local y el TLV 0x08. */
    uint8_t  har_state;

    /* Timestamps de última actualización (ms desde boot) */
    uint32_t hr_updated_ms;
    uint32_t spo2_updated_ms;
    uint32_t temp_updated_ms;
    uint32_t bat_updated_ms;
    uint32_t har_updated_ms;
} shared_sensor_data_t;

/**
 * @brief Inicializa el mutex para proteger el estado compartido de sensores.
 */
void app_state_init(void);

/**
 * @brief Intenta bloquear el estado para lectura o escritura.
 * @param timeout_ms Tiempo máximo de espera en milisegundos.
 * @return Puntero al estado compartido si se obtuvo el lock, NULL si expiró.
 */
shared_sensor_data_t *app_state_lock(uint32_t timeout_ms);

/**
 * @brief Libera el lock del estado compartido.
 */
void app_state_unlock(void);

/**
 * @brief Obtiene si la transmisión de datos IMU por BLE está habilitada.
 */
bool app_state_imu_tx_enabled(void);

/**
 * @brief Establece si la transmisión de datos IMU por BLE está habilitada.
 */
void app_state_set_imu_tx_enabled(bool enabled);

#endif // APP_STATE_H

/**
 * @file power_modes.h
 * @brief Modos de energía del SupaClock (Sport / Normal / Saver).
 *
 * Cada modo define cadencias de polling y de transmisión BLE para todos
 * los sensores. Las tasks consultan el perfil activo al inicio de cada
 * iteración via power_get_profile() y ajustan su vTaskDelay.
 *
 * El modo se persiste en NVS bajo la clave "power_mode" (namespace
 * "supaclock"). El default si no existe en NVS es POWER_MODE_SPORT
 * (período de validación de sensores).
 *
 * Los timeouts de auto-off de pantalla por modo se persisten en NVS
 * bajo "off_sport_s", "off_normal_s", "off_saver_s".
 */
#ifndef POWER_MODES_H
#define POWER_MODES_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    POWER_MODE_SPORT  = 0,
    POWER_MODE_NORMAL = 1,
    POWER_MODE_SAVER  = 2,
    POWER_MODE_COUNT
} power_mode_t;

/**
 * @brief Perfil de cadencias y comportamiento por modo.
 *
 * Todas las cadencias en milisegundos. Un valor de 0 en
 * hrm_auto_period_ms significa "modo continuo" (HRM siempre on).
 * Un valor de 0 en spo2_auto_period_ms significa "no medir SpO2 en
 * automático" (sólo manual desde la pantalla SPOT).
 */
typedef struct {
    /* MAX30102 */
    uint16_t hrm_poll_ms;          /**< Período de polling FIFO cuando HRM activo */
    uint32_t hrm_auto_period_ms;   /**< 0 = continuo; >0 = spot cada N ms */
    uint32_t spo2_auto_period_ms;  /**< 0 = sólo manual; >0 = spot cada N ms */
    bool     hrm_shdn_between;     /**< true = poner sensor en SHDN entre mediciones */

    /* IMU */
    uint16_t imu_poll_ms;          /**< Período de polling accel/giro */

    /* Sensores I2C lentos */
    uint16_t temp_period_ms;       /**< Período lectura MAX30205 */
    uint16_t bat_period_ms;        /**< Período lectura MAX17048 */

    /* BLE telemetría agregada */
    uint16_t ble_agg_flush_ms;     /**< Período de flush del buffer agregado */

    /* Display */
    uint16_t display_off_default_s;/**< Default auto-off (override por NVS) */

    const char *name;
} power_profile_t;

/**
 * @brief Inicializa el módulo. Lee modo guardado de NVS.
 * Si no hay valor en NVS, usa POWER_MODE_SPORT (validación de sensores).
 *
 * Debe llamarse después de nvs_flash_init().
 */
esp_err_t power_modes_init(void);

/**
 * @brief Devuelve el modo activo.
 */
power_mode_t power_get_mode(void);

/**
 * @brief Cambia el modo activo y lo persiste en NVS.
 *
 * Las tasks lo verán en su próxima iteración (no requiere notificación
 * explícita). Si quieres respuesta inmediata, dale un yield al scheduler.
 *
 * @return ESP_OK si exitoso.
 */
esp_err_t power_set_mode(power_mode_t mode);

/**
 * @brief Devuelve puntero al perfil del modo activo.
 *
 * Puntero estable durante toda la ejecución; los datos pueden cambiar
 * tras un power_set_mode() pero la dirección no se invalida (apunta a
 * tabla const).
 */
const power_profile_t *power_get_profile(void);

/**
 * @brief Devuelve el perfil de un modo arbitrario (para UI / debug).
 */
const power_profile_t *power_get_profile_by_mode(power_mode_t mode);

/**
 * @brief Auto-off de pantalla configurado para el modo dado (segundos).
 *
 * Lee de NVS si fue configurado por el usuario; si no, usa el default
 * del perfil.
 */
uint16_t power_get_display_off_s(power_mode_t mode);

/**
 * @brief Configura el auto-off de pantalla para un modo y persiste en NVS.
 */
esp_err_t power_set_display_off_s(power_mode_t mode, uint16_t seconds);

/**
 * @brief Helper: nombre legible del modo.
 */
const char *power_mode_name(power_mode_t mode);

#endif /* POWER_MODES_H */

#include "power_modes.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "POWER_MODES";

#define NVS_NAMESPACE       "supaclock"
#define NVS_KEY_MODE        "power_mode"
#define NVS_KEY_OFF_SPORT   "off_sport_s"
#define NVS_KEY_OFF_NORMAL  "off_normal_s"
#define NVS_KEY_OFF_SAVER   "off_saver_s"

static SemaphoreHandle_t s_mtx = NULL;
static power_mode_t s_mode = POWER_MODE_SPORT;

/* Tabla const con los perfiles. Las cadencias siguen las decisiones
 * acordadas con el usuario (ver historial de diseño). */
static const power_profile_t s_profiles[POWER_MODE_COUNT] = {
    [POWER_MODE_SPORT] = {
        .hrm_poll_ms          = 100,        /* polling FIFO 10 Hz, capta 2-3 muestras/burst */
        .hrm_auto_period_ms   = 0,          /* continuo, nunca SHDN */
        .spo2_auto_period_ms  = 5UL * 60 * 1000,  /* SpO2 cada 5 min */
        .hrm_shdn_between     = false,
        .imu_poll_ms          = 20,         /* 50 Hz para pasos + jerk para HRM gating */
        .temp_period_ms       = 30 * 1000,  /* temp cada 30 s */
        .bat_period_ms        = 30 * 1000,  /* batería cada 30 s en TODOS los modos */
        .ble_agg_flush_ms     = 1000,       /* flush 1 s */
        .display_off_default_s= 30,
        .name = "SPORT",
    },
    [POWER_MODE_NORMAL] = {
        .hrm_poll_ms          = 100,
        .hrm_auto_period_ms   = 10UL * 60 * 1000, /* HR spot cada 10 min */
        .spo2_auto_period_ms  = 30UL * 60 * 1000, /* SpO2 spot cada 30 min */
        .hrm_shdn_between     = true,
        .imu_poll_ms          = 40,         /* 25 Hz */
        .temp_period_ms       = 5 * 60 * 1000, /* temp cada 5 min */
        .bat_period_ms        = 30 * 1000,
        .ble_agg_flush_ms     = 10 * 1000,  /* flush cada 10 s */
        .display_off_default_s= 15,
        .name = "NORMAL",
    },
    [POWER_MODE_SAVER] = {
        .hrm_poll_ms          = 100,
        .hrm_auto_period_ms   = 30UL * 60 * 1000, /* HR spot cada 30 min */
        .spo2_auto_period_ms  = 0,          /* sólo manual */
        .hrm_shdn_between     = true,
        .imu_poll_ms          = 80,         /* 12.5 Hz */
        .temp_period_ms       = 15 * 60 * 1000, /* temp cada 15 min */
        .bat_period_ms        = 30 * 1000,
        .ble_agg_flush_ms     = 60 * 1000,  /* flush cada 60 s */
        .display_off_default_s= 8,
        .name = "SAVER",
    },
};

/* Auto-off por modo (con override desde NVS) */
static uint16_t s_display_off_s[POWER_MODE_COUNT];

esp_err_t power_modes_init(void) {
    if (s_mtx == NULL) {
        s_mtx = xSemaphoreCreateMutex();
        if (!s_mtx) return ESP_ERR_NO_MEM;
    }

    /* Cargar modo desde NVS, default SPORT */
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open falló (%s); usando defaults", esp_err_to_name(err));
        s_mode = POWER_MODE_SPORT;
        for (int i = 0; i < POWER_MODE_COUNT; i++)
            s_display_off_s[i] = s_profiles[i].display_off_default_s;
        return ESP_OK;
    }

    uint8_t mode_u8 = POWER_MODE_SPORT;
    if (nvs_get_u8(h, NVS_KEY_MODE, &mode_u8) != ESP_OK || mode_u8 >= POWER_MODE_COUNT) {
        mode_u8 = POWER_MODE_SPORT;
    }
    s_mode = (power_mode_t)mode_u8;

    /* Cargar overrides de auto-off; si no existen usar defaults */
    const char *keys[POWER_MODE_COUNT] = {
        NVS_KEY_OFF_SPORT, NVS_KEY_OFF_NORMAL, NVS_KEY_OFF_SAVER
    };
    for (int i = 0; i < POWER_MODE_COUNT; i++) {
        uint16_t v = 0;
        if (nvs_get_u16(h, keys[i], &v) == ESP_OK && v > 0) {
            s_display_off_s[i] = v;
        } else {
            s_display_off_s[i] = s_profiles[i].display_off_default_s;
        }
    }

    nvs_close(h);
    ESP_LOGI(TAG, "Modo inicial: %s", s_profiles[s_mode].name);
    return ESP_OK;
}

power_mode_t power_get_mode(void) {
    if (!s_mtx) return s_mode;
    power_mode_t m;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    m = s_mode;
    xSemaphoreGive(s_mtx);
    return m;
}

esp_err_t power_set_mode(power_mode_t mode) {
    if (mode >= POWER_MODE_COUNT) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_mode = mode;
    xSemaphoreGive(s_mtx);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        nvs_set_u8(h, NVS_KEY_MODE, (uint8_t)mode);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "Modo cambiado a: %s", s_profiles[mode].name);
    return ESP_OK;
}

const power_profile_t *power_get_profile(void) {
    return &s_profiles[power_get_mode()];
}

const power_profile_t *power_get_profile_by_mode(power_mode_t mode) {
    if (mode >= POWER_MODE_COUNT) return &s_profiles[POWER_MODE_NORMAL];
    return &s_profiles[mode];
}

uint16_t power_get_display_off_s(power_mode_t mode) {
    if (mode >= POWER_MODE_COUNT) return 15;
    return s_display_off_s[mode];
}

esp_err_t power_set_display_off_s(power_mode_t mode, uint16_t seconds) {
    if (mode >= POWER_MODE_COUNT) return ESP_ERR_INVALID_ARG;
    if (seconds < 3) seconds = 3;       /* mínimo razonable */
    if (seconds > 600) seconds = 600;   /* tope 10 min */

    s_display_off_s[mode] = seconds;

    const char *keys[POWER_MODE_COUNT] = {
        NVS_KEY_OFF_SPORT, NVS_KEY_OFF_NORMAL, NVS_KEY_OFF_SAVER
    };
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u16(h, keys[mode], seconds);
        nvs_commit(h);
        nvs_close(h);
    }
    return ESP_OK;
}

const char *power_mode_name(power_mode_t mode) {
    if (mode >= POWER_MODE_COUNT) return "?";
    return s_profiles[mode].name;
}

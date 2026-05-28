#include "ui_theme.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "UI_THEME";

#define NVS_NAMESPACE   "supaclock"
#define NVS_KEY_THEME   "ui_theme"

/* Tabla const de paletas. Orden == ui_theme_id_t. */
static const ui_theme_t s_themes[UI_THEME_COUNT] = {
    [UI_THEME_AMOLED] = {
        .name = "AMOLED",
        .bg = 0x000000, .surface = 0x141414,
        .text = 0xFFFFFF, .text_dim = 0x8A8A8A,
        .accent = 0x4ECDC4, .alert = 0xFF5252,
        .ok = 0x4ECDC4, .warn = 0xE0A458,
        /* casi monocromo: métricas en blanco, acento sólo en batería/actividad */
        .c_hr = 0xFFFFFF, .c_spo2 = 0xFFFFFF, .c_temp = 0xFFFFFF,
        .c_steps = 0xFFFFFF, .c_batt = 0x4ECDC4, .c_activity = 0x4ECDC4,
    },
    [UI_THEME_WARM] = {
        .name = "WARM",
        .bg = 0x121212, .surface = 0x1E1B18,
        .text = 0xF5F0E8, .text_dim = 0x9A9388,
        .accent = 0x8FB996, .alert = 0xE0A458,
        .ok = 0x8FB996, .warn = 0xE0A458,
        .c_hr = 0xD98C7A, .c_spo2 = 0x8FB0B9, .c_temp = 0xE0A458,
        .c_steps = 0x8FB996, .c_batt = 0x8FB996, .c_activity = 0xC9B68F,
    },
    [UI_THEME_SLATE] = {
        .name = "SLATE",
        .bg = 0x0D1117, .surface = 0x161B22,
        .text = 0xE6EDF3, .text_dim = 0x7D8590,
        .accent = 0x4EA8DE, .alert = 0xF85149,
        .ok = 0x3FB950, .warn = 0xF0883E,
        .c_hr = 0xF85149, .c_spo2 = 0x4EA8DE, .c_temp = 0xF0883E,
        .c_steps = 0x3FB950, .c_batt = 0x3FB950, .c_activity = 0x58A6FF,
    },
    [UI_THEME_VIVID] = {
        .name = "VIVID",
        .bg = 0x000000, .surface = 0x1A1A1A,
        .text = 0xFFFFFF, .text_dim = 0x8B949E,
        .accent = 0x00D2FF, .alert = 0xFF0000,
        .ok = 0x3FB950, .warn = 0xF0883E,
        .c_hr = 0xFF3B6E, .c_spo2 = 0x3F9BFF, .c_temp = 0xF0883E,
        .c_steps = 0x3FB950, .c_batt = 0x3FB950, .c_activity = 0x3F9BFF,
    },
};

static ui_theme_id_t s_active = UI_THEME_AMOLED;

esp_err_t ui_theme_init(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open falló (%s); usando AMOLED", esp_err_to_name(err));
        s_active = UI_THEME_AMOLED;
        return ESP_OK;
    }

    uint8_t id = UI_THEME_AMOLED;
    if (nvs_get_u8(h, NVS_KEY_THEME, &id) != ESP_OK || id >= UI_THEME_COUNT) {
        id = UI_THEME_AMOLED;
    }
    s_active = (ui_theme_id_t)id;
    nvs_close(h);
    ESP_LOGI(TAG, "Tema activo: %s", s_themes[s_active].name);
    return ESP_OK;
}

const ui_theme_t *ui_theme_get(void) {
    return &s_themes[s_active];
}

ui_theme_id_t ui_theme_get_id(void) {
    return s_active;
}

esp_err_t ui_theme_set(ui_theme_id_t id) {
    if (id >= UI_THEME_COUNT) return ESP_ERR_INVALID_ARG;
    s_active = id;

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, NVS_KEY_THEME, (uint8_t)id);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "Tema cambiado a: %s", s_themes[id].name);
    return ESP_OK;
}

uint8_t ui_theme_count(void) {
    return UI_THEME_COUNT;
}

const char *ui_theme_name(ui_theme_id_t id) {
    if (id >= UI_THEME_COUNT) return "?";
    return s_themes[id].name;
}

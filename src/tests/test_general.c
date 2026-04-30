#ifdef ENV_TEST_GENERAL

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include "st7789.h"
#include "i2c_bus.h"
#include "max17048.h"
#include "bmi160.h"
#include "max30205.h"
#include "max30102.h"
#include "ble_telemetry.h"
#include "step_algorithm.h"
#include "gpio_buttons.h"
#include "ad8232.h"
#include "esp_adc/adc_continuous.h"
#include "power_modes.h"

static const char *TAG = "Test_General";

/* ───────────────────────── Mutexes ───────────────────────── */
static SemaphoreHandle_t xGuiSemaphore;
static SemaphoreHandle_t xSensorDataMutex;

/* ───────────────────────── Datos compartidos ───────────────────────── */
typedef struct {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    uint32_t steps_sw;
    float    temperature_c;
    uint16_t battery_mv;
    float    battery_soc;
    uint8_t  hr_bpm;
    uint8_t  spo2_pct;
    bool     finger_present;

    /* Timestamps de última actualización (ms desde boot) */
    uint32_t hr_updated_ms;
    uint32_t spo2_updated_ms;
    uint32_t temp_updated_ms;
    uint32_t bat_updated_ms;
} shared_sensor_data_t;

static shared_sensor_data_t sensor_data = {0};

static bool imu_ble_tx_enabled = true;

static inline uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* ═══════════════════════════════════════════════════════════════════
 *  GUI multi-pantalla
 *
 *  Ciclo NEXT: HOME → BIO → HRSPOT → ECG → MENU → HOME
 *  MODE y SETTINGS son sub-screens, sólo accesibles desde MENU.
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    SCREEN_HOME = 0,
    SCREEN_BIO,
    SCREEN_HRSPOT,
    SCREEN_ECG,
    SCREEN_MENU,
    SCREEN_MODE,       /* sub-screen */
    SCREEN_SETTINGS,   /* sub-screen */
    SCREEN_COUNT,
} ui_screen_t;
#define SCREEN_CYCLE_COUNT 5  /* HOME..MENU ciclan */

#define MENU_ITEM_COUNT 7
static const char *MENU_LABELS[MENU_ITEM_COUNT] = {
    LV_SYMBOL_SETTINGS " Modo Energia",
    LV_SYMBOL_EYE_CLOSE " Auto-off Pant.",
    LV_SYMBOL_REFRESH " Reiniciar Pasos",
    LV_SYMBOL_BLUETOOTH " Vincular BLE",
    LV_SYMBOL_POWER " Apagar",
    LV_SYMBOL_WIFI " Tx IMU: ON",
    LV_SYMBOL_CHARGE " Reset Bateria",
};

#define MODE_ITEM_COUNT 3
static const char *MODE_LABELS[MODE_ITEM_COUNT] = {
    "SPORT",
    "NORMAL",
    "SAVER",
};

#define SETTINGS_ITEM_COUNT 3
static const char *SETTINGS_LABELS[SETTINGS_ITEM_COUNT] = {
    "Off SPORT",
    "Off NORMAL",
    "Off SAVER",
};

/* Valores cíclicos para auto-off (segundos) */
static const uint16_t AUTO_OFF_VALUES[] = {5, 8, 15, 30, 60, 120};
#define AUTO_OFF_VALUES_COUNT (sizeof(AUTO_OFF_VALUES) / sizeof(AUTO_OFF_VALUES[0]))

/* Estado de navegación */
static ui_screen_t current_screen = SCREEN_HOME;
static uint8_t menu_selection = 0;
static uint8_t mode_selection = 0;
static uint8_t settings_selection = 0;
static int64_t ecg_start_us = 0;

/* Screens */
static lv_obj_t *scr_obj[SCREEN_COUNT];

/* Labels Home */
static lv_obj_t *home_clock, *home_steps, *home_bat, *home_bat_arc, *home_hr, *home_act, *home_mode;
/* Labels Bio */
static lv_obj_t *bio_hr, *bio_spo2, *bio_temp, *bio_status, *bio_age_hr, *bio_age_spo2;
/* Labels HRSpot */
static lv_obj_t *hrspot_instr, *hrspot_progress, *hrspot_result, *hrspot_quality;
/* Labels ECG */
static lv_obj_t *ecg_instr, *ecg_timer, *ecg_rec;
/* Menú principal */
static lv_obj_t *menu_rows[MENU_ITEM_COUNT];
/* Sub-menú Mode */
static lv_obj_t *mode_rows[MODE_ITEM_COUNT];
static lv_obj_t *mode_active_label;
/* Sub-menú Settings */
static lv_obj_t *settings_rows[SETTINGS_ITEM_COUNT];

/* Display backlight */
static int inactivity_counter_ds = 0;  /* en décimas de segundo (~33 ms × 3) */
static bool backlight_on = false;

/* Buffer LVGL */
#define DISP_BUF_SIZE (240 * 30 * 1)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[DISP_BUF_SIZE];

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_40);

/* ─────────────────── Utilities para optimizar LVGL CPU ─────────────────── */
static void lv_label_set_text_safe(lv_obj_t *label, const char *text) {
    if (!label || !text) return;
    const char *current_text = lv_label_get_text(label);
    if (strcmp(current_text ? current_text : "", text) != 0) {
        lv_label_set_text(label, text);
    }
}

static void lv_label_set_text_fmt_safe(lv_obj_t *label, const char *fmt, ...) {
    if (!label || !fmt) return;
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    const char *current_text = lv_label_get_text(label);
    if (strcmp(current_text ? current_text : "", buf) != 0) {
        lv_label_set_text(label, buf);
    }
}

static void display_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint16_t x = area->x1, y = area->y1;
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    st7789_draw_bitmap(x, y, w, h, (const uint16_t *)color_p);
    lv_disp_flush_ready(disp_drv);
}

/* ─────────────────── Helpers de construcción ─────────────────── */

static lv_obj_t * create_card(lv_obj_t * parent, int x, int y, int w, int h) {
    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static lv_obj_t *make_screen(const char *title, uint32_t title_color) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text_safe(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(title_color), LV_PART_MAIN);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 6);
    return scr;
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                            uint32_t color_hex, lv_align_t align,
                            int x_ofs, int y_ofs, const char *txt) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text_safe(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(color_hex), LV_PART_MAIN);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_align(l, align, x_ofs, y_ofs);
    return l;
}

/* ─────────────────── Construcción de pantallas ─────────────────── */

static void build_home(void) {
    scr_obj[SCREEN_HOME] = make_screen("SUPACLOCK", 0x00D2FF);
    lv_obj_t *s = scr_obj[SCREEN_HOME];

    home_clock = make_label(s, &lv_font_montserrat_40, 0xFFFFFF,
                            LV_ALIGN_TOP_MID, 0, 30, "--:--");
    lv_label_set_recolor(home_clock, true);

    home_mode  = make_label(s, &lv_font_montserrat_14, 0x8B949E,
                            LV_ALIGN_TOP_MID, 0, 80, "MODE: SPORT");

    int card_w = 100;
    int card_h = 70;
    int pad_x = 15;
    int pad_y = 110;
    int gap = 10;

    lv_obj_t * card_steps = create_card(s, pad_x, pad_y, card_w, card_h);
    lv_obj_t * card_bat   = create_card(s, pad_x + card_w + gap, pad_y, card_w, card_h);
    lv_obj_t * card_hr    = create_card(s, pad_x, pad_y + card_h + gap, card_w, card_h);
    lv_obj_t * card_act   = create_card(s, pad_x + card_w + gap, pad_y + card_h + gap, card_w, card_h);

    home_steps = make_label(card_steps, &lv_font_montserrat_20, 0x3FB950, LV_ALIGN_CENTER, 0, 0, "#ffffff " LV_SYMBOL_LIST "# 0");
    lv_label_set_recolor(home_steps, true);

    home_bat_arc = lv_arc_create(card_bat);
    lv_obj_set_size(home_bat_arc, 60, 60);
    lv_obj_align(home_bat_arc, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_rotation(home_bat_arc, 270);
    lv_arc_set_bg_angles(home_bat_arc, 0, 360);
    lv_obj_remove_style(home_bat_arc, NULL, LV_PART_KNOB); // No knob
    lv_obj_clear_flag(home_bat_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(home_bat_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(home_bat_arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(home_bat_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(home_bat_arc, lv_color_hex(0x3FB950), LV_PART_INDICATOR);
    
    home_bat = make_label(home_bat_arc, &lv_font_montserrat_14, 0xFFFFFF, LV_ALIGN_CENTER, 0, 0, "--%");

    home_hr = make_label(card_hr, &lv_font_montserrat_20, 0xFF3B6E, LV_ALIGN_CENTER, 0, 0, "#ffffff " LV_SYMBOL_TINT "# --");
    lv_label_set_recolor(home_hr, true);

    home_act = make_label(card_act, &lv_font_montserrat_14, 0x3F9BFF, LV_ALIGN_CENTER, 0, 0, "#ffffff " LV_SYMBOL_CHARGE "# --");
    lv_label_set_recolor(home_act, true);
}

static void build_bio(void) {
    scr_obj[SCREEN_BIO] = make_screen("BIOMETRIA", 0x00D2FF);
    lv_obj_t *s = scr_obj[SCREEN_BIO];

    bio_hr     = make_label(s, &lv_font_montserrat_20, 0xFF3B6E, LV_ALIGN_TOP_LEFT, 20, 50,  "#ffffff " LV_SYMBOL_TINT "# -- bpm");
    lv_label_set_recolor(bio_hr, true);
    bio_age_hr = make_label(s, &lv_font_montserrat_14, 0x8B949E, LV_ALIGN_TOP_LEFT, 20, 78, "");

    bio_spo2     = make_label(s, &lv_font_montserrat_20, 0x3F9BFF, LV_ALIGN_TOP_LEFT, 20, 105, "#ffffff " LV_SYMBOL_TINT "# --%");
    lv_label_set_recolor(bio_spo2, true);
    bio_age_spo2 = make_label(s, &lv_font_montserrat_14, 0x8B949E, LV_ALIGN_TOP_LEFT, 20, 133, "");

    bio_temp   = make_label(s, &lv_font_montserrat_20, 0xF0883E, LV_ALIGN_TOP_LEFT, 20, 160, "#ffffff " LV_SYMBOL_WARNING "# --.- C");
    lv_label_set_recolor(bio_temp, true);
    bio_status = make_label(s, &lv_font_montserrat_20, 0x3FB950, LV_ALIGN_TOP_LEFT, 20, 210, "Estado: --");
}

static void build_hrspot(void) {
    scr_obj[SCREEN_HRSPOT] = make_screen("MEDIDA HR/SPO2", 0xFF3B6E);
    lv_obj_t *s = scr_obj[SCREEN_HRSPOT];

    hrspot_instr = make_label(s, &lv_font_montserrat_14, 0xE6EDF3,
                              LV_ALIGN_TOP_MID, 0, 50,
                              "Apoye el dedo sobre\nel sensor MAX30102.\n\n"
                              "Pulse SELECT para iniciar.");
    lv_obj_set_style_text_align(hrspot_instr, LV_TEXT_ALIGN_CENTER, 0);

    hrspot_progress = make_label(s, &lv_font_montserrat_20, 0xF0C34E,
                                 LV_ALIGN_CENTER, 0, -10, "");
    hrspot_result   = make_label(s, &lv_font_montserrat_20, 0xFFFFFF,
                                 LV_ALIGN_CENTER, 0, 30, "");
    hrspot_quality  = make_label(s, &lv_font_montserrat_14, 0x8B949E,
                                 LV_ALIGN_BOTTOM_MID, 0, -20, "");
}

static void build_ecg(void) {
    scr_obj[SCREEN_ECG] = make_screen("MODO ECG", 0x3FB950);
    lv_obj_t *s = scr_obj[SCREEN_ECG];

    ecg_instr = make_label(s, &lv_font_montserrat_14, 0xE6EDF3,
                           LV_ALIGN_TOP_MID, 0, 50,
                           "Presione los electrodos\nlaterales con la mano\nopuesta.\n\n"
                           "Pulsa SELECT para iniciar");
    lv_obj_set_style_text_align(ecg_instr, LV_TEXT_ALIGN_CENTER, 0);

    ecg_timer = make_label(s, &lv_font_montserrat_40, 0xFFFFFF,
                           LV_ALIGN_CENTER, 0, 0, "0:00");
    lv_obj_add_flag(ecg_timer, LV_OBJ_FLAG_HIDDEN);

    ecg_rec = make_label(s, &lv_font_montserrat_20, 0xDA3633,
                         LV_ALIGN_BOTTOM_MID, 0, -20, "● REC");
    lv_obj_add_flag(ecg_rec, LV_OBJ_FLAG_HIDDEN);
}

static void build_menu(void) {
    scr_obj[SCREEN_MENU] = make_screen("MENU", 0x00D2FF);
    lv_obj_t *s = scr_obj[SCREEN_MENU];
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        menu_rows[i] = lv_label_create(s);
        lv_obj_set_style_text_font(menu_rows[i], &lv_font_montserrat_20, LV_PART_MAIN);
        lv_label_set_text_safe(menu_rows[i], MENU_LABELS[i]);
        lv_obj_set_width(menu_rows[i], 220);
        lv_obj_set_style_pad_all(menu_rows[i], 6, LV_PART_MAIN);
        lv_obj_set_style_radius(menu_rows[i], 6, LV_PART_MAIN);
        lv_obj_align(menu_rows[i], LV_ALIGN_TOP_MID, 0, 35 + i * 40);
    }
}

static void build_mode(void) {
    scr_obj[SCREEN_MODE] = make_screen("MODO ENERGIA", 0xF0C34E);
    lv_obj_t *s = scr_obj[SCREEN_MODE];

    mode_active_label = make_label(s, &lv_font_montserrat_14, 0x3FB950,
                                   LV_ALIGN_TOP_MID, 0, 30, "Activo: SPORT");

    for (int i = 0; i < MODE_ITEM_COUNT; i++) {
        mode_rows[i] = lv_label_create(s);
        lv_obj_set_style_text_font(mode_rows[i], &lv_font_montserrat_20, LV_PART_MAIN);
        lv_label_set_text_safe(mode_rows[i], MODE_LABELS[i]);
        lv_obj_set_width(mode_rows[i], 220);
        lv_obj_set_style_pad_all(mode_rows[i], 8, LV_PART_MAIN);
        lv_obj_set_style_radius(mode_rows[i], 6, LV_PART_MAIN);
        lv_obj_align(mode_rows[i], LV_ALIGN_TOP_MID, 0, 60 + i * 55);
    }

    /* Hint inferior */
    make_label(s, &lv_font_montserrat_14, 0x8B949E, LV_ALIGN_BOTTOM_MID, 0, -8,
               "SELECT: aplicar  L_NEXT: salir");
}

static void build_settings(void) {
    scr_obj[SCREEN_SETTINGS] = make_screen("AUTO-OFF PANT.", 0xF0C34E);
    lv_obj_t *s = scr_obj[SCREEN_SETTINGS];

    for (int i = 0; i < SETTINGS_ITEM_COUNT; i++) {
        settings_rows[i] = lv_label_create(s);
        lv_obj_set_style_text_font(settings_rows[i], &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_width(settings_rows[i], 220);
        lv_obj_set_style_pad_all(settings_rows[i], 8, LV_PART_MAIN);
        lv_obj_set_style_radius(settings_rows[i], 6, LV_PART_MAIN);
        lv_obj_align(settings_rows[i], LV_ALIGN_TOP_MID, 0, 50 + i * 55);
    }
    make_label(s, &lv_font_montserrat_14, 0x8B949E, LV_ALIGN_BOTTOM_MID, 0, -8,
               "SELECT: cambiar  L_NEXT: salir");
}

/* ─────────────────── Render de selección ─────────────────── */

static void render_list_selection(lv_obj_t **rows, int count, int sel, int y_base, int y_step, int max_vis) {
    int offset = 0;
    if (sel >= max_vis) {
        offset = (sel - max_vis + 1) * y_step;
    }
    for (int i = 0; i < count; i++) {
        if (y_step > 0) {
            lv_obj_align(rows[i], LV_ALIGN_TOP_MID, 0, y_base + i * y_step - offset);
        }
        if (i == sel) {
            lv_obj_set_style_bg_color(rows[i], lv_color_hex(0x1F6FEB), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(rows[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_text_color(rows[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_opa(rows[i], LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_text_color(rows[i], lv_color_hex(0xAAAAAA), LV_PART_MAIN);
        }
    }
}

static void render_settings_labels(void) {
    for (int i = 0; i < SETTINGS_ITEM_COUNT; i++) {
        uint16_t v = power_get_display_off_s((power_mode_t)i);
        lv_label_set_text_fmt_safe(settings_rows[i], "%s: %us", SETTINGS_LABELS[i], v);
    }
}

static void render_mode_active(void) {
    power_mode_t m = power_get_mode();
    lv_label_set_text_fmt_safe(mode_active_label, "Activo: %s", power_mode_name(m));
}

/* ─────────────────── Actualizadores por pantalla ─────────────────── */

static void update_home_screen(const shared_sensor_data_t *d) {
    uint32_t s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    lv_label_set_text_fmt_safe(home_clock, "#ffffff %lu#:#00d2ff %02lu#",
                          (unsigned long)((s / 60) % 100),
                          (unsigned long)(s % 60));

    lv_label_set_text_fmt_safe(home_mode, "MODE: %s", power_mode_name(power_get_mode()));
    lv_label_set_text_fmt_safe(home_steps, "#ffffff " LV_SYMBOL_LIST "# %lu", (unsigned long)d->steps_sw);
    
    int bat_pct = (int)d->battery_soc;
    if (bat_pct < 0) bat_pct = 0;
    if (bat_pct > 100) bat_pct = 100;
    lv_arc_set_value(home_bat_arc, bat_pct);
    lv_label_set_text_fmt_safe(home_bat, "%d%%", bat_pct);
    uint32_t bat_color = (bat_pct > 20) ? 0x3FB950 : 0xFF0000;
    lv_obj_set_style_arc_color(home_bat_arc, lv_color_hex(bat_color), LV_PART_INDICATOR);

    if (d->finger_present && d->hr_bpm > 0) {
        lv_label_set_text_fmt_safe(home_hr, "#ffffff " LV_SYMBOL_TINT "# %u", d->hr_bpm);
    } else {
        lv_label_set_text_safe(home_hr, "#ffffff " LV_SYMBOL_TINT "# --");
    }

    int32_t amag = (int32_t)d->ax * d->ax + (int32_t)d->ay * d->ay + (int32_t)d->az * d->az;
    if (amag > 300000000L) {
        lv_label_set_text_safe(home_act, "#ffffff " LV_SYMBOL_CHARGE "# Activo");
    } else {
        lv_label_set_text_safe(home_act, "#ffffff " LV_SYMBOL_CHARGE "# Reposo");
    }
}

/* Devuelve "ahora" / "hace 12s" / "hace 5m" según la edad */
static void format_age(char *out, size_t cap, uint32_t updated_ms) {
    if (updated_ms == 0) { snprintf(out, cap, "(sin medir)"); return; }
    uint32_t age = (now_ms() - updated_ms) / 1000;
    if (age < 5)        snprintf(out, cap, "ahora");
    else if (age < 60)  snprintf(out, cap, "hace %lus", (unsigned long)age);
    else if (age < 3600)snprintf(out, cap, "hace %lum", (unsigned long)(age / 60));
    else                snprintf(out, cap, "hace %luh", (unsigned long)(age / 3600));
}

static void update_bio_screen(const shared_sensor_data_t *d) {
    char age[24];

    if (d->hr_bpm > 0) {
        lv_label_set_text_fmt_safe(bio_hr, "#ffffff " LV_SYMBOL_TINT "# %u bpm", d->hr_bpm);
        format_age(age, sizeof(age), d->hr_updated_ms);
        lv_label_set_text_safe(bio_age_hr, age);
    } else {
        lv_label_set_text_safe(bio_hr, "#ffffff " LV_SYMBOL_TINT "# -- bpm");
        lv_label_set_text_safe(bio_age_hr, "");
    }

    if (d->spo2_pct > 0) {
        lv_label_set_text_fmt_safe(bio_spo2, "#ffffff " LV_SYMBOL_TINT "# %u%%", d->spo2_pct);
        format_age(age, sizeof(age), d->spo2_updated_ms);
        lv_label_set_text_safe(bio_age_spo2, age);
    } else {
        lv_label_set_text_safe(bio_spo2, "#ffffff " LV_SYMBOL_TINT "# --%");
        lv_label_set_text_safe(bio_age_spo2, "");
    }

    int t_int  = (int)d->temperature_c;
    int t_frac = (int)((d->temperature_c - t_int) * 10);
    if (t_frac < 0) t_frac = -t_frac;
    lv_label_set_text_fmt_safe(bio_temp, "#ffffff " LV_SYMBOL_WARNING "# %d.%d C", t_int, t_frac);

    const char *st;
    uint32_t col;
    if (!d->finger_present)        { st = "Estado: Sin dedo";  col = 0x8B949E; }
    else if (d->hr_bpm > 100)      { st = "Estado: Alto";      col = 0xF0883E; }
    else if (d->hr_bpm > 0)        { st = "Estado: Normal";    col = 0x3FB950; }
    else                           { st = "Estado: Midiendo";  col = 0xF0C34E; }
    
    /* Evitar actualizar si el texto y color son iguales (optimización de memoria/CPU en LVGL) */
    if (strcmp(lv_label_get_text(bio_status), st) != 0) {
        lv_label_set_text_safe(bio_status, st);
    }
    
    // LVGL no tiene un getter simple para el color principal de un label en V8, 
    // pero podemos re-aplicar el color. A diferencia de crear estilos nuevos o strings, 
    // esto es relativamente ligero, pero aún mejor es evitarlo si es posible.
    // Usaremos local_col para trackear el estado y no spamear set_style.
    static uint32_t last_col = 0;
    if (last_col != col) {
        lv_obj_set_style_text_color(bio_status, lv_color_hex(col), LV_PART_MAIN);
        last_col = col;
    }
}

static const char *quality_str(max30102_spot_quality_t q) {
    switch (q) {
        case SPOT_QUALITY_GOOD: return "Calidad: BUENA";
        case SPOT_QUALITY_FAIR: return "Calidad: REGULAR";
        case SPOT_QUALITY_POOR: return "Calidad: POBRE";
        default: return "";
    }
}

static void update_hrspot_screen(void) {
    max30102_spot_status_t st;
    max30102_spot_get_status(&st);

    switch (st.state) {
        case SPOT_STATE_IDLE:
            lv_obj_clear_flag(hrspot_instr, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_safe(hrspot_progress, "");
            lv_label_set_text_safe(hrspot_result, "");
            lv_label_set_text_safe(hrspot_quality, "");
            break;
        case SPOT_STATE_SETTLING:
            lv_obj_add_flag(hrspot_instr, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt_safe(hrspot_progress, "Estabilizando %u%%", st.progress_pct);
            lv_label_set_text_safe(hrspot_result, "");
            lv_label_set_text_safe(hrspot_quality, "");
            break;
        case SPOT_STATE_MEASURING:
            lv_obj_add_flag(hrspot_instr, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt_safe(hrspot_progress, "Midiendo %u%%", st.progress_pct);
            lv_label_set_text_safe(hrspot_result, "");
            lv_label_set_text_safe(hrspot_quality, "Quédese quieto");
            break;
        case SPOT_STATE_DONE:
            lv_obj_add_flag(hrspot_instr, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt_safe(hrspot_progress, "%.1fs", st.duration_ms / 1000.0f);
            lv_label_set_text_fmt_safe(hrspot_result, "%u bpm  %u%%", st.bpm, st.spo2);
            lv_label_set_text_safe(hrspot_quality, quality_str(st.quality));
            break;
        case SPOT_STATE_FAILED:
            lv_obj_add_flag(hrspot_instr, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_safe(hrspot_progress, "Sin senal usable");
            lv_label_set_text_safe(hrspot_result, "");
            lv_label_set_text_safe(hrspot_quality, "Apoye bien el dedo");
            break;
        case SPOT_STATE_ABORTED:
            lv_obj_add_flag(hrspot_instr, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_safe(hrspot_progress, "Cancelado");
            lv_label_set_text_safe(hrspot_result, "");
            lv_label_set_text_safe(hrspot_quality, "");
            break;
    }
}

static void update_ecg_screen(void) {
    bool rec = ble_telemetry_is_ecg_mode_active();
    if (rec) {
        if (ecg_start_us == 0) ecg_start_us = esp_timer_get_time();
        uint32_t secs = (uint32_t)((esp_timer_get_time() - ecg_start_us) / 1000000ULL);
        lv_label_set_text_fmt_safe(ecg_timer, "%lu:%02lu",
                              (unsigned long)(secs / 60), (unsigned long)(secs % 60));
        lv_obj_add_flag(ecg_instr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ecg_timer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ecg_rec, LV_OBJ_FLAG_HIDDEN);
    } else {
        ecg_start_us = 0;
        lv_obj_clear_flag(ecg_instr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ecg_timer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ecg_rec, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ─────────────────── Navegación & acciones ─────────────────── */

static void switch_to(ui_screen_t s) {
    current_screen = s;
    lv_scr_load(scr_obj[s]);
    if (s == SCREEN_MENU)     render_list_selection(menu_rows,     MENU_ITEM_COUNT,     menu_selection, 35, 40, 5);
    if (s == SCREEN_MODE)     { render_mode_active(); render_list_selection(mode_rows, MODE_ITEM_COUNT, mode_selection, 60, 55, 4); }
    if (s == SCREEN_SETTINGS) { render_settings_labels(); render_list_selection(settings_rows, SETTINGS_ITEM_COUNT, settings_selection, 50, 55, 4); }
    /* Al salir del HRSPOT en estado IDLE, asegúrate de cancelar */
    if (s != SCREEN_HRSPOT) {
        max30102_spot_status_t st;
        max30102_spot_get_status(&st);
        if (st.state == SPOT_STATE_SETTLING || st.state == SPOT_STATE_MEASURING) {
            max30102_spot_abort();
        }
    }
}

static void apply_selected_mode(void) {
    power_mode_t new_mode = (power_mode_t)mode_selection;
    if (new_mode == power_get_mode()) return;
    power_set_mode(new_mode);

    /* Notificar por BLE el cambio de modo */
    uint8_t m = (uint8_t)new_mode;
    ble_tx_push(BLE_TLV_TYPE_MODE_EVT, &m, 1, m);
    render_mode_active();
}

static void cycle_settings_value(void) {
    uint16_t cur = power_get_display_off_s((power_mode_t)settings_selection);
    int idx = 0;
    for (int i = 0; i < (int)AUTO_OFF_VALUES_COUNT; i++) {
        if (AUTO_OFF_VALUES[i] == cur) { idx = i; break; }
    }
    idx = (idx + 1) % AUTO_OFF_VALUES_COUNT;
    power_set_display_off_s((power_mode_t)settings_selection, AUTO_OFF_VALUES[idx]);
    render_settings_labels();
    render_list_selection(settings_rows, SETTINGS_ITEM_COUNT, settings_selection, 50, 55, 4);
}

static void menu_execute_selected(void) {
    switch (menu_selection) {
        case 0: /* Modo Energía */
            mode_selection = (uint8_t)power_get_mode();
            switch_to(SCREEN_MODE);
            break;
        case 1: /* Auto-off Pantalla */
            settings_selection = 0;
            switch_to(SCREEN_SETTINGS);
            break;
        case 2: /* Reiniciar Pasos */
            if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                sensor_data.steps_sw = 0;
                xSemaphoreGive(xSensorDataMutex);
            }
            ESP_LOGI(TAG, "Menu: pasos reiniciados");
            break;
        case 3: /* Vincular BLE */
            ESP_LOGI(TAG, "Menu: BLE advertising activo");
            break;
        case 4: /* Apagar */
            ESP_LOGI(TAG, "Menu: deep sleep");
            vTaskDelay(pdMS_TO_TICKS(200));
            esp_deep_sleep_enable_gpio_wakeup((1ULL << BTN_SELECT_PIN),
                                              ESP_GPIO_WAKEUP_GPIO_LOW);
            esp_deep_sleep_start();
            break;
        case 5: /* Tx IMU BLE */
            imu_ble_tx_enabled = !imu_ble_tx_enabled;
            lv_label_set_text_safe(menu_rows[5], imu_ble_tx_enabled ? "Tx IMU: ON" : "Tx IMU: OFF");
            ESP_LOGI(TAG, "Menu: Tx IMU BLE = %s", imu_ble_tx_enabled ? "ON" : "OFF");
            break;
        case 6: /* Reset Bateria */
            max17048_reset();
            ESP_LOGI(TAG, "Menu: Bateria reseteada (POR + Quick Start)");
            switch_to(SCREEN_HOME);
            break;
    }
}

static void handle_button(btn_event_t ev) {
    if (ev == BTN_EVENT_NONE) return;

    switch (ev) {
        case BTN_EVENT_NEXT_SHORT:
            if (current_screen == SCREEN_MENU) {
                if (menu_selection + 1 >= MENU_ITEM_COUNT) {
                    menu_selection = 0;
                    render_list_selection(menu_rows, MENU_ITEM_COUNT, menu_selection, 35, 40, 5);
                    switch_to(SCREEN_HOME);
                } else {
                    menu_selection++;
                    render_list_selection(menu_rows, MENU_ITEM_COUNT, menu_selection, 35, 40, 5);
                }
            } else if (current_screen == SCREEN_MODE) {
                mode_selection = (mode_selection + 1) % MODE_ITEM_COUNT;
                render_list_selection(mode_rows, MODE_ITEM_COUNT, mode_selection, 60, 55, 4);
            } else if (current_screen == SCREEN_SETTINGS) {
                settings_selection = (settings_selection + 1) % SETTINGS_ITEM_COUNT;
                render_list_selection(settings_rows, SETTINGS_ITEM_COUNT, settings_selection, 50, 55, 4);
            } else {
                switch_to((current_screen + 1) % SCREEN_CYCLE_COUNT);
            }
            break;
        case BTN_EVENT_NEXT_LONG:
            if (current_screen == SCREEN_MODE || current_screen == SCREEN_SETTINGS) {
                switch_to(SCREEN_MENU);
            } else {
                switch_to((current_screen + SCREEN_CYCLE_COUNT - 1) % SCREEN_CYCLE_COUNT);
            }
            break;
        case BTN_EVENT_SELECT_SHORT:
            if (current_screen == SCREEN_ECG) {
                ble_telemetry_set_ecg_mode(!ble_telemetry_is_ecg_mode_active());
            } else if (current_screen == SCREEN_MENU) {
                menu_execute_selected();
            } else if (current_screen == SCREEN_MODE) {
                apply_selected_mode();
            } else if (current_screen == SCREEN_SETTINGS) {
                cycle_settings_value();
            } else if (current_screen == SCREEN_HRSPOT) {
                max30102_spot_status_t st;
                max30102_spot_get_status(&st);
                if (st.state == SPOT_STATE_SETTLING || st.state == SPOT_STATE_MEASURING) {
                    max30102_spot_abort();
                } else {
                    max30102_spot_start();
                }
            }
            break;
        case BTN_EVENT_SELECT_LONG:
            switch_to(SCREEN_HOME);
            break;
        default: break;
    }
}

static void build_ui(void) {
    build_home();
    build_bio();
    build_hrspot();
    build_ecg();
    build_menu();
    build_mode();
    build_settings();
    render_list_selection(menu_rows, MENU_ITEM_COUNT, menu_selection, 35, 40, 5);
    render_list_selection(mode_rows, MODE_ITEM_COUNT, mode_selection, 60, 55, 4);
    render_list_selection(settings_rows, SETTINGS_ITEM_COUNT, settings_selection, 50, 55, 4);
    render_settings_labels();
    render_mode_active();
    lv_scr_load(scr_obj[SCREEN_HOME]);
}

/* ═══════════════════════════════════════════════════════════════════
 *  TASKS
 * ═══════════════════════════════════════════════════════════════════ */

void gui_task(void *pvParameter) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    st7789_set_brightness(100);
    backlight_on = true;

    /* Frames de inactividad acumulados (frame ≈ 33 ms) */
    while (1) {
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {

            btn_event_t ev;
            bool action_taken = false;
            while ((ev = gpio_buttons_poll()) != BTN_EVENT_NONE) {
                action_taken = true;
                inactivity_counter_ds = 0;
                if (!backlight_on) {
                    st7789_set_brightness(100);
                    backlight_on = true;
                } else {
                    handle_button(ev);
                }
            }

            if (!action_taken) {
                inactivity_counter_ds++;
                /* auto-off según modo activo */
                uint16_t off_s = power_get_display_off_s(power_get_mode());
                /* a ~30 fps → 30 frames/s */
                if (inactivity_counter_ds > (int)off_s * 30 && backlight_on) {
                    st7789_set_brightness(0);
                    backlight_on = false;
                }
            }

            /* Auto-switch a ECG cuando el cliente lo activa */
            if (ble_telemetry_is_ecg_mode_active() && current_screen != SCREEN_ECG) {
                switch_to(SCREEN_ECG);
            }

            /* Si la pantalla está apagada, no gastamos CPU actualizando la interfaz */
            if (backlight_on) {
                if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                    shared_sensor_data_t snap = sensor_data;
                    xSemaphoreGive(xSensorDataMutex);

                    switch (current_screen) {
                        case SCREEN_HOME:     update_home_screen(&snap); break;
                        case SCREEN_BIO:      update_bio_screen(&snap);  break;
                        case SCREEN_HRSPOT:   update_hrspot_screen();    break;
                        case SCREEN_ECG:      update_ecg_screen();       break;
                        case SCREEN_MENU:     break;
                        case SCREEN_MODE:     render_mode_active();      break;
                        case SCREEN_SETTINGS: break;
                        default: break;
                    }
                }

                lv_timer_handler();
            }
            xSemaphoreGive(xGuiSemaphore);
        }
        
        /* 
         * Si la pantalla está apagada, reducimos el frame-rate a 10 Hz (100ms) 
         * Dejamos 30 Hz (33ms) cuando está encendida para mantener los menús fluidos.
         */
        vTaskDelay(pdMS_TO_TICKS(backlight_on ? 33 : 100));
    }
}

/* ─────────────── ECG task ─────────────── */
void ecg_task(void *pvParameter) {
    uint8_t dma_buf[AD8232_READ_LEN];
    uint32_t ret_num = 0;

    #define ECG_DOWNSAMPLE_RATIO 40
    #define ECG_BLE_CHUNK_SIZE 10

    int16_t ble_chunk[ECG_BLE_CHUNK_SIZE];
    int chunk_idx = 0;
    uint32_t sum = 0;
    int count = 0;
    bool is_dma_running = false;

    while (1) {
        if (!ble_telemetry_is_ecg_mode_active()) {
            if (is_dma_running) {
                ad8232_stop_dma();
                is_dma_running = false;
            }
            vTaskDelay(pdMS_TO_TICKS(500)); /* Si no hay ECG, dormir profundamente este hilo */
            chunk_idx = 0; sum = 0; count = 0;
            continue;
        } else if (!is_dma_running) {
            ad8232_start_dma();
            is_dma_running = true;
        }

        esp_err_t ret = adc_continuous_read(ad8232_get_adc_handle(), dma_buf,
                                            AD8232_READ_LEN, &ret_num, pdMS_TO_TICKS(100));
        if (ret == ESP_OK) {
            for (int i = 0; i < ret_num; i += sizeof(adc_digi_output_data_t)) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t*)&dma_buf[i];
                uint16_t raw_val = p->type2.data;
                sum += raw_val; count++;
                if (count >= ECG_DOWNSAMPLE_RATIO) {
                    ble_chunk[chunk_idx++] = (int16_t)(sum / ECG_DOWNSAMPLE_RATIO);
                    sum = 0; count = 0;
                    if (chunk_idx >= ECG_BLE_CHUNK_SIZE) {
                        ble_telemetry_send_ecg(ble_chunk, sizeof(ble_chunk));
                        chunk_idx = 0;
                    }
                }
            }
        }
    }
}

/* ─────────────── IMU task: lee + step_algo + jerk + envío directo BLE ─────────────── */
void imu_task(void *pvParameter) {
    int16_t imu_raw[6] = {0};
    step_algo_state_t sw_pedometer;
    step_algo_init(&sw_pedometer);

    int16_t prev_ax = 0, prev_ay = 0, prev_az = 0;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1) {
        const power_profile_t *p = power_get_profile();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(p->imu_poll_ms));

        esp_err_t err = bmi160_read_accel_gyro(&imu_raw[0], &imu_raw[1], &imu_raw[2],
                                                &imu_raw[3], &imu_raw[4], &imu_raw[5]);
        if (err == ESP_OK) {
            uint32_t now = now_ms();

            /* Cálculo de jerk simple: |Δa| escalado a 0..255 */
            int32_t dx = imu_raw[0] - prev_ax;
            int32_t dy = imu_raw[1] - prev_ay;
            int32_t dz = imu_raw[2] - prev_az;
            int32_t mag2 = dx*dx + dy*dy + dz*dz;
            /* Umbral: ±2g ≈ 16384 LSB. Δa de 4000 = movimiento moderado.
             * mag2 ~ 16e6 → jerk_score ~ 80 (justo el threshold). */
            uint32_t jerk = mag2 / 200000;
            if (jerk > 255) jerk = 255;
            max30102_set_motion_level((uint8_t)jerk);
            prev_ax = imu_raw[0]; prev_ay = imu_raw[1]; prev_az = imu_raw[2];

            uint32_t new_steps = step_algo_update(&sw_pedometer,
                imu_raw[0], imu_raw[1], imu_raw[2],
                imu_raw[3], imu_raw[4], imu_raw[5], now);

            /* Envío IMU directo (no agregado): SPORT 50Hz, NORMAL 25Hz, SAVER 12.5Hz */
            if (imu_ble_tx_enabled) {
                ble_telemetry_send_imu(imu_raw, sizeof(imu_raw));
            }

            if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                sensor_data.ax = imu_raw[0]; sensor_data.ay = imu_raw[1]; sensor_data.az = imu_raw[2];
                sensor_data.gx = imu_raw[3]; sensor_data.gy = imu_raw[4]; sensor_data.gz = imu_raw[5];
                sensor_data.steps_sw += new_steps;
                xSemaphoreGive(xSensorDataMutex);
            }
        }
    }
}

/* ─────────────── HRM task: SM con modos energéticos ─────────────── */

static max30102_spot_state_t s_last_spot_state_handled = SPOT_STATE_IDLE;
static uint32_t s_last_auto_spot_ms = 0;
static uint32_t s_last_continuous_publish_ms = 0;

static void publish_hr_tlv(uint8_t bpm, uint8_t quality) {
    uint8_t rec[4] = {0};
    /* delta_ms = 0 (relativo al header del agg flush) */
    rec[2] = bpm;
    rec[3] = quality;
    ble_tx_push(BLE_TLV_TYPE_HR, rec, sizeof(rec), 0xFF);
}

static void publish_spo2_tlv(uint8_t pct, uint8_t quality) {
    uint8_t rec[4] = {0};
    rec[2] = pct;
    rec[3] = quality;
    ble_tx_push(BLE_TLV_TYPE_SPO2, rec, sizeof(rec), 0xFF);
}

static void publish_spot_result(const max30102_spot_status_t *st) {
    uint8_t rec[6];
    rec[0] = st->bpm;
    rec[1] = st->spo2;
    rec[2] = (uint8_t)(st->duration_ms & 0xFF);
    rec[3] = (uint8_t)((st->duration_ms >> 8) & 0xFF);
    rec[4] = (uint8_t)st->quality;
    rec[5] = (st->state == SPOT_STATE_ABORTED || st->state == SPOT_STATE_FAILED) ? 1 : 0;
    /* Forzar flush con resultado SPOT (alta prioridad) */
    ble_tx_push(BLE_TLV_TYPE_SPOT_RESULT, rec, sizeof(rec), (uint8_t)power_get_mode());
}

void hrm_task(void *pvParameter) {
    max30102_sample_t samples[32];
    max30102_flush_fifo();

    uint32_t last_ovf_logged = 0;

    while (1) {
        const power_profile_t *p = power_get_profile();
        bool continuous = (p->hrm_auto_period_ms == 0);

        // Si nunca ha medido, tomamos una medición rápida
        static bool has_measured_once = false;
        if (!continuous && !has_measured_once) {
            has_measured_once = true;
            s_last_auto_spot_ms = now_ms() - p->hrm_auto_period_ms; // Forzar que arranque la primera vez
        }

        max30102_spot_status_t spot_st;
        max30102_spot_get_status(&spot_st);
        bool spot_active = (spot_st.state == SPOT_STATE_SETTLING ||
                            spot_st.state == SPOT_STATE_MEASURING);

        bool sensor_should_be_on = continuous || spot_active;

        /* Auto-spot en modos NORMAL/SAVER cuando vence el período */
        if (!continuous && !spot_active &&
            (now_ms() - s_last_auto_spot_ms) >= p->hrm_auto_period_ms) {
            if (!max30102_is_awake()) max30102_wake();
            max30102_spot_start();
            s_last_auto_spot_ms = now_ms();
            sensor_should_be_on = true;
            ESP_LOGI(TAG, "HRM auto-spot iniciado (modo %s)", power_mode_name(power_get_mode()));
        }

        if (sensor_should_be_on && !max30102_is_awake()) {
            max30102_wake();
        } else if (!sensor_should_be_on && max30102_is_awake() && p->hrm_shdn_between) {
            max30102_shutdown();
        }

        /* Si el sensor está activo, leer FIFO y procesar */
        if (max30102_is_awake()) {
            uint8_t n = 0;
            if (max30102_read_samples(samples, 32, &n) == ESP_OK && n > 0) {
                for (uint8_t i = 0; i < n; i++) {
                    max30102_process_sample(samples[i].red, samples[i].ir);
                }
            }

            /* Publicar a sensor_data + BLE */
            uint8_t bpm = 0, spo2 = 0;
            max30102_get_hr(&bpm);
            max30102_get_spo2(&spo2);
            bool finger = max30102_finger_present();

            if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                sensor_data.finger_present = finger;
                if (bpm > 0)  { sensor_data.hr_bpm = bpm;     sensor_data.hr_updated_ms = now_ms(); }
                if (spo2 > 0) { sensor_data.spo2_pct = spo2;  sensor_data.spo2_updated_ms = now_ms(); }
                if (!finger)  { sensor_data.hr_bpm = 0; sensor_data.spo2_pct = 0; }
                xSemaphoreGive(xSensorDataMutex);
            }

            /* En SPORT publica HR/SpO2 cada 1 s al stream agregado */
            if (continuous && bpm > 0 &&
                (now_ms() - s_last_continuous_publish_ms) >= 1000) {
                publish_hr_tlv(bpm, 1);
                if (spo2 > 0) publish_spo2_tlv(spo2, 1);
                s_last_continuous_publish_ms = now_ms();
            }

            /* Manejo de transición SPOT terminado */
            max30102_spot_get_status(&spot_st);
            if (spot_st.state != s_last_spot_state_handled &&
                (spot_st.state == SPOT_STATE_DONE   ||
                 spot_st.state == SPOT_STATE_FAILED ||
                 spot_st.state == SPOT_STATE_ABORTED)) {
                publish_spot_result(&spot_st);
                if (spot_st.state == SPOT_STATE_DONE) {
                    if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        sensor_data.hr_bpm        = spot_st.bpm;
                        sensor_data.spo2_pct      = spot_st.spo2;
                        sensor_data.hr_updated_ms = now_ms();
                        sensor_data.spo2_updated_ms = now_ms();
                        xSemaphoreGive(xSensorDataMutex);
                    }
                }
                s_last_spot_state_handled = spot_st.state;

                /* Si era auto-spot en NORMAL/SAVER, dormir el sensor */
                if (!continuous) max30102_shutdown();
            }
            /* Si volvimos a IDLE (caller hizo spot_start de nuevo), resetear handled */
            if (spot_st.state == SPOT_STATE_IDLE ||
                spot_st.state == SPOT_STATE_SETTLING ||
                spot_st.state == SPOT_STATE_MEASURING) {
                s_last_spot_state_handled = SPOT_STATE_IDLE;
            }

            uint32_t ovf = max30102_get_overflow_count();
            if (ovf - last_ovf_logged >= 10) {
                ESP_LOGW(TAG, "MAX30102 overflows: %lu", (unsigned long)ovf);
                last_ovf_logged = ovf;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(p->hrm_poll_ms));
    }
}

/* ─────────────── system_task: temp + batería con cadencia por modo ─────────────── */
void system_task(void *pvParameter) {
    uint32_t last_temp_ms = 0;
    uint32_t last_bat_ms  = 0;
    uint32_t last_steps_pub_ms = 0;

    while (1) {
        const power_profile_t *p = power_get_profile();
        uint32_t now = now_ms();

        if (now - last_bat_ms >= p->bat_period_ms) {
            uint16_t bat_mv = 0;
            float bat_soc_raw = 0.0f;
            max17048_get_voltage(&bat_mv);
            esp_err_t err_soc = max17048_get_soc(&bat_soc_raw);

            // Filtro EMA para SOC
            static float bat_soc_filtered = -1.0f;
            if (err_soc == ESP_OK) {
                if (bat_soc_filtered < 0.0f) {
                    bat_soc_filtered = bat_soc_raw;
                } else {
                    bat_soc_filtered = 0.1f * bat_soc_raw + 0.9f * bat_soc_filtered;
                }
            }

            if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                sensor_data.battery_mv     = bat_mv;
                sensor_data.battery_soc    = (bat_soc_filtered >= 0.0f) ? bat_soc_filtered : 0.0f;
                sensor_data.bat_updated_ms = now;
                xSemaphoreGive(xSensorDataMutex);
            }

            uint8_t rec[5] = {0};
            /* delta_ms relativo al header (pongo 0; el header trae el ts base) */
            memcpy(&rec[2], &bat_mv, 2);
            rec[4] = (uint8_t)((bat_soc_filtered >= 0.0f) ? bat_soc_filtered : 0.0f);
            ble_tx_push(BLE_TLV_TYPE_BAT, rec, sizeof(rec), 0xFF);
            last_bat_ms = now;
        }

        if (now - last_temp_ms >= p->temp_period_ms) {
            float t = 0.0f;
            if (max30205_read_temperature(&t) == ESP_OK) {
                if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    sensor_data.temperature_c   = t;
                    sensor_data.temp_updated_ms = now;
                    xSemaphoreGive(xSensorDataMutex);
                }
                int16_t tx100 = (int16_t)(t * 100.0f);
                uint8_t rec[4] = {0};
                memcpy(&rec[2], &tx100, 2);
                ble_tx_push(BLE_TLV_TYPE_TEMP, rec, sizeof(rec), 0xFF);
            }
            last_temp_ms = now;
        }

        /* Pasos cada 30 s siempre (es info muy resumida) */
        if (now - last_steps_pub_ms >= 30 * 1000) {
            uint32_t steps = 0;
            if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                steps = sensor_data.steps_sw;
                xSemaphoreGive(xSensorDataMutex);
            }
            uint8_t rec[4];
            memcpy(rec, &steps, 4);
            ble_tx_push(BLE_TLV_TYPE_STEPS, rec, sizeof(rec), 0xFF);
            last_steps_pub_ms = now;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* ─────────────── ble_tx_task: flush periódico del buffer agregado ─────────────── */
void ble_tx_task(void *pvParameter) {
    while (1) {
        const power_profile_t *p = power_get_profile();
        vTaskDelay(pdMS_TO_TICKS(p->ble_agg_flush_ms));
        ble_tx_flush((uint8_t)power_get_mode());
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  app_main
 * ═══════════════════════════════════════════════════════════════════ */

void perf_monitor_task(void *pvParameter) {
    while(1) {
        ESP_LOGI("PERF", "--- Rendimiento ---");
        ESP_LOGI("PERF", "Heap Libre: %lu bytes", (unsigned long)esp_get_free_heap_size());
        ESP_LOGI("PERF", "Heap Min Libre: %lu bytes", (unsigned long)esp_get_minimum_free_heap_size());
        
#if 1
        char stats_buffer[1024];
        vTaskList(stats_buffer);
        ESP_LOGI("PERF", "=== Lista de Tareas (Estado, Prioridad, Pila (Libre), Task_Num) ===\n%s", stats_buffer);
        
        char runtime_buffer[1024];
        vTaskGetRunTimeStats(runtime_buffer);
        ESP_LOGI("PERF", "=== Uso de CPU ===\n%s", runtime_buffer);
#endif

#if CONFIG_PM_PROFILING
        ESP_LOGI("PERF", "=== Power Manager Locks ===");
        esp_pm_dump_locks(stdout);   
#endif
        
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== INICIANDO ENTORNO TEST GENERAL ===");

    /* Inicializar Power Management (Light Sleep Dinámico) */
#if CONFIG_PM_ENABLE
    esp_pm_config_esp32c3_t pm_config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 10,   /* Opcional, baja la velocidad si está ocioso pero no durmiendo */
        .light_sleep_enable = true
    };
    if (esp_pm_configure(&pm_config) == ESP_OK) {
        ESP_LOGI(TAG, "Power Management: Automático Light Sleep HABILIADO!");
    }
#endif

    /* Bajar verbosidad del stack BLE (tags más comunes) */
    esp_log_level_set("NimBLE",     ESP_LOG_WARN);
    esp_log_level_set("NimBLE_GAP", ESP_LOG_WARN);
    esp_log_level_set("BLE_GAP",    ESP_LOG_WARN);
    esp_log_level_set("BLE_GATT",   ESP_LOG_WARN);
    esp_log_level_set("BTDM_INIT",  ESP_LOG_WARN);
    esp_log_level_set("phy_init",   ESP_LOG_WARN);

    /* NVS para persistir modo y settings */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    power_modes_init();
    ESP_LOGI(TAG, "Modo inicial: %s", power_mode_name(power_get_mode()));

    xGuiSemaphore = xSemaphoreCreateMutex();
    xSensorDataMutex = xSemaphoreCreateMutex();

    /* ── FASE 1: I2C y sensores ── */
    ESP_LOGI(TAG, "[Fase 1] I2C + sensores...");
    if (i2c_master_init() != ESP_OK) ESP_LOGE(TAG, "I2C Bus failed!");
    if (gpio_buttons_init() != ESP_OK) ESP_LOGW(TAG, "gpio_buttons_init falló");

    if (max17048_init() != ESP_OK) ESP_LOGW(TAG, "MAX17048 ausente");

    /* BMI160: NO habilitar step counter HW (no irá en producción) */
    if (bmi160_init() != ESP_OK) ESP_LOGE(TAG, "BMI160 init falló");

    if (max30205_init() != ESP_OK) ESP_LOGW(TAG, "MAX30205 ausente");
    if (max30102_init_hrm() != ESP_OK) ESP_LOGW(TAG, "MAX30102 ausente");

    if (ad8232_init_dma() == ESP_OK) {
        ESP_LOGI(TAG, "AD8232 DMA configurado (No iniciado, modo bajo consumo activo)");
    } else {
        ESP_LOGW(TAG, "AD8232 ausente");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    /* ── FASE 2: Display + LVGL ── */
    ESP_LOGI(TAG, "[Fase 2] Display + LVGL...");
    st7789_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    st7789_fill_screen(0x0000);
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, DISP_BUF_SIZE);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 280;
    disp_drv.flush_cb = display_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    build_ui();
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* ── FASE 3: BLE ── */
    ESP_LOGI(TAG, "[Fase 3] BLE...");
    if (ble_telemetry_init() != ESP_OK) ESP_LOGE(TAG, "BLE Stack falló");

    /* Tasks */
    xTaskCreate(gui_task,    "gui_task",    4096, NULL, 5, NULL);
    xTaskCreate(imu_task,    "imu_task",    4096, NULL, 6, NULL);
    xTaskCreate(hrm_task,    "hrm_task",    4096, NULL, 5, NULL);
    xTaskCreate(system_task, "system_task", 4096, NULL, 3, NULL);
    xTaskCreate(ble_tx_task, "ble_tx_task", 4096, NULL, 4, NULL);  /* +1024: el HWM medido era 960 B */
    xTaskCreate(ecg_task,    "ecg_task",    4096, NULL, 7, NULL);
    xTaskCreate(perf_monitor_task, "perf_task", 6144, NULL, 2, NULL); /* vTaskList+RunTimeStats consumen ~3.8 KB */

    ESP_LOGI(TAG, "=== SISTEMA INICIADO ===");
}

#endif

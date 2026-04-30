#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "lvgl.h"
#include "sdl/sdl.h"

#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)

#define BLE_TLV_TYPE_MODE_EVT 1
static uint32_t esp_timer_get_time() { return SDL_GetTicks() * 1000; }

typedef enum { SPOT_QUALITY_GOOD, SPOT_QUALITY_FAIR, SPOT_QUALITY_POOR } max30102_spot_quality_t;
typedef enum { SPOT_STATE_IDLE, SPOT_STATE_SETTLING, SPOT_STATE_MEASURING, SPOT_STATE_DONE, SPOT_STATE_FAILED, SPOT_STATE_ABORTED } max30102_spot_state_t;
typedef struct { max30102_spot_state_t state; uint8_t progress_pct; uint32_t duration_ms; uint8_t bpm; uint8_t spo2; max30102_spot_quality_t quality; } max30102_spot_status_t;
typedef enum { POWER_MODE_SPORT = 0, POWER_MODE_NORMAL, POWER_MODE_SAVER } power_mode_t;
typedef enum { BTN_EVENT_NONE, BTN_EVENT_NEXT_SHORT, BTN_EVENT_NEXT_LONG, BTN_EVENT_SELECT_SHORT, BTN_EVENT_SELECT_LONG } btn_event_t;

static max30102_spot_status_t mock_spot = {SPOT_STATE_IDLE, 0, 0, 0, 0, SPOT_QUALITY_GOOD};
void max30102_spot_get_status(max30102_spot_status_t *st) { *st = mock_spot; }
void max30102_spot_abort() { mock_spot.state = SPOT_STATE_IDLE; }
void max30102_spot_start() { mock_spot.state = SPOT_STATE_MEASURING; mock_spot.progress_pct = 0; }
power_mode_t power_get_mode() { return POWER_MODE_SPORT; }
void power_set_mode(power_mode_t m) {}
const char* power_mode_name(power_mode_t m) { return "SPORT"; }
uint16_t power_get_display_off_s(power_mode_t m) { return 15; }
void power_set_display_off_s(power_mode_t m, uint16_t s) {}
static bool ecg_active = false;
bool ble_telemetry_is_ecg_mode_active() { return ecg_active; }
void ble_telemetry_set_ecg_mode(bool act) { ecg_active = act; }
void ble_tx_push(int t, void* d, int l, int m) {}
void max17048_reset() {}
static const char *TAG = "Test_General";
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

/* PM lock para ECG: bloquea light sleep mientras el ADC continuo está activo,
 * evita que el reloj del APB se reconfigure entre frames del DMA y produzca
 * los escalones cuadrados sobre la traza. */
#if CONFIG_PM_ENABLE
static esp_pm_lock_handle_t s_ecg_pm_lock = NULL;
#endif

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
    // st7789_draw_bitmap(x, y, w, h, (const uint16_t *)color_p);
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
            sensor_data.steps_sw = 0;
            ESP_LOGI(TAG, "Menu: pasos reiniciados");
            break;
        case 3: /* Vincular BLE */
            ESP_LOGI(TAG, "Menu: BLE advertising activo");
            break;
        case 4: /* Apagar */
            ESP_LOGI(TAG, "Menu: deep sleep");
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
                switch_to((ui_screen_t)((current_screen + 1) % SCREEN_CYCLE_COUNT));
            }
            break;
        case BTN_EVENT_NEXT_LONG:
            if (current_screen == SCREEN_MODE || current_screen == SCREEN_SETTINGS) {
                switch_to(SCREEN_MENU);
            } else {
                switch_to((ui_screen_t)((current_screen + SCREEN_CYCLE_COUNT - 1) % SCREEN_CYCLE_COUNT));
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


int main(void) {
    lv_init();
    sdl_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[DISP_BUF_SIZE];
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 280;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);

    build_ui();
    
    // Simulate some sensor data
    sensor_data.steps_sw = 1234;
    sensor_data.battery_soc = 85.0f;
    sensor_data.finger_present = true;
    sensor_data.hr_bpm = 72;
    sensor_data.spo2_pct = 98;
    sensor_data.temperature_c = 36.5f;

    while(1) {
        update_home_screen(&sensor_data);
        update_bio_screen(&sensor_data);
        update_ecg_screen();
        update_hrspot_screen();

        /* Simulación de Botones Físicos usando Teclado PC */
        const Uint8 *state = SDL_GetKeyboardState(NULL);
        static bool select_pressed = false;
        static bool next_pressed = false;
        
        /* Tecla ESPACIO o ENTER para simular SELECT */
        if (state[SDL_SCANCODE_SPACE] || state[SDL_SCANCODE_RETURN]) {
            if (!select_pressed) {
                handle_button(BTN_EVENT_SELECT_SHORT);
                select_pressed = true;
            }
        } else {
            select_pressed = false;
        }

        /* Tecla FLECHA DERECHA o TAB para simular NEXT */
        if (state[SDL_SCANCODE_RIGHT] || state[SDL_SCANCODE_DOWN] || state[SDL_SCANCODE_TAB]) {
            if (!next_pressed) {
                handle_button(BTN_EVENT_NEXT_SHORT);
                next_pressed = true;
            }
        } else {
            next_pressed = false;
        }

        lv_task_handler();
        SDL_Delay(5);
        lv_tick_inc(5);
    }
    return 0;
}

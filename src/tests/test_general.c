#ifdef ENV_TEST_GENERAL

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include "gc9a01.h"
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
#include "ui_theme.h"

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
    SCREEN_THEME,      /* sub-screen */
    SCREEN_COUNT,
} ui_screen_t;
#define SCREEN_CYCLE_COUNT 5  /* HOME..MENU ciclan */

#define MENU_ITEM_COUNT 8
/* Índices de los items que abren sub-pantallas (deben coincidir con el orden) */
#define MENU_IDX_MODE   0
#define MENU_IDX_THEME  1
#define MENU_IDX_OFF    2
#define MENU_IDX_TXIMU  6
static const char *MENU_LABELS[MENU_ITEM_COUNT] = {
    LV_SYMBOL_SETTINGS " Modo Energia",
    LV_SYMBOL_TINT " Tema",
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

#define THEME_ITEM_COUNT UI_THEME_COUNT

/* Valores cíclicos para auto-off (segundos) */
static const uint16_t AUTO_OFF_VALUES[] = {5, 8, 15, 30, 60, 120};
#define AUTO_OFF_VALUES_COUNT (sizeof(AUTO_OFF_VALUES) / sizeof(AUTO_OFF_VALUES[0]))

/* Estado de navegación */
static ui_screen_t current_screen = SCREEN_HOME;
static uint8_t menu_selection = 0;
static uint8_t mode_selection = 0;
static uint8_t settings_selection = 0;
static uint8_t theme_selection = 0;
static int64_t ecg_start_us = 0;

/* PM lock para ECG: bloquea light sleep mientras el ADC continuo está activo,
 * evita que el reloj del APB se reconfigure entre frames del DMA y produzca
 * los escalones cuadrados sobre la traza. */
#if CONFIG_PM_ENABLE
static esp_pm_lock_handle_t s_ecg_pm_lock = NULL;
#endif

/* Tema de color activo. Apunta a la tabla const de ui_theme; se refresca
 * con ui_theme_get() tras un cambio de tema. Todos los builders leen sus
 * colores de aquí, así que un cambio de tema + rebuild re-skinea toda la UI. */
static const ui_theme_t *TH;

/* Screens */
static lv_obj_t *scr_obj[SCREEN_COUNT];

/* Labels Home */
static lv_obj_t *home_clock, *home_steps, *home_bat, *home_bat_arc, *home_hr, *home_act, *home_mode;
/* Labels Bio */
static lv_obj_t *bio_hr, *bio_spo2, *bio_temp, *bio_status, *bio_age_hr, *bio_age_spo2;
/* Labels HRSpot */
static lv_obj_t *hrspot_instr, *hrspot_progress, *hrspot_result, *hrspot_quality;
static lv_obj_t *hrspot_heart, *hrspot_ring;   /* corazón latiente durante la medición */
/* Labels ECG */
static lv_obj_t *ecg_instr, *ecg_timer, *ecg_rec, *ecg_rec_circle;
static lv_obj_t *ecg_wave;       /* línea ECG decorativa (efecto visual) */
#define ECG_PTS 61
static lv_point_t ecg_pts[ECG_PTS];
static float ecg_phase = 0.0f;
/* Menú principal */
static lv_obj_t *menu_rows[MENU_ITEM_COUNT];
/* Sub-menú Mode */
static lv_obj_t *mode_rows[MODE_ITEM_COUNT];
static lv_obj_t *mode_active_label;
/* Sub-menú Settings */
static lv_obj_t *settings_rows[SETTINGS_ITEM_COUNT];
/* Sub-menú Theme */
static lv_obj_t *theme_rows[THEME_ITEM_COUNT];
static lv_obj_t *theme_active_label;

/* Display backlight */
static uint32_t last_activity_ms = 0;
static bool backlight_on = false;

/* Último % de batería animado en el arco. -1 fuerza re-animar (boot/rebuild). */
static int s_last_bat_target = -1;

/* Buffers de dibujo LVGL. Doble-buffer en RAM interna (DMA-safe para el SPI
 * del ST7789) para animaciones sin tearing. Banda de 48 líneas cada uno. */
#define DISP_BUF_SIZE (240 * 48)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[DISP_BUF_SIZE];
static lv_color_t buf_2[DISP_BUF_SIZE];

#include "ui_fonts.h"   /* ui_font_hero_56 / ui_font_value_28 / ui_font_label_16 */

/* Aliases semánticos: hero (reloj/números grandes), value (datos), label (texto). */
#define FONT_HERO   (&ui_font_hero_56)
#define FONT_VALUE  (&ui_font_value_28)
#define FONT_LABEL  (&ui_font_label_16)

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
    gc9a01_draw_bitmap(x, y, w, h, (const uint16_t *)color_p);
    lv_disp_flush_ready(disp_drv);
}

/* ─────────────────── Helpers de construcción ─────────────────── */

static lv_obj_t * create_card(lv_obj_t * parent, int x, int y, int w, int h) {
    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(TH->surface), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static lv_obj_t *make_screen(const char *title) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text_safe(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(t, FONT_LABEL, LV_PART_MAIN);
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
    scr_obj[SCREEN_HOME] = make_screen("SUPACLOCK");
    lv_obj_t *s = scr_obj[SCREEN_HOME];

    home_clock = make_label(s, FONT_HERO, TH->text,
                            LV_ALIGN_TOP_MID, 0, 34, "--:--");

    home_mode  = make_label(s, FONT_LABEL, TH->text_dim,
                            LV_ALIGN_TOP_MID, 0, 100, "MODE: SPORT");

    int card_w = 100;
    int card_h = 66;
    int pad_x = 15;
    int pad_y = 126;
    int gap = 8;

    lv_obj_t * card_steps = create_card(s, pad_x, pad_y, card_w, card_h);
    lv_obj_t * card_bat   = create_card(s, pad_x + card_w + gap, pad_y, card_w, card_h);
    lv_obj_t * card_hr    = create_card(s, pad_x, pad_y + card_h + gap, card_w, card_h);
    lv_obj_t * card_act   = create_card(s, pad_x + card_w + gap, pad_y + card_h + gap, card_w, card_h);

    home_steps = make_label(card_steps, FONT_VALUE, TH->c_steps, LV_ALIGN_CENTER, 0, 0, LV_SYMBOL_LIST " 0");

    home_bat_arc = lv_arc_create(card_bat);
    lv_obj_set_size(home_bat_arc, 60, 60);
    lv_obj_align(home_bat_arc, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_rotation(home_bat_arc, 270);
    lv_arc_set_bg_angles(home_bat_arc, 0, 360);
    lv_obj_remove_style(home_bat_arc, NULL, LV_PART_KNOB); // No knob
    lv_obj_clear_flag(home_bat_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(home_bat_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(home_bat_arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(home_bat_arc, lv_color_hex(TH->surface), LV_PART_MAIN);
    lv_obj_set_style_arc_color(home_bat_arc, lv_color_hex(TH->c_batt), LV_PART_INDICATOR);

    home_bat = make_label(home_bat_arc, FONT_LABEL, TH->text, LV_ALIGN_CENTER, 0, 0, "--%");

    home_hr = make_label(card_hr, FONT_VALUE, TH->c_hr, LV_ALIGN_CENTER, 0, 0, LV_SYMBOL_TINT " --");

    home_act = make_label(card_act, FONT_LABEL, TH->c_activity, LV_ALIGN_CENTER, 0, 0, LV_SYMBOL_CHARGE " --");
}

static void build_bio(void) {
    scr_obj[SCREEN_BIO] = make_screen("BIOMETRIA");
    lv_obj_t *s = scr_obj[SCREEN_BIO];

    bio_hr     = make_label(s, FONT_VALUE, TH->c_hr, LV_ALIGN_TOP_LEFT, 20, 40,  LV_SYMBOL_TINT " -- bpm");
    bio_age_hr = make_label(s, FONT_LABEL, TH->text_dim, LV_ALIGN_TOP_LEFT, 20, 72, "");

    bio_spo2     = make_label(s, FONT_VALUE, TH->c_spo2, LV_ALIGN_TOP_LEFT, 20, 104, LV_SYMBOL_TINT " --%");
    bio_age_spo2 = make_label(s, FONT_LABEL, TH->text_dim, LV_ALIGN_TOP_LEFT, 20, 136, "");

    bio_temp   = make_label(s, FONT_VALUE, TH->c_temp, LV_ALIGN_TOP_LEFT, 20, 168, LV_SYMBOL_WARNING " --.- C");
    bio_status = make_label(s, FONT_LABEL, TH->ok, LV_ALIGN_TOP_LEFT, 20, 212, "Estado: --");
}

static void build_hrspot(void) {
    scr_obj[SCREEN_HRSPOT] = make_screen("MEDIDA HR/SPO2");
    lv_obj_t *s = scr_obj[SCREEN_HRSPOT];

    hrspot_instr = make_label(s, FONT_LABEL, TH->text_dim,
                              LV_ALIGN_TOP_MID, 0, 50,
                              "Apoye el dedo sobre\nel sensor MAX30102.\n\n"
                              "Pulse SELECT para iniciar.");
    lv_obj_set_style_text_align(hrspot_instr, LV_TEXT_ALIGN_CENTER, 0);

    /* Pulse ring detrás del corazón */
    hrspot_ring = lv_obj_create(s);
    lv_obj_set_size(hrspot_ring, 44, 44);
    lv_obj_align(hrspot_ring, LV_ALIGN_CENTER, 0, -38);
    lv_obj_set_style_radius(hrspot_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(hrspot_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hrspot_ring, 4, 0);
    lv_obj_set_style_border_color(hrspot_ring, lv_color_hex(TH->c_hr), 0);
    lv_obj_add_flag(hrspot_ring, LV_OBJ_FLAG_HIDDEN);

    /* Corazón latiente (icon font), visible sólo durante la medición. */
    hrspot_heart = make_label(s, &ui_font_icon_56, TH->c_hr,
                              LV_ALIGN_CENTER, 0, -38, UI_SYM_HEART);
    lv_obj_add_flag(hrspot_heart, LV_OBJ_FLAG_HIDDEN);

    hrspot_progress = make_label(s, FONT_LABEL, TH->warn,
                                 LV_ALIGN_CENTER, 0, 38, "");
    hrspot_result   = make_label(s, FONT_VALUE, TH->text,
                                 LV_ALIGN_CENTER, 0, 0, "");
    hrspot_quality  = make_label(s, FONT_LABEL, TH->text_dim,
                                 LV_ALIGN_BOTTOM_MID, 0, -20, "");
}

static void build_ecg(void) {
    scr_obj[SCREEN_ECG] = make_screen("MODO ECG");
    lv_obj_t *s = scr_obj[SCREEN_ECG];

    ecg_instr = make_label(s, FONT_LABEL, TH->text_dim,
                           LV_ALIGN_TOP_MID, 0, 50,
                           "Presione los electrodos\nlaterales con la mano\nopuesta.\n\n"
                           "Pulsa SELECT para iniciar");
    lv_obj_set_style_text_align(ecg_instr, LV_TEXT_ALIGN_CENTER, 0);

    ecg_timer = make_label(s, FONT_HERO, TH->text,
                           LV_ALIGN_TOP_MID, 0, 40, "0:00");
    lv_obj_add_flag(ecg_timer, LV_OBJ_FLAG_HIDDEN);

    /* Onda ECG decorativa (efecto visual, NO la señal real). */
    ecg_wave = lv_line_create(s);
    lv_obj_set_size(ecg_wave, 220, 90);
    lv_obj_align(ecg_wave, LV_ALIGN_CENTER, 0, 28);
    lv_obj_set_style_line_color(ecg_wave, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_line_width(ecg_wave, 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(ecg_wave, true, LV_PART_MAIN);
    for (int i = 0; i < ECG_PTS; i++) { ecg_pts[i].x = (lv_coord_t)(i * 220 / (ECG_PTS - 1)); ecg_pts[i].y = 45; }
    lv_line_set_points(ecg_wave, ecg_pts, ECG_PTS);
    lv_obj_add_flag(ecg_wave, LV_OBJ_FLAG_HIDDEN);

    ecg_rec = make_label(s, FONT_VALUE, TH->alert,
                         LV_ALIGN_BOTTOM_MID, 12, -20, "REC");
    lv_obj_add_flag(ecg_rec, LV_OBJ_FLAG_HIDDEN);

    ecg_rec_circle = lv_obj_create(s);
    lv_obj_set_size(ecg_rec_circle, 14, 14);
    lv_obj_align_to(ecg_rec_circle, ecg_rec, LV_ALIGN_OUT_LEFT_MID, -6, 0);
    lv_obj_set_style_radius(ecg_rec_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ecg_rec_circle, lv_color_hex(TH->alert), 0);
    lv_obj_set_style_border_width(ecg_rec_circle, 0, 0);
    lv_obj_add_flag(ecg_rec_circle, LV_OBJ_FLAG_HIDDEN);
}

static void build_menu(void) {
    scr_obj[SCREEN_MENU] = make_screen("MENU");
    lv_obj_t *s = scr_obj[SCREEN_MENU];
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        menu_rows[i] = lv_label_create(s);
        lv_obj_set_style_text_font(menu_rows[i], FONT_LABEL, LV_PART_MAIN);
        lv_label_set_text_safe(menu_rows[i], MENU_LABELS[i]);
        lv_obj_set_width(menu_rows[i], 220);
        lv_obj_set_style_pad_all(menu_rows[i], 6, LV_PART_MAIN);
        lv_obj_set_style_radius(menu_rows[i], 6, LV_PART_MAIN);
        lv_obj_align(menu_rows[i], LV_ALIGN_TOP_MID, 0, 35 + i * 40);
    }
}

static void build_mode(void) {
    scr_obj[SCREEN_MODE] = make_screen("MODO ENERGIA");
    lv_obj_t *s = scr_obj[SCREEN_MODE];

    mode_active_label = make_label(s, FONT_LABEL, TH->ok,
                                   LV_ALIGN_TOP_MID, 0, 30, "Activo: SPORT");

    for (int i = 0; i < MODE_ITEM_COUNT; i++) {
        mode_rows[i] = lv_label_create(s);
        lv_obj_set_style_text_font(mode_rows[i], FONT_LABEL, LV_PART_MAIN);
        lv_label_set_text_safe(mode_rows[i], MODE_LABELS[i]);
        lv_obj_set_width(mode_rows[i], 220);
        lv_obj_set_style_pad_all(mode_rows[i], 8, LV_PART_MAIN);
        lv_obj_set_style_radius(mode_rows[i], 6, LV_PART_MAIN);
        lv_obj_align(mode_rows[i], LV_ALIGN_TOP_MID, 0, 60 + i * 55);
    }

    /* Hint inferior */
    make_label(s, FONT_LABEL, TH->text_dim, LV_ALIGN_BOTTOM_MID, 0, -8,
               "SELECT: aplicar  L_NEXT: salir");
}

static void build_settings(void) {
    scr_obj[SCREEN_SETTINGS] = make_screen("AUTO-OFF PANT.");
    lv_obj_t *s = scr_obj[SCREEN_SETTINGS];

    for (int i = 0; i < SETTINGS_ITEM_COUNT; i++) {
        settings_rows[i] = lv_label_create(s);
        lv_obj_set_style_text_font(settings_rows[i], FONT_LABEL, LV_PART_MAIN);
        lv_obj_set_width(settings_rows[i], 220);
        lv_obj_set_style_pad_all(settings_rows[i], 8, LV_PART_MAIN);
        lv_obj_set_style_radius(settings_rows[i], 6, LV_PART_MAIN);
        lv_obj_align(settings_rows[i], LV_ALIGN_TOP_MID, 0, 50 + i * 55);
    }
    make_label(s, FONT_LABEL, TH->text_dim, LV_ALIGN_BOTTOM_MID, 0, -8,
               "SELECT: cambiar  L_NEXT: salir");
}

static void build_theme(void) {
    scr_obj[SCREEN_THEME] = make_screen("TEMA");
    lv_obj_t *s = scr_obj[SCREEN_THEME];

    theme_active_label = make_label(s, FONT_LABEL, TH->ok,
                                    LV_ALIGN_TOP_MID, 0, 30, "Activo: --");

    for (int i = 0; i < THEME_ITEM_COUNT; i++) {
        theme_rows[i] = lv_label_create(s);
        lv_obj_set_style_text_font(theme_rows[i], FONT_LABEL, LV_PART_MAIN);
        lv_label_set_text_safe(theme_rows[i], ui_theme_name((ui_theme_id_t)i));
        lv_obj_set_width(theme_rows[i], 220);
        lv_obj_set_style_pad_all(theme_rows[i], 8, LV_PART_MAIN);
        lv_obj_set_style_radius(theme_rows[i], 6, LV_PART_MAIN);
        lv_obj_align(theme_rows[i], LV_ALIGN_TOP_MID, 0, 60 + i * 50);
    }
    make_label(s, FONT_LABEL, TH->text_dim, LV_ALIGN_BOTTOM_MID, 0, -8,
               "SELECT: aplicar  L_NEXT: salir");
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
            lv_obj_set_style_bg_color(rows[i], lv_color_hex(TH->accent), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(rows[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_text_color(rows[i], lv_color_hex(TH->bg), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_opa(rows[i], LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_text_color(rows[i], lv_color_hex(TH->text_dim), LV_PART_MAIN);
        }
    }
}

static void render_settings_labels(void) {
    for (int i = 0; i < SETTINGS_ITEM_COUNT; i++) {
        uint16_t v = power_get_display_off_s((power_mode_t)i);
        lv_label_set_text_fmt_safe(settings_rows[i], "%s: %us", SETTINGS_LABELS[i], v);
    }
}

static void render_theme_active(void) {
    lv_label_set_text_fmt_safe(theme_active_label, "Activo: %s", ui_theme_name(ui_theme_get_id()));
}

static void render_mode_active(void) {
    power_mode_t m = power_get_mode();
    lv_label_set_text_fmt_safe(mode_active_label, "Activo: %s", power_mode_name(m));
}

/* ─────────────────── Actualizadores por pantalla ─────────────────── */

/* Anima el valor de un arco (ease-out) en vez de saltar. Para motion sutil. */
static void anim_arc_exec_cb(void *obj, int32_t v) {
    lv_arc_set_value((lv_obj_t *)obj, v);
}
static void animate_arc_to(lv_obj_t *arc, int32_t to) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, anim_arc_exec_cb);
    lv_anim_set_values(&a, lv_arc_get_value(arc), to);
    lv_anim_set_time(&a, 400);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

/* ── Corazón latiente (HRSPOT) ── */
/* Pulsa el zoom del glifo corazón como un latido (contracción rápida,
 * relajación lenta). transform_zoom: 256 = 1x. */
static void heart_opa_cb(void *obj, int32_t v) {
    lv_obj_set_style_text_opa((lv_obj_t *)obj, v, LV_PART_MAIN);
}
static void heart_ring_size_cb(void *obj, int32_t v) {
    lv_obj_set_size((lv_obj_t *)obj, v, v);
}
static void heart_ring_opa_cb(void *obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)obj, v, LV_PART_MAIN);
}


static bool s_heart_beating = false;
static void heart_start_beat(void) {
    if (s_heart_beating) return;
    s_heart_beating = true;
    
    // Instead of transform_zoom (which fails memory allocation for large glyphs and disappears),
    // we pulse the heart opacity and emphasize the ring.
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, hrspot_heart);
    lv_anim_set_exec_cb(&a, heart_opa_cb);
    lv_anim_set_values(&a, 150, 255);          /* Pulse opacity */
    lv_anim_set_time(&a, 150);                  /* attack 150ms */
    lv_anim_set_playback_time(&a, 600);         /* release 600ms */
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_t rs;
    lv_anim_init(&rs);
    lv_anim_set_var(&rs, hrspot_ring);
    lv_anim_set_exec_cb(&rs, heart_ring_size_cb);
    lv_anim_set_values(&rs, 40, 140);           /* Emphasize ring size */
    lv_anim_set_time(&rs, 750);
    lv_anim_set_repeat_count(&rs, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&rs, lv_anim_path_ease_out);
    lv_anim_start(&rs);

    lv_anim_t ro;
    lv_anim_init(&ro);
    lv_anim_set_var(&ro, hrspot_ring);
    lv_anim_set_exec_cb(&ro, heart_ring_opa_cb);
    lv_anim_set_values(&ro, 255, 0);            /* Emphasize ring opacity */
    lv_anim_set_time(&ro, 750);
    lv_anim_set_repeat_count(&ro, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&ro, lv_anim_path_ease_in);
    lv_anim_start(&ro);
}
static void heart_stop_beat(void) {
    if (!s_heart_beating) return;
    s_heart_beating = false;
    lv_anim_del(hrspot_heart, heart_opa_cb);
    lv_obj_set_style_text_opa(hrspot_heart, 255, LV_PART_MAIN);

    lv_anim_del(hrspot_ring, heart_ring_size_cb);
    lv_anim_del(hrspot_ring, heart_ring_opa_cb);
    lv_obj_set_size(hrspot_ring, 44, 44);
    lv_obj_set_style_opa(hrspot_ring, 0, LV_PART_MAIN);
}

/* ── Onda ECG decorativa (no es la señal real) ── */
/* Morfología PQRST aproximada; p en [0,1) → deflexión ~[-0.3, 1.0]. */
static float ecg_morph(float p) {
    float y = 0.0f;
    y += 0.12f * expf(-powf((p - 0.12f) / 0.030f, 2.0f));  /* P */
    y -= 0.10f * expf(-powf((p - 0.22f) / 0.012f, 2.0f));  /* Q */
    y += 1.00f * expf(-powf((p - 0.25f) / 0.012f, 2.0f));  /* R */
    y -= 0.25f * expf(-powf((p - 0.28f) / 0.014f, 2.0f));  /* S */
    y += 0.22f * expf(-powf((p - 0.45f) / 0.050f, 2.0f));  /* T */
    return y;
}
static void ecg_wave_update(void) {
    static uint32_t last_ms = 0;
    uint32_t now = now_ms();
    if (last_ms == 0) last_ms = now;
    
    const float W = 220.0f, cycles = 2.0f, amp = 34.0f;
    const float cy = 45.0f;
    ecg_phase += (now - last_ms) * 0.0006f;
    last_ms = now;
    if (ecg_phase > 1.0f) ecg_phase -= 1.0f;
    for (int i = 0; i < ECG_PTS; i++) {
        float fx = (float)i / (ECG_PTS - 1);
        float p = fx * cycles + ecg_phase;
        p -= (float)(int)p;                    /* frac */
        ecg_pts[i].x = (lv_coord_t)(fx * W);
        ecg_pts[i].y = (lv_coord_t)(cy - ecg_morph(p) * amp);
    }
    lv_line_set_points(ecg_wave, ecg_pts, ECG_PTS);
}

static void update_home_screen(const shared_sensor_data_t *d) {
    uint32_t s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    lv_label_set_text_fmt_safe(home_clock, "%lu:%02lu",
                          (unsigned long)((s / 60) % 100),
                          (unsigned long)(s % 60));

    lv_label_set_text_fmt_safe(home_mode, "MODE: %s", power_mode_name(power_get_mode()));
    lv_label_set_text_fmt_safe(home_steps, LV_SYMBOL_LIST " %lu", (unsigned long)d->steps_sw);

    int bat_pct = (int)d->battery_soc;
    if (bat_pct < 0) bat_pct = 0;
    if (bat_pct > 100) bat_pct = 100;
    /* Anima el arco sólo cuando el % cambia (batería se actualiza c/30s),
     * así no peleamos con la cadencia de frames. */
    if (bat_pct != s_last_bat_target) {
        animate_arc_to(home_bat_arc, bat_pct);
        s_last_bat_target = bat_pct;
    }
    lv_label_set_text_fmt_safe(home_bat, "%d%%", bat_pct);
    uint32_t bat_color = (bat_pct > 20) ? TH->c_batt : TH->alert;
    lv_obj_set_style_arc_color(home_bat_arc, lv_color_hex(bat_color), LV_PART_INDICATOR);

    if (d->finger_present && d->hr_bpm > 0) {
        lv_label_set_text_fmt_safe(home_hr, LV_SYMBOL_TINT " %u", d->hr_bpm);
    } else {
        lv_label_set_text_safe(home_hr, LV_SYMBOL_TINT " --");
    }

    int32_t amag = (int32_t)d->ax * d->ax + (int32_t)d->ay * d->ay + (int32_t)d->az * d->az;
    if (amag > 300000000L) {
        lv_label_set_text_safe(home_act, LV_SYMBOL_CHARGE " Activo");
    } else {
        lv_label_set_text_safe(home_act, LV_SYMBOL_CHARGE " Reposo");
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
        lv_label_set_text_fmt_safe(bio_hr, LV_SYMBOL_TINT " %u bpm", d->hr_bpm);
        format_age(age, sizeof(age), d->hr_updated_ms);
        lv_label_set_text_safe(bio_age_hr, age);
    } else {
        lv_label_set_text_safe(bio_hr, LV_SYMBOL_TINT " -- bpm");
        lv_label_set_text_safe(bio_age_hr, "");
    }

    if (d->spo2_pct > 0) {
        lv_label_set_text_fmt_safe(bio_spo2, LV_SYMBOL_TINT " %u%%", d->spo2_pct);
        format_age(age, sizeof(age), d->spo2_updated_ms);
        lv_label_set_text_safe(bio_age_spo2, age);
    } else {
        lv_label_set_text_safe(bio_spo2, LV_SYMBOL_TINT " --%");
        lv_label_set_text_safe(bio_age_spo2, "");
    }

    int t_int  = (int)d->temperature_c;
    int t_frac = (int)((d->temperature_c - t_int) * 10);
    if (t_frac < 0) t_frac = -t_frac;
    lv_label_set_text_fmt_safe(bio_temp, LV_SYMBOL_WARNING " %d.%d C", t_int, t_frac);

    const char *st;
    uint32_t col;
    if (!d->finger_present)        { st = "Estado: Sin dedo";  col = TH->text_dim; }
    else if (d->hr_bpm > 100)      { st = "Estado: Alto";      col = TH->warn; }
    else if (d->hr_bpm > 0)        { st = "Estado: Normal";    col = TH->ok; }
    else                           { st = "Estado: Midiendo";  col = TH->warn; }
    
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

    bool measuring = (st.state == SPOT_STATE_SETTLING || st.state == SPOT_STATE_MEASURING);
    if (measuring) {
        lv_obj_clear_flag(hrspot_heart, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(hrspot_ring, LV_OBJ_FLAG_HIDDEN);
        heart_start_beat();
    } else {
        heart_stop_beat();
        lv_obj_add_flag(hrspot_heart, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(hrspot_ring, LV_OBJ_FLAG_HIDDEN);
    }

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

static void ecg_rec_opa_cb(void *obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)obj, v, LV_PART_MAIN);
}
static bool s_ecg_rec_beating = false;
static void ecg_start_rec_anim(void) {
    if (s_ecg_rec_beating) return;
    s_ecg_rec_beating = true;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ecg_rec_circle);
    lv_anim_set_exec_cb(&a, ecg_rec_opa_cb);
    lv_anim_set_values(&a, 255, 60);
    lv_anim_set_time(&a, 600);
    lv_anim_set_playback_time(&a, 600);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}
static void ecg_stop_rec_anim(void) {
    if (!s_ecg_rec_beating) return;
    s_ecg_rec_beating = false;
    lv_anim_del(ecg_rec_circle, ecg_rec_opa_cb);
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
        lv_obj_clear_flag(ecg_rec_circle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ecg_wave, LV_OBJ_FLAG_HIDDEN);
        ecg_start_rec_anim();
        ecg_wave_update();    /* avanza la onda decorativa */
    } else {
        ecg_start_us = 0;
        lv_obj_clear_flag(ecg_instr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ecg_timer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ecg_rec, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ecg_rec_circle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ecg_wave, LV_OBJ_FLAG_HIDDEN);
        ecg_stop_rec_anim();
    }
}

/* ─────────────────── Navegación & acciones ─────────────────── */

static void switch_to(ui_screen_t s) {
    if (s == current_screen) return;
    
    /* Decidir dirección de animación: por defecto hacia adelante (pantalla nueva entra por la derecha -> MOVE_LEFT).
       Si retrocedemos, la pantalla entra por la izquierda -> MOVE_RIGHT. */
    lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;
    
    if (s == (current_screen + SCREEN_CYCLE_COUNT - 1) % SCREEN_CYCLE_COUNT) {
        anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    } else if (s < current_screen && !(current_screen == SCREEN_CYCLE_COUNT - 1 && s == 0)) {
        anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    }
    
    current_screen = s;
    /* Animación de 150 ms para que el deslizamiento se vea fluido. */
    lv_scr_load_anim(scr_obj[s], anim, 120, 0, false);
    if (s == SCREEN_MENU)     render_list_selection(menu_rows,     MENU_ITEM_COUNT,     menu_selection, 35, 40, 5);
    if (s == SCREEN_MODE)     { render_mode_active(); render_list_selection(mode_rows, MODE_ITEM_COUNT, mode_selection, 60, 55, 4); }
    if (s == SCREEN_SETTINGS) { render_settings_labels(); render_list_selection(settings_rows, SETTINGS_ITEM_COUNT, settings_selection, 50, 55, 4); }
    if (s == SCREEN_THEME)    { render_theme_active(); render_list_selection(theme_rows, THEME_ITEM_COUNT, theme_selection, 60, 50, 4); }
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

static void rebuild_ui(void);   /* fwd: reconstruye la UI con el tema activo */

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

/* Aplica el tema seleccionado y re-skinea toda la UI. rebuild_ui recarga la
 * pantalla TEMA con el nuevo look. */
static void apply_selected_theme(void) {
    if ((ui_theme_id_t)theme_selection == ui_theme_get_id()) return;
    ui_theme_set((ui_theme_id_t)theme_selection);
    rebuild_ui();
}

static void menu_execute_selected(void) {
    switch (menu_selection) {
        case 0: /* Modo Energía */
            mode_selection = (uint8_t)power_get_mode();
            switch_to(SCREEN_MODE);
            break;
        case 1: /* Tema */
            theme_selection = (uint8_t)ui_theme_get_id();
            switch_to(SCREEN_THEME);
            break;
        case 2: /* Auto-off Pantalla */
            settings_selection = 0;
            switch_to(SCREEN_SETTINGS);
            break;
        case 3: /* Reiniciar Pasos */
            if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                sensor_data.steps_sw = 0;
                xSemaphoreGive(xSensorDataMutex);
            }
            ESP_LOGI(TAG, "Menu: pasos reiniciados");
            break;
        case 4: /* Vincular BLE */
            ESP_LOGI(TAG, "Menu: BLE advertising activo");
            break;
        case 5: /* Apagar */
            ESP_LOGI(TAG, "Menu: deep sleep");
            vTaskDelay(pdMS_TO_TICKS(200));
            /* Wake-up por BTN_SELECT (activo en bajo).
             *
             * C3/C6/H2 (RISC-V): API "GPIO wakeup" sirve para cualquier pin.
             * S3 (Xtensa): solo los RTC-GPIOs (0..21) despiertan de deep
             *              sleep → usamos ext1 con TRIGGER_LOW. BTN_SELECT
             *              en el carrier es GPIO8, que SÍ es RTC.
             */
#if CONFIG_IDF_TARGET_ESP32S3
            esp_sleep_enable_ext1_wakeup_io((1ULL << BTN_SELECT_PIN),
                                            ESP_EXT1_WAKEUP_ANY_LOW);
#else
            esp_deep_sleep_enable_gpio_wakeup((1ULL << BTN_SELECT_PIN),
                                              ESP_GPIO_WAKEUP_GPIO_LOW);
#endif
            esp_deep_sleep_start();
            break;
        case 6: /* Tx IMU BLE */
            imu_ble_tx_enabled = !imu_ble_tx_enabled;
            lv_label_set_text_safe(menu_rows[MENU_IDX_TXIMU],
                                   imu_ble_tx_enabled ? LV_SYMBOL_WIFI " Tx IMU: ON"
                                                      : LV_SYMBOL_WIFI " Tx IMU: OFF");
            ESP_LOGI(TAG, "Menu: Tx IMU BLE = %s", imu_ble_tx_enabled ? "ON" : "OFF");
            break;
        case 7: /* Reset Bateria */
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
            } else if (current_screen == SCREEN_THEME) {
                theme_selection = (theme_selection + 1) % THEME_ITEM_COUNT;
                render_list_selection(theme_rows, THEME_ITEM_COUNT, theme_selection, 60, 55, 4);
            } else {
                switch_to((current_screen + 1) % SCREEN_CYCLE_COUNT);
            }
            break;
        case BTN_EVENT_NEXT_LONG:
            if (current_screen == SCREEN_MODE || current_screen == SCREEN_SETTINGS
                || current_screen == SCREEN_THEME) {
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
            } else if (current_screen == SCREEN_THEME) {
                apply_selected_theme();
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

static void build_all_screens(void) {
    TH = ui_theme_get();    /* todos los builders leen colores de TH */
    build_home();
    build_bio();
    build_hrspot();
    build_ecg();
    build_menu();
    build_mode();
    build_settings();
    build_theme();
    render_list_selection(menu_rows, MENU_ITEM_COUNT, menu_selection, 35, 40, 5);
    render_list_selection(mode_rows, MODE_ITEM_COUNT, mode_selection, 60, 55, 4);
    render_list_selection(settings_rows, SETTINGS_ITEM_COUNT, settings_selection, 50, 55, 4);
    render_list_selection(theme_rows, THEME_ITEM_COUNT, theme_selection, 60, 50, 4);
    render_settings_labels();
    render_mode_active();
    render_theme_active();
}

static void build_ui(void) {
    build_all_screens();
    lv_scr_load(scr_obj[SCREEN_HOME]);
}

/* Reconstruye todas las pantallas con el tema activo y recarga la pantalla
 * actual. Se llama tras cambiar de tema. Carga una pantalla temporal para no
 * borrar un scr_obj que esté activo. */
static void rebuild_ui(void) {
    ui_screen_t keep = current_screen;
    lv_obj_t *tmp = lv_obj_create(NULL);
    lv_scr_load(tmp);
    for (int i = 0; i < SCREEN_COUNT; i++) {
        if (scr_obj[i]) { lv_obj_del(scr_obj[i]); scr_obj[i] = NULL; }
    }
    build_all_screens();
    s_last_bat_target = -1;   /* el arco nuevo arranca en 0; re-animar al valor real */
    s_heart_beating = false;  /* el corazón viejo fue borrado; resetear el flag */
    lv_scr_load(scr_obj[keep]);
    lv_obj_del(tmp);
}

/* ═══════════════════════════════════════════════════════════════════
 *  TASKS
 * ═══════════════════════════════════════════════════════════════════ */

void gui_task(void *pvParameter) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    gc9a01_set_brightness(power_get_display_brightness(power_get_mode()));
    backlight_on = true;
    last_activity_ms = now_ms();

    /* Frames de inactividad acumulados (frame ≈ 33 ms) */
    while (1) {
        uint32_t time_till_next = 100;
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {

            btn_event_t ev;
            bool action_taken = false;
            while ((ev = gpio_buttons_poll()) != BTN_EVENT_NONE) {
                action_taken = true;
                last_activity_ms = now_ms();
                if (!backlight_on) {
                    gc9a01_set_brightness(power_get_display_brightness(power_get_mode()));
                    backlight_on = true;
    last_activity_ms = now_ms();
                } else {
                    handle_button(ev);
                }
            }

            if (!action_taken) {
                /* auto-off según modo activo */
                uint16_t off_s = power_get_display_off_s(power_get_mode());
                if (now_ms() - last_activity_ms >= off_s * 1000 && backlight_on) {
                    gc9a01_set_brightness(0);
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

                time_till_next = lv_timer_handler();
            }
            xSemaphoreGive(xGuiSemaphore);
        }
        
        uint32_t delay_ms;
        if (backlight_on) {
            delay_ms = time_till_next;
            if (delay_ms < 5) delay_ms = 5;
            if (delay_ms > 30) delay_ms = 30;
        } else {
            delay_ms = 100;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
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
#if CONFIG_PM_ENABLE
                if (s_ecg_pm_lock) esp_pm_lock_release(s_ecg_pm_lock);
#endif
            }
            vTaskDelay(pdMS_TO_TICKS(500)); /* Si no hay ECG, dormir profundamente este hilo */
            chunk_idx = 0; sum = 0; count = 0;
            continue;
        } else if (!is_dma_running) {
#if CONFIG_PM_ENABLE
            if (s_ecg_pm_lock) esp_pm_lock_acquire(s_ecg_pm_lock);
#endif
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

    /* Inicializar Power Management (Light Sleep Dinámico).
     * esp_pm_config_t es el tipo portable en IDF 5.x; en S3 podemos
     * subir el máximo a 240 MHz, en C3 nos quedamos en 160 MHz. */
#if CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {
#if CONFIG_IDF_TARGET_ESP32S3
        .max_freq_mhz = 240,
        .min_freq_mhz = 80,
#else
        .max_freq_mhz = 160,
        .min_freq_mhz = 10,
#endif
        .light_sleep_enable = true
    };
    if (esp_pm_configure(&pm_config) == ESP_OK) {
        ESP_LOGI(TAG, "Power Management: Automático Light Sleep HABILIADO!");
    }
    if (esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "ecg", &s_ecg_pm_lock) != ESP_OK) {
        ESP_LOGW(TAG, "ECG PM lock no se pudo crear — light sleep podría meter ruido al ECG");
        s_ecg_pm_lock = NULL;
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
    ui_theme_init();
    ESP_LOGI(TAG, "Modo inicial: %s, tema: %s",
             power_mode_name(power_get_mode()), ui_theme_name(ui_theme_get_id()));

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
    gc9a01_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    gc9a01_fill_screen(0x0000);
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf_1, buf_2, DISP_BUF_SIZE);
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

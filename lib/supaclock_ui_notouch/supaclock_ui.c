#include "supaclock_ui_notouch.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "gc9a01.h"
#include "cst816s.h"
#include "max30102.h"
#include "ble_telemetry.h"
#include "power_modes.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "app_state.h"

static const char *TAG = "SupaClock_UI";

static ui_actions_t s_ui_actions = {0};

void ui_set_actions(const ui_actions_t *actions) {
    if (actions) {
        s_ui_actions = *actions;
    }
}

/* ───────────────────────── Navigation & Screens ───────────────────────── */
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
#define SCREEN_CYCLE_COUNT 5  /* HOME..MENU cycle */

#define MENU_ITEM_COUNT 8
/* Indices of items that open sub-screens */
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

/* Cyclic values for auto-off (seconds) */
static const uint16_t AUTO_OFF_VALUES[] = {5, 8, 15, 30, 60, 120};
#define AUTO_OFF_VALUES_COUNT (sizeof(AUTO_OFF_VALUES) / sizeof(AUTO_OFF_VALUES[0]))

/* Navigation State */
static ui_screen_t current_screen = SCREEN_HOME;
static uint8_t menu_selection = 0;
static uint8_t mode_selection = 0;
static uint8_t settings_selection = 0;
static uint8_t theme_selection = 0;
static int64_t ecg_start_us = 0;

/* Active Color Theme */
static const ui_theme_t *TH;

/* Screen objects */
static lv_obj_t *scr_obj[SCREEN_COUNT];

/* Home Labels */
static lv_obj_t *home_clock, *home_steps, *home_bat, *home_bat_arc, *home_hr, *home_act, *home_mode;
/* Bio Labels */
static lv_obj_t *bio_hr, *bio_spo2, *bio_temp, *bio_status, *bio_age_hr, *bio_age_spo2;
/* HRSpot Labels */
static lv_obj_t *hrspot_instr, *hrspot_progress, *hrspot_result, *hrspot_quality;
static lv_obj_t *hrspot_heart, *hrspot_ring;   /* Beating heart during measurement */
/* ECG Labels */
static lv_obj_t *ecg_instr, *ecg_timer, *ecg_rec, *ecg_rec_circle;
static lv_obj_t *ecg_wave;       /* Decorative wave line */
#define ECG_PTS 61
static lv_point_t ecg_pts[ECG_PTS];
static float ecg_phase = 0.0f;
/* Main Menu rows */
static lv_obj_t *menu_rows[MENU_ITEM_COUNT];
/* Sub-menu Mode rows */
static lv_obj_t *mode_rows[MODE_ITEM_COUNT];
static lv_obj_t *mode_active_label;
/* Sub-menu Settings rows */
static lv_obj_t *settings_rows[SETTINGS_ITEM_COUNT];
/* Sub-menu Theme rows */
static lv_obj_t *theme_rows[THEME_ITEM_COUNT];
static lv_obj_t *theme_active_label;

/* Last battery % animated. -1 forces re-animation (boot/rebuild) */
static int s_last_bat_target = -1;

/* LVGL Draw Buffers */
#define DISP_BUF_SIZE (240 * 48)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[DISP_BUF_SIZE];
static lv_color_t buf_2[DISP_BUF_SIZE];

/* Aliases for Fonts */
#define FONT_HERO   (&ui_font_hero_56)
#define FONT_VALUE  (&ui_font_value_28)
#define FONT_LABEL  (&ui_font_label_16)

/* ── Utilities ── */
static inline uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

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

/* ── Touch activity flag ──
 * Escrito desde touch_read_cb (contexto de la tarea GUI, no ISR) y leído
 * desde gui_task en supaclock_app.c. atomic_bool alcanza porque ambos
 * corren en tareas y no hay ISR involucrado. */
static atomic_bool s_touch_activity = ATOMIC_VAR_INIT(false);

void ui_notify_touch_activity(void) {
    atomic_store(&s_touch_activity, true);
}

bool ui_take_and_clear_touch_activity(void) {
    return atomic_exchange(&s_touch_activity, false);
}

static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    cst816s_touch_t t;
    cst816s_read(&t);
    if (t.pressed) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = t.x;
        data->point.y = t.y;
        ui_notify_touch_activity();
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

/* ── Screen Builders ── */
static lv_obj_t *make_screen(const char *title) {
    lv_obj_t *scr = lv_obj_create(NULL);
    /* Panel Waveshare Ø240 redondo, framebuffer 240x240 rectangular. NO usamos
     * clip_corner + radius CIRCLE: con eso LVGL nunca invalida las 4 esquinas
     * y la RAM del panel se queda con basura (blancos, ruido). Mejor pintar
     * el rect completo con TH->bg — las esquinas caen fuera del bezel y no se
     * ven, pero el framebuffer queda inicializado limpio. */
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text_safe(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(t, FONT_LABEL, LV_PART_MAIN);
    /* Título más adentro para no chocar con el bezel curvo (arco superior). */
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 18);
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

static void build_home(void) {
    scr_obj[SCREEN_HOME] = make_screen("SUPACLOCK");
    lv_obj_t *s = scr_obj[SCREEN_HOME];

    home_clock = make_label(s, FONT_HERO, TH->text,
                            LV_ALIGN_TOP_MID, 0, 46, "--:--");

    home_mode  = make_label(s, FONT_LABEL, TH->text_dim,
                            LV_ALIGN_TOP_MID, 0, 110, "MODE: SPORT");

    /* Cards 2x2 apretadas al disco Ø240. Restricción: la esquina BR de la row2
     * debe caer dentro de r=120 desde (120,120). Con card 72x36, col_ofs=39,
     * row2_y=+72: corner en (43, 210), dist=sqrt(77²+90²)=118.4 → adentro con
     * ~2 px de aire respecto al bezel. */
    int card_w = 72;
    int card_h = 36;
    int gap    = 6;
    int col_ofs = (card_w + gap) / 2;   /* ±39 desde el centro */
    int row1_y  = 30;
    int row2_y  = row1_y + card_h + gap;  /* +72 */

    lv_obj_t *card_steps = lv_obj_create(s);
    lv_obj_t *card_bat   = lv_obj_create(s);
    lv_obj_t *card_hr    = lv_obj_create(s);
    lv_obj_t *card_act   = lv_obj_create(s);
    lv_obj_t *cards[4] = {card_steps, card_bat, card_hr, card_act};
    for (int i = 0; i < 4; i++) {
        lv_obj_set_size(cards[i], card_w, card_h);
        lv_obj_set_style_bg_color(cards[i], lv_color_hex(TH->surface), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(cards[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(cards[i], 10, LV_PART_MAIN);
        lv_obj_set_style_border_width(cards[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(cards[i], 0, LV_PART_MAIN);
        lv_obj_clear_flag(cards[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_align(card_steps, LV_ALIGN_CENTER, -col_ofs, row1_y);
    lv_obj_align(card_bat,   LV_ALIGN_CENTER, +col_ofs, row1_y);
    lv_obj_align(card_hr,    LV_ALIGN_CENTER, -col_ofs, row2_y);
    lv_obj_align(card_act,   LV_ALIGN_CENTER, +col_ofs, row2_y);

    home_steps = make_label(card_steps, FONT_LABEL, TH->c_steps, LV_ALIGN_CENTER, 0, 0, LV_SYMBOL_LIST " 0");

    home_bat_arc = lv_arc_create(card_bat);
    /* Arc ajustado al card 72x36. Stroke 4 deja anillo visible sin invadir el
     * interior del label. El label "100%" (font 16 ~40px de ancho) sale del
     * bounding box del arc pero queda dentro del card. */
    lv_obj_set_size(home_bat_arc, 32, 32);
    lv_obj_align(home_bat_arc, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_rotation(home_bat_arc, 270);
    lv_arc_set_bg_angles(home_bat_arc, 0, 360);
    lv_obj_remove_style(home_bat_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(home_bat_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(home_bat_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(home_bat_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(home_bat_arc, lv_color_hex(TH->surface), LV_PART_MAIN);
    lv_obj_set_style_arc_color(home_bat_arc, lv_color_hex(TH->c_batt), LV_PART_INDICATOR);

    home_bat = make_label(home_bat_arc, FONT_LABEL, TH->text, LV_ALIGN_CENTER, 0, 0, "--%");

    home_hr  = make_label(card_hr,  FONT_LABEL, TH->c_hr,       LV_ALIGN_CENTER, 0, 0, LV_SYMBOL_TINT " --");
    home_act = make_label(card_act, FONT_LABEL, TH->c_activity, LV_ALIGN_CENTER, 0, 0, LV_SYMBOL_CHARGE " --");
}

static void build_bio(void) {
    scr_obj[SCREEN_BIO] = make_screen("BIOMETRIA");
    lv_obj_t *s = scr_obj[SCREEN_BIO];

    /* Layout vertical centrado: título arriba (y=18), 3 valores + edad de 2
     * de ellos, estado abajo. Todo alineado a LV_ALIGN_TOP_MID para caber
     * en el chord horizontal del disco a cada altura. */
    bio_hr       = make_label(s, FONT_VALUE, TH->c_hr,     LV_ALIGN_TOP_MID, 0, 46,  LV_SYMBOL_TINT " -- bpm");
    bio_age_hr   = make_label(s, FONT_LABEL, TH->text_dim, LV_ALIGN_TOP_MID, 0, 78,  "");

    bio_spo2     = make_label(s, FONT_VALUE, TH->c_spo2,   LV_ALIGN_TOP_MID, 0, 100, LV_SYMBOL_TINT " --%");
    bio_age_spo2 = make_label(s, FONT_LABEL, TH->text_dim, LV_ALIGN_TOP_MID, 0, 132, "");

    bio_temp     = make_label(s, FONT_VALUE, TH->c_temp,   LV_ALIGN_TOP_MID, 0, 154, LV_SYMBOL_WARNING " --.- C");
    bio_status   = make_label(s, FONT_LABEL, TH->ok,       LV_ALIGN_TOP_MID, 0, 200, "Estado: --");
}

static void build_hrspot(void) {
    scr_obj[SCREEN_HRSPOT] = make_screen("MEDIDA HR/SPO2");
    lv_obj_t *s = scr_obj[SCREEN_HRSPOT];

    hrspot_instr = make_label(s, FONT_LABEL, TH->text_dim,
                              LV_ALIGN_TOP_MID, 0, 50,
                              "Apoye el dedo sobre\nel sensor MAX30102.\n\n"
                              "Pulse SELECT para iniciar.");
    lv_obj_set_style_text_align(hrspot_instr, LV_TEXT_ALIGN_CENTER, 0);

    hrspot_ring = lv_obj_create(s);
    lv_obj_set_size(hrspot_ring, 44, 44);
    lv_obj_align(hrspot_ring, LV_ALIGN_CENTER, 0, -38);
    lv_obj_set_style_radius(hrspot_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(hrspot_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hrspot_ring, 4, 0);
    lv_obj_set_style_border_color(hrspot_ring, lv_color_hex(TH->c_hr), 0);
    lv_obj_add_flag(hrspot_ring, LV_OBJ_FLAG_HIDDEN);

    hrspot_heart = make_label(s, &ui_font_icon_56, TH->c_hr,
                              LV_ALIGN_CENTER, 0, -38, UI_SYM_HEART);
    lv_obj_add_flag(hrspot_heart, LV_OBJ_FLAG_HIDDEN);

    hrspot_progress = make_label(s, FONT_LABEL, TH->warn,
                                 LV_ALIGN_CENTER, 0, 38, "");
    hrspot_result   = make_label(s, FONT_VALUE, TH->text,
                                 LV_ALIGN_CENTER, 0, 0, "");
    /* Bottom -20 dejaba la baseline en Y=220; con texto largo como
     * "Apoye bien el dedo" (~150 px de ancho) la esquina caía fuera del disco.
     * -32 corre el label 12 px arriba y lo mete adentro con margen. */
    hrspot_quality  = make_label(s, FONT_LABEL, TH->text_dim,
                                 LV_ALIGN_BOTTOM_MID, 0, -32, "");
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

    ecg_wave = lv_line_create(s);
    lv_obj_set_size(ecg_wave, 180, 90);
    lv_obj_align(ecg_wave, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_line_color(ecg_wave, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_line_width(ecg_wave, 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(ecg_wave, true, LV_PART_MAIN);
    for (int i = 0; i < ECG_PTS; i++) { ecg_pts[i].x = (lv_coord_t)(i * 180 / (ECG_PTS - 1)); ecg_pts[i].y = 45; }
    lv_line_set_points(ecg_wave, ecg_pts, ECG_PTS);
    lv_obj_add_flag(ecg_wave, LV_OBJ_FLAG_HIDDEN);

    ecg_rec = make_label(s, FONT_VALUE, TH->alert,
                         LV_ALIGN_BOTTOM_MID, 8, -34, "REC");
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
    /* Rows apretados al disco: width 150 y y_step 32 mantienen la 5ta fila
     * (max_vis) en Y≈208 con esquina dentro del r=120. */
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        menu_rows[i] = lv_label_create(s);
        lv_obj_set_style_text_font(menu_rows[i], FONT_LABEL, LV_PART_MAIN);
        lv_label_set_text_safe(menu_rows[i], MENU_LABELS[i]);
        lv_obj_set_width(menu_rows[i], 150);
        lv_obj_set_style_pad_all(menu_rows[i], 4, LV_PART_MAIN);
        lv_obj_set_style_radius(menu_rows[i], 6, LV_PART_MAIN);
        lv_obj_align(menu_rows[i], LV_ALIGN_TOP_MID, 0, 48 + i * 32);
    }
}

static void build_mode(void) {
    scr_obj[SCREEN_MODE] = make_screen("MODO ENERGIA");
    lv_obj_t *s = scr_obj[SCREEN_MODE];

    mode_active_label = make_label(s, FONT_LABEL, TH->ok,
                                   LV_ALIGN_TOP_MID, 0, 42, "Activo: SPORT");

    for (int i = 0; i < MODE_ITEM_COUNT; i++) {
        mode_rows[i] = lv_label_create(s);
        lv_obj_set_style_text_font(mode_rows[i], FONT_LABEL, LV_PART_MAIN);
        lv_label_set_text_safe(mode_rows[i], MODE_LABELS[i]);
        lv_obj_set_width(mode_rows[i], 160);
        lv_obj_set_style_pad_all(mode_rows[i], 8, LV_PART_MAIN);
        lv_obj_set_style_radius(mode_rows[i], 6, LV_PART_MAIN);
        lv_obj_align(mode_rows[i], LV_ALIGN_TOP_MID, 0, 68 + i * 48);
    }

    make_label(s, FONT_LABEL, TH->text_dim, LV_ALIGN_BOTTOM_MID, 0, -24,
               "SELECT: aplicar");
}

static void build_settings(void) {
    scr_obj[SCREEN_SETTINGS] = make_screen("AUTO-OFF");
    lv_obj_t *s = scr_obj[SCREEN_SETTINGS];

    for (int i = 0; i < SETTINGS_ITEM_COUNT; i++) {
        settings_rows[i] = lv_label_create(s);
        lv_obj_set_style_text_font(settings_rows[i], FONT_LABEL, LV_PART_MAIN);
        lv_obj_set_width(settings_rows[i], 180);
        lv_obj_set_style_pad_all(settings_rows[i], 8, LV_PART_MAIN);
        lv_obj_set_style_radius(settings_rows[i], 6, LV_PART_MAIN);
        lv_obj_align(settings_rows[i], LV_ALIGN_TOP_MID, 0, 58 + i * 48);
    }
    make_label(s, FONT_LABEL, TH->text_dim, LV_ALIGN_BOTTOM_MID, 0, -24,
               "SELECT: cambiar");
}

static void build_theme(void) {
    scr_obj[SCREEN_THEME] = make_screen("TEMA");
    lv_obj_t *s = scr_obj[SCREEN_THEME];

    theme_active_label = make_label(s, FONT_LABEL, TH->ok,
                                    LV_ALIGN_TOP_MID, 0, 42, "Activo: --");

    /* 4 temas: y_step 32 mantiene la última fila (i=3) en Y≈160+28, cuerda
     * a Y=188 = 99 hemi → width 140 cabe con margen. */
    for (int i = 0; i < THEME_ITEM_COUNT; i++) {
        theme_rows[i] = lv_label_create(s);
        lv_obj_set_style_text_font(theme_rows[i], FONT_LABEL, LV_PART_MAIN);
        lv_label_set_text_safe(theme_rows[i], ui_theme_name((ui_theme_id_t)i));
        lv_obj_set_width(theme_rows[i], 140);
        lv_obj_set_style_pad_all(theme_rows[i], 5, LV_PART_MAIN);
        lv_obj_set_style_radius(theme_rows[i], 6, LV_PART_MAIN);
        lv_obj_align(theme_rows[i], LV_ALIGN_TOP_MID, 0, 68 + i * 32);
    }
    make_label(s, FONT_LABEL, TH->text_dim, LV_ALIGN_BOTTOM_MID, 0, -30,
               "SELECT: aplicar");
}

/* ── Selection Renders ── */
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

/* ── Animations ── */
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
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, hrspot_heart);
    lv_anim_set_exec_cb(&a, heart_opa_cb);
    lv_anim_set_values(&a, 150, 255);
    lv_anim_set_time(&a, 150);
    lv_anim_set_playback_time(&a, 600);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_t rs;
    lv_anim_init(&rs);
    lv_anim_set_var(&rs, hrspot_ring);
    lv_anim_set_exec_cb(&rs, heart_ring_size_cb);
    lv_anim_set_values(&rs, 40, 140);
    lv_anim_set_time(&rs, 750);
    lv_anim_set_repeat_count(&rs, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&rs, lv_anim_path_ease_out);
    lv_anim_start(&rs);

    lv_anim_t ro;
    lv_anim_init(&ro);
    lv_anim_set_var(&ro, hrspot_ring);
    lv_anim_set_exec_cb(&ro, heart_ring_opa_cb);
    lv_anim_set_values(&ro, 255, 0);
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
    
    const float W = 180.0f, cycles = 2.0f, amp = 34.0f;
    const float cy = 45.0f;
    ecg_phase += (now - last_ms) * 0.0006f;
    last_ms = now;
    if (ecg_phase > 1.0f) ecg_phase -= 1.0f;
    for (int i = 0; i < ECG_PTS; i++) {
        float fx = (float)i / (ECG_PTS - 1);
        float p = fx * cycles + ecg_phase;
        p -= (float)(int)p;
        ecg_pts[i].x = (lv_coord_t)(fx * W);
        ecg_pts[i].y = (lv_coord_t)(cy - ecg_morph(p) * amp);
    }
    lv_line_set_points(ecg_wave, ecg_pts, ECG_PTS);
}

/* ── Screen Updates ── */
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
    
    if (strcmp(lv_label_get_text(bio_status), st) != 0) {
        lv_label_set_text_safe(bio_status, st);
    }
    
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
        ecg_wave_update();
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

/* ── Navigation ── */
static void switch_to(ui_screen_t s) {
    if (s == current_screen) return;
    
    lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;
    
    if (s == (current_screen + SCREEN_CYCLE_COUNT - 1) % SCREEN_CYCLE_COUNT) {
        anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    } else if (s < current_screen && !(current_screen == SCREEN_CYCLE_COUNT - 1 && s == 0)) {
        anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    }
    
    current_screen = s;
    lv_scr_load_anim(scr_obj[s], anim, 120, 0, false);
    if (s == SCREEN_MENU)     render_list_selection(menu_rows,     MENU_ITEM_COUNT,     menu_selection, 48, 32, 5);
    if (s == SCREEN_MODE)     { render_mode_active(); render_list_selection(mode_rows, MODE_ITEM_COUNT, mode_selection, 68, 48, 4); }
    if (s == SCREEN_SETTINGS) { render_settings_labels(); render_list_selection(settings_rows, SETTINGS_ITEM_COUNT, settings_selection, 58, 48, 4); }
    if (s == SCREEN_THEME)    { render_theme_active(); render_list_selection(theme_rows, THEME_ITEM_COUNT, theme_selection, 68, 32, 4); }
    
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

    uint8_t m = (uint8_t)new_mode;
    ble_tx_push(BLE_TLV_TYPE_MODE_EVT, &m, 1, m);
    render_mode_active();
}

static void rebuild_ui(void);

static void cycle_settings_value(void) {
    uint16_t cur = power_get_display_off_s((power_mode_t)settings_selection);
    int idx = 0;
    for (int i = 0; i < (int)AUTO_OFF_VALUES_COUNT; i++) {
        if (AUTO_OFF_VALUES[i] == cur) { idx = i; break; }
    }
    idx = (idx + 1) % AUTO_OFF_VALUES_COUNT;
    power_set_display_off_s((power_mode_t)settings_selection, AUTO_OFF_VALUES[idx]);
    render_settings_labels();
    render_list_selection(settings_rows, SETTINGS_ITEM_COUNT, settings_selection, 58, 48, 4);
}

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
            {
                shared_sensor_data_t *sd = app_state_lock(100);
                if (sd) {
                    sd->steps_sw = 0;
                    app_state_unlock();
                }
                ESP_LOGI(TAG, "Menu: pasos reiniciados");
            }
            break;
        case 4: /* Vincular BLE */
            ESP_LOGI(TAG, "Menu: BLE advertising activo");
            break;
        case 5: /* Apagar */
            ESP_LOGI(TAG, "Menu: apagado solicitado");
            if (s_ui_actions.on_power_off) {
                s_ui_actions.on_power_off();
            }
            break;
        case 6: /* Tx IMU BLE */
            {
                bool new_state = !app_state_imu_tx_enabled();
                app_state_set_imu_tx_enabled(new_state);
                lv_label_set_text_safe(menu_rows[MENU_IDX_TXIMU],
                                       new_state ? LV_SYMBOL_WIFI " Tx IMU: ON"
                                                 : LV_SYMBOL_WIFI " Tx IMU: OFF");
                ESP_LOGI(TAG, "Menu: Tx IMU BLE = %s", new_state ? "ON" : "OFF");
            }
            break;
        case 7: /* Reset Bateria */
            if (s_ui_actions.on_battery_reset) {
                s_ui_actions.on_battery_reset();
            }
            switch_to(SCREEN_HOME);
            break;
    }
}

void ui_handle_button(btn_event_t ev) {
    if (ev == BTN_EVENT_NONE) return;

    switch (ev) {
        case BTN_EVENT_NEXT_SHORT:
            if (current_screen == SCREEN_MENU) {
                if (menu_selection + 1 >= MENU_ITEM_COUNT) {
                    menu_selection = 0;
                    render_list_selection(menu_rows, MENU_ITEM_COUNT, menu_selection, 48, 32, 5);
                    switch_to(SCREEN_HOME);
                } else {
                    menu_selection++;
                    render_list_selection(menu_rows, MENU_ITEM_COUNT, menu_selection, 48, 32, 5);
                }
            } else if (current_screen == SCREEN_MODE) {
                mode_selection = (mode_selection + 1) % MODE_ITEM_COUNT;
                render_list_selection(mode_rows, MODE_ITEM_COUNT, mode_selection, 68, 48, 4);
            } else if (current_screen == SCREEN_SETTINGS) {
                settings_selection = (settings_selection + 1) % SETTINGS_ITEM_COUNT;
                render_list_selection(settings_rows, SETTINGS_ITEM_COUNT, settings_selection, 58, 48, 4);
            } else if (current_screen == SCREEN_THEME) {
                theme_selection = (theme_selection + 1) % THEME_ITEM_COUNT;
                render_list_selection(theme_rows, THEME_ITEM_COUNT, theme_selection, 68, 32, 4);
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
    TH = ui_theme_get();
    build_home();
    build_bio();
    build_hrspot();
    build_ecg();
    build_menu();
    build_mode();
    build_settings();
    build_theme();
    render_list_selection(menu_rows, MENU_ITEM_COUNT, menu_selection, 48, 32, 5);
    render_list_selection(mode_rows, MODE_ITEM_COUNT, mode_selection, 68, 48, 4);
    render_list_selection(settings_rows, SETTINGS_ITEM_COUNT, settings_selection, 58, 48, 4);
    render_list_selection(theme_rows, THEME_ITEM_COUNT, theme_selection, 68, 32, 4);
    render_settings_labels();
    render_mode_active();
    render_theme_active();

    /* Make sure menu row for Tx IMU BLE displays correct initial state */
    bool imu_tx = app_state_imu_tx_enabled();
    lv_label_set_text_safe(menu_rows[MENU_IDX_TXIMU],
                           imu_tx ? LV_SYMBOL_WIFI " Tx IMU: ON"
                                  : LV_SYMBOL_WIFI " Tx IMU: OFF");
}

static void build_ui(void) {
    build_all_screens();
    lv_scr_load(scr_obj[SCREEN_HOME]);
}

static void rebuild_ui(void) {
    ui_screen_t keep = current_screen;
    lv_obj_t *tmp = lv_obj_create(NULL);
    lv_scr_load(tmp);
    for (int i = 0; i < SCREEN_COUNT; i++) {
        if (scr_obj[i]) { lv_obj_del(scr_obj[i]); scr_obj[i] = NULL; }
    }
    build_all_screens();
    s_last_bat_target = -1;
    s_heart_beating = false;
    lv_scr_load(scr_obj[keep]);
    lv_obj_del(tmp);
}

void ui_init(void) {
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf_1, buf_2, DISP_BUF_SIZE);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = display_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Touch capacitivo → indev pointer. LV_INDEV_DEF_READ_PERIOD (16 ms en
     * lv_conf.h) dispara touch_read_cb a ~60 Hz, alcanza para gestos y taps. */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    build_ui();
}

uint32_t ui_tick(void) {
    if (ble_telemetry_is_ecg_mode_active() && current_screen != SCREEN_ECG) {
        switch_to(SCREEN_ECG);
    }

    shared_sensor_data_t snap;
    bool has_data = false;
    shared_sensor_data_t *sd = app_state_lock(10);
    if (sd) {
        snap = *sd;
        has_data = true;
        app_state_unlock();
    }

    if (has_data) {
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

    return lv_timer_handler();
}

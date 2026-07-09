/**
 * @file ui_quickpanel.c
 * @brief Panel rápido de configuración (Quick Panel overlay).
 */
#include "ui_quickpanel.h"
#include "ui_router.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "ui_confirm.h"
#include "power_modes.h"
#include "app_state.h"
#include "gc9a01.h"
#include "lvgl.h"

#include <stdio.h>

#define TH (ui_theme_get())

static const uint16_t AUTO_OFF_VALUES[] = {5, 8, 15, 30, 60, 120};
#define AUTO_OFF_VALUES_COUNT (sizeof(AUTO_OFF_VALUES) / sizeof(AUTO_OFF_VALUES[0]))

static lv_obj_t *s_btn_autooff = NULL;
static lv_obj_t *s_btn_mode    = NULL;
static lv_obj_t *s_btn_theme   = NULL;
static lv_obj_t *s_btn_imutx   = NULL;
static lv_obj_t *s_lbl_brillo  = NULL;
static lv_obj_t *s_slider      = NULL;

/* Override de brillo manual */
static int s_brightness_override = -1;

static void update_autooff_text(void) {
    if (!s_btn_autooff) return;
    power_mode_t mode = power_get_mode();
    uint16_t sec = power_get_display_off_s(mode);
    lv_obj_t *lbl = lv_obj_get_child(s_btn_autooff, 0);
    if (lbl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Auto-off: %us", sec);
        lv_label_set_text(lbl, buf);
    }
}

static void update_mode_text(void) {
    if (!s_btn_mode) return;
    power_mode_t mode = power_get_mode();
    lv_obj_t *lbl = lv_obj_get_child(s_btn_mode, 0);
    if (lbl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Modo: %s", power_mode_name(mode));
        lv_label_set_text(lbl, buf);
    }
}

static void update_theme_text(void) {
    if (!s_btn_theme) return;
    ui_theme_id_t tid = ui_theme_get_id();
    const char *tname = "Desconocido";
    switch (tid) {
        case UI_THEME_MONO_WHITE: tname = "Blanco"; break;
        case UI_THEME_MONO_AMBER: tname = "Ámbar"; break;
        case UI_THEME_MONO_GREEN: tname = "Verde"; break;
        case UI_THEME_MONO_CYAN:  tname = "Cian"; break;
        default: break;
    }
    lv_obj_t *lbl = lv_obj_get_child(s_btn_theme, 0);
    if (lbl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Tema: %s", tname);
        lv_label_set_text(lbl, buf);
    }
}

static void update_imutx_text(void) {
    if (!s_btn_imutx) return;
    bool enabled = app_state_imu_tx_enabled();
    lv_obj_t *lbl = lv_obj_get_child(s_btn_imutx, 0);
    if (lbl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Tx IMU: %s", enabled ? "ON" : "OFF");
        lv_label_set_text(lbl, buf);
    }
}

static void update_brillo_text(int val) {
    if (!s_lbl_brillo) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "Brillo: %d%%", val);
    lv_label_set_text(s_lbl_brillo, buf);
}

static void btn_autooff_cb(lv_event_t *e) {
    power_mode_t mode = power_get_mode();
    uint16_t cur = power_get_display_off_s(mode);
    int idx = 0;
    for (int i = 0; i < (int)AUTO_OFF_VALUES_COUNT; i++) {
        if (AUTO_OFF_VALUES[i] == cur) { idx = i; break; }
    }
    idx = (idx + 1) % AUTO_OFF_VALUES_COUNT;
    power_set_display_off_s(mode, AUTO_OFF_VALUES[idx]);
    update_autooff_text();
}

static void btn_mode_cb(lv_event_t *e) {
    power_mode_t mode = power_get_mode();
    power_mode_t next = (mode + 1) % POWER_MODE_COUNT;
    power_set_mode(next);
    
    /* Adaptar brillo al nuevo perfil */
    int b = power_get_display_brightness(next);
    s_brightness_override = b;
    gc9a01_set_brightness(b);
    if (s_slider) lv_slider_set_value(s_slider, b, LV_ANIM_OFF);
    update_brillo_text(b);

    update_mode_text();
    update_autooff_text();
}

static void btn_theme_cb(lv_event_t *e) {
    ui_theme_id_t tid = ui_theme_get_id();
    ui_theme_id_t next = (tid - UI_THEME_MONO_FIRST + 1) % 4 + UI_THEME_MONO_FIRST;
    ui_theme_set(next);
    
    /* Reconstruir la UI con los nuevos colores de forma instantánea */
    ui_router_reset_all();
}

static void btn_imutx_cb(lv_event_t *e) {
    bool enabled = app_state_imu_tx_enabled();
    app_state_set_imu_tx_enabled(!enabled);
    update_imutx_text();
}

static void do_reset_steps(void) {
    shared_sensor_data_t *sd = app_state_lock(5);
    if (sd) {
        sd->steps_sw = 0;
        app_state_unlock();
    }
}

static void btn_reset_steps_cb(lv_event_t *e) {
    ui_confirm_open("¿Reiniciar pasos?", do_reset_steps);
}

static void slider_brillo_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    s_brightness_override = val;
    gc9a01_set_brightness(val);
    update_brillo_text(val);
}

static lv_obj_t *quickpanel_build(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Título */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "AJUSTES");
    lv_obj_set_style_text_color(title, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    /* Contenedor scrollable vertical */
    lv_obj_t *cnt = lv_obj_create(scr);
    lv_obj_set_size(cnt, 200, 182);
    lv_obj_align(cnt, LV_ALIGN_CENTER, 0, 14);
    lv_obj_set_flex_flow(cnt, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cnt, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(cnt, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(cnt, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(cnt, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cnt, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(cnt, 10, LV_PART_MAIN);

    /* Slider de Brillo */
    lv_obj_t *brillo_group = lv_obj_create(cnt);
    lv_obj_set_size(brillo_group, 160, 48);
    lv_obj_set_style_bg_opa(brillo_group, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(brillo_group, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(brillo_group, 0, LV_PART_MAIN);
    lv_obj_clear_flag(brillo_group, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_brillo = lv_label_create(brillo_group);
    lv_obj_set_style_text_color(s_lbl_brillo, lv_color_hex(TH->text_dim), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_brillo, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_brillo, LV_ALIGN_TOP_MID, 0, 0);

    int cur_b = (s_brightness_override >= 0) ? s_brightness_override : power_get_display_brightness(power_get_mode());
    update_brillo_text(cur_b);

    s_slider = lv_slider_create(brillo_group);
    lv_obj_set_size(s_slider, 140, 10);
    lv_obj_align(s_slider, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_slider_set_range(s_slider, 10, 100);
    lv_slider_set_value(s_slider, cur_b, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(TH->surface), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(TH->accent), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(TH->accent), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_slider, 2, LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider, slider_brillo_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Creación de botones */
    lv_obj_t *lbl;

    #define CREATE_PANEL_BTN(btn_var, cb_func) \
        btn_var = lv_btn_create(cnt); \
        lv_obj_set_size(btn_var, 160, 36); \
        lv_obj_set_style_bg_color(btn_var, lv_color_hex(TH->surface), LV_PART_MAIN); \
        lv_obj_set_style_radius(btn_var, 8, LV_PART_MAIN); \
        lv_obj_set_style_shadow_width(btn_var, 0, LV_PART_MAIN); \
        lbl = lv_label_create(btn_var); \
        lv_obj_set_style_text_color(lbl, lv_color_hex(TH->text), LV_PART_MAIN); \
        lv_obj_set_style_text_font(lbl, &ui_font_label_16, LV_PART_MAIN); \
        lv_obj_center(lbl); \
        lv_obj_add_event_cb(btn_var, cb_func, LV_EVENT_CLICKED, NULL);

    /* Autooff */
    CREATE_PANEL_BTN(s_btn_autooff, btn_autooff_cb);
    update_autooff_text();

    /* Modo */
    CREATE_PANEL_BTN(s_btn_mode, btn_mode_cb);
    update_mode_text();

    /* Tema */
    CREATE_PANEL_BTN(s_btn_theme, btn_theme_cb);
    update_theme_text();

    /* IMU Tx */
    CREATE_PANEL_BTN(s_btn_imutx, btn_imutx_cb);
    update_imutx_text();

    /* Reset Pasos */
    lv_obj_t *btn_reset;
    CREATE_PANEL_BTN(btn_reset, btn_reset_steps_cb);
    lv_label_set_text(lv_obj_get_child(btn_reset, 0), "Reset Pasos");

    return scr;
}

static bool quickpanel_on_gesture(uint8_t g) {
    if (g == 0x01) { /* SWIPE_UP */
        ui_router_pop(NAV_ANIM_SLIDE_UP);
        return true;
    }
    return false;
}

void ui_quickpanel_register(void) {
    ui_route_desc_t desc = {
        .id         = ROUTE_QUICK_PANEL,
        .build      = quickpanel_build,
        .on_enter   = NULL,
        .on_leave   = NULL,
        .tick       = NULL,
        .on_button  = NULL,
        .on_gesture = quickpanel_on_gesture,
        .name       = "quick_panel",
    };
    ui_router_register(&desc);
}

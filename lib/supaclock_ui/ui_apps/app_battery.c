/**
 * @file app_battery.c
 * @brief Aplicación de Batería con monitor de voltaje y reset de SoC.
 */
#include "app_battery.h"
#include "ui_router.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "ui_confirm.h"
#include "supaclock_ui.h"
#include "app_state.h"
#include "lvgl.h"

#include <stdio.h>

#define TH (ui_theme_get())

static lv_obj_t *s_arc         = NULL;
static lv_obj_t *s_lbl_pct     = NULL;
static lv_obj_t *s_lbl_mv      = NULL;
static lv_obj_t *s_btn_reset   = NULL;

static void update_battery_ui(void) {
    float soc = 0.0f;
    uint16_t mv = 0;
    shared_sensor_data_t *st = app_state_lock(5);
    if (st) {
        soc = st->battery_soc;
        mv = st->battery_mv;
        app_state_unlock();
    }

    if (soc < 0.0f) soc = 0.0f;
    if (soc > 100.0f) soc = 100.0f;

    if (s_arc) {
        lv_arc_set_value(s_arc, (int)soc);
    }
    if (s_lbl_pct) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", (int)soc);
        lv_label_set_text(s_lbl_pct, buf);
    }
    if (s_lbl_mv) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u mV (%.2f V)", mv, mv / 1000.0f);
        lv_label_set_text(s_lbl_mv, buf);
    }
}

static void btn_reset_cb(lv_event_t *e) {
    ui_confirm_open("¿Reiniciar Batería?", ui_action_battery_reset);
}

static lv_obj_t *battery_build(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Título */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ESTADO BATERÍA");
    lv_obj_set_style_text_color(title, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    /* Arco circular de carga */
    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, 160, 160);
    lv_obj_align(s_arc, LV_ALIGN_CENTER, 0, -10);
    lv_arc_set_rotation(s_arc, 135);
    lv_arc_set_bg_angles(s_arc, 0, 270);
    lv_arc_set_range(s_arc, 0, 100);
    lv_obj_set_style_arc_width(s_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(TH->surface), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(TH->c_batt), LV_PART_INDICATOR);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);

    /* Label del porcentaje */
    s_lbl_pct = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_pct, lv_color_hex(TH->text), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_pct, &ui_font_subhero_36, LV_PART_MAIN);
    lv_obj_align(s_lbl_pct, LV_ALIGN_CENTER, 0, -12);

    /* Label del voltaje */
    s_lbl_mv = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_mv, lv_color_hex(TH->text_dim), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_mv, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_mv, LV_ALIGN_CENTER, 0, 18);

    /* Botón Reset */
    s_btn_reset = lv_btn_create(scr);
    lv_obj_set_size(s_btn_reset, 110, 32);
    lv_obj_align(s_btn_reset, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_style_bg_color(s_btn_reset, lv_color_hex(TH->surface), LV_PART_MAIN);
    lv_obj_set_style_radius(s_btn_reset, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_btn_reset, 0, LV_PART_MAIN);
    
    lv_obj_t *lbl_btn = lv_label_create(s_btn_reset);
    lv_label_set_text(lbl_btn, "RESETEAR SOC");
    lv_obj_set_style_text_color(lbl_btn, lv_color_hex(TH->text), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_btn, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_center(lbl_btn);

    lv_obj_add_event_cb(s_btn_reset, btn_reset_cb, LV_EVENT_CLICKED, NULL);

    update_battery_ui();

    return scr;
}

static void battery_tick(void) {
    update_battery_ui();
}

void app_battery_register(void) {
    ui_route_desc_t desc = {
        .id         = ROUTE_APP_BATTERY,
        .build      = battery_build,
        .on_enter   = NULL,
        .on_leave   = NULL,
        .tick       = battery_tick,
        .on_button  = NULL,
        .on_gesture = NULL,
        .name       = "app_battery",
    };
    ui_router_register(&desc);
}

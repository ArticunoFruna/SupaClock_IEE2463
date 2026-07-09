/**
 * @file app_power.c
 * @brief Aplicación de apagado del dispositivo (Power).
 */
#include "app_power.h"
#include "ui_router.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "ui_confirm.h"
#include "supaclock_ui.h"
#include "lvgl.h"

#define TH (ui_theme_get())

static void do_power_off(void) {
    ui_action_power_off();
}

static void btn_power_cb(lv_event_t *e) {
    ui_confirm_open("¿Apagar dispositivo?", do_power_off);
}

static lv_obj_t *power_build(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Título */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "APAGAR SISTEMA");
    lv_obj_set_style_text_color(title, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    /* Icono POWER gigante */
    lv_obj_t *icon = lv_label_create(scr);
    lv_label_set_text(icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_color(icon, lv_color_hex(TH->alert), LV_PART_MAIN);
    lv_obj_set_style_text_font(icon, &ui_font_icon_56, LV_PART_MAIN);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -35);

    /* Instrucción */
    lv_obj_t *lbl_instr = lv_label_create(scr);
    lv_label_set_text(lbl_instr, "Confirme para entrar\nen modo sleep profundo.");
    lv_obj_set_style_text_color(lbl_instr, lv_color_hex(TH->text_dim), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_instr, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_instr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl_instr, LV_ALIGN_CENTER, 0, 20);

    /* Botón Apagar */
    lv_obj_t *btn_power = lv_btn_create(scr);
    lv_obj_set_size(btn_power, 120, 32);
    lv_obj_align(btn_power, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_style_bg_color(btn_power, lv_color_hex(TH->alert), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_power, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_power, 0, LV_PART_MAIN);
    
    lv_obj_t *lbl_btn = lv_label_create(btn_power);
    lv_label_set_text(lbl_btn, "APAGAR");
    lv_obj_set_style_text_color(lbl_btn, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_btn, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_center(lbl_btn);

    lv_obj_add_event_cb(btn_power, btn_power_cb, LV_EVENT_CLICKED, NULL);

    return scr;
}

void app_power_register(void) {
    ui_route_desc_t desc = {
        .id         = ROUTE_APP_POWER,
        .build      = power_build,
        .on_enter   = NULL,
        .on_leave   = NULL,
        .tick       = NULL,
        .on_button  = NULL,
        .on_gesture = NULL,
        .name       = "app_power",
    };
    ui_router_register(&desc);
}

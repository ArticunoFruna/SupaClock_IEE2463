/**
 * @file app_ble.c
 * @brief Aplicación de BLE (Pairing y estado).
 */
#include "app_ble.h"
#include "ui_router.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "ui_confirm.h"
#include "ble_bond.h"
#include "lvgl.h"

#include <stdio.h>

#define TH (ui_theme_get())

static lv_obj_t *s_lbl_status = NULL;
static lv_obj_t *s_btn_action = NULL;

static void update_ble_ui(void) {
    bool paired = ble_bond_is_paired();
    if (s_lbl_status) {
        lv_label_set_text(s_lbl_status, paired ? "ESTADO: VINCULADO" : "ESTADO: SIN VINCULAR\n\nListo para conectar\ndesde la App móvil.");
    }
    if (s_btn_action) {
        if (paired) {
            lv_obj_clear_flag(s_btn_action, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_btn_action, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void do_erase_bonds(void) {
    ble_bond_erase_all();
    update_ble_ui();
}

static void btn_action_cb(lv_event_t *e) {
    ui_confirm_open("¿Borrar Vinculaciones?", do_erase_bonds);
}

static lv_obj_t *ble_build(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Título */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "CONEXIÓN BLE");
    lv_obj_set_style_text_color(title, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    /* Icono BLE grande */
    lv_obj_t *icon = lv_label_create(scr);
    lv_label_set_text(icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(icon, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(icon, &ui_font_icon_56, LV_PART_MAIN);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -45);

    /* Estado de conexión */
    s_lbl_status = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(TH->text), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_status, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_lbl_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_status, LV_ALIGN_CENTER, 0, 20);

    /* Botón Desvincular */
    s_btn_action = lv_btn_create(scr);
    lv_obj_set_size(s_btn_action, 120, 32);
    lv_obj_align(s_btn_action, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_style_bg_color(s_btn_action, lv_color_hex(TH->surface), LV_PART_MAIN);
    lv_obj_set_style_radius(s_btn_action, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_btn_action, 0, LV_PART_MAIN);
    
    lv_obj_t *lbl_btn = lv_label_create(s_btn_action);
    lv_label_set_text(lbl_btn, "DESVINCULAR");
    lv_obj_set_style_text_color(lbl_btn, lv_color_hex(TH->text), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_btn, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_center(lbl_btn);

    lv_obj_add_event_cb(s_btn_action, btn_action_cb, LV_EVENT_CLICKED, NULL);

    update_ble_ui();

    return scr;
}

static void ble_tick(void) {
    update_ble_ui();
}

void app_ble_register(void) {
    ui_route_desc_t desc = {
        .id         = ROUTE_APP_BLE,
        .build      = ble_build,
        .on_enter   = NULL,
        .on_leave   = NULL,
        .tick       = ble_tick,
        .on_button  = NULL,
        .on_gesture = NULL,
        .name       = "app_ble",
    };
    ui_router_register(&desc);
}

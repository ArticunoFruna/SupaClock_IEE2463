/**
 * @file ui_confirm.c
 * @brief Helper para modales de confirmación en la UI.
 */
#include "ui_confirm.h"
#include "ui_theme.h"
#include "ui_fonts.h"

#define TH (ui_theme_get())

static lv_obj_t *s_overlay = NULL;
static ui_confirm_cb_t s_ok_cb = NULL;

static void btn_cancel_event_cb(lv_event_t *e) {
    if (s_overlay) {
        lv_obj_del(s_overlay);
        s_overlay = NULL;
    }
}

static void btn_ok_event_cb(lv_event_t *e) {
    if (s_ok_cb) s_ok_cb();
    if (s_overlay) {
        lv_obj_del(s_overlay);
        s_overlay = NULL;
    }
}

void ui_confirm_open(const char *msg, ui_confirm_cb_t ok_cb) {
    if (s_overlay) return; /* Prevenir duplicados */

    s_ok_cb = ok_cb;

    /* Overlay oscuro sobre toda la pantalla */
    s_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_overlay, 240, 240);
    lv_obj_center(s_overlay);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_overlay, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* Caja de diálogo central */
    lv_obj_t *box = lv_obj_create(s_overlay);
    lv_obj_set_size(box, 190, 160);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(TH->surface), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(box, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_radius(box, 16, LV_PART_MAIN);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    /* Mensaje descriptivo */
    lv_obj_t *lbl = lv_label_create(box);
    lv_label_set_text(lbl, msg);
    lv_obj_set_style_text_color(lbl, lv_color_hex(TH->text), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl, 160);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 15);

    /* Botón SÍ (Aceptar) */
    lv_obj_t *btn_ok = lv_btn_create(box);
    lv_obj_set_size(btn_ok, 65, 36);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_LEFT, 5, -10);
    lv_obj_set_style_radius(btn_ok, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(TH->ok), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_ok, 0, LV_PART_MAIN);

    lv_obj_t *lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "SÍ");
    lv_obj_set_style_text_color(lbl_ok, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_ok, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_center(lbl_ok);

    lv_obj_add_event_cb(btn_ok, btn_ok_event_cb, LV_EVENT_CLICKED, NULL);

    /* Botón NO (Cancelar) */
    lv_obj_t *btn_cancel = lv_btn_create(box);
    lv_obj_set_size(btn_cancel, 65, 36);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -5, -10);
    lv_obj_set_style_radius(btn_cancel, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(TH->alert), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_cancel, 0, LV_PART_MAIN);

    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "NO");
    lv_obj_set_style_text_color(lbl_cancel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_cancel, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_center(lbl_cancel);

    lv_obj_add_event_cb(btn_cancel, btn_cancel_event_cb, LV_EVENT_CLICKED, NULL);
}

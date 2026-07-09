/**
 * @file app_about.c
 * @brief Aplicación "Acerca de" (About).
 */
#include "app_about.h"
#include "ui_router.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "lvgl.h"

#define TH (ui_theme_get())

static lv_obj_t *about_build(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Título */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ACERCA DE");
    lv_obj_set_style_text_color(title, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    /* Nombre del reloj */
    lv_obj_t *name = lv_label_create(scr);
    lv_label_set_text(name, "SupaClock");
    lv_obj_set_style_text_color(name, lv_color_hex(TH->text), LV_PART_MAIN);
    lv_obj_set_style_text_font(name, &ui_font_value_28, LV_PART_MAIN);
    lv_obj_align(name, LV_ALIGN_CENTER, 0, -35);

    /* Versión */
    lv_obj_t *version = lv_label_create(scr);
    lv_label_set_text(version, "v2.5.0 (Touch UI)");
    lv_obj_set_style_text_color(version, lv_color_hex(TH->text_dim), LV_PART_MAIN);
    lv_obj_set_style_text_font(version, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(version, LV_ALIGN_CENTER, 0, -5);

    /* Git Hash / Build info */
    lv_obj_t *git = lv_label_create(scr);
    lv_label_set_text(git, "Build: IEE2463-ESP32S3\nHash: 7f3b89d (Main)");
    lv_obj_set_style_text_color(git, lv_color_hex(TH->text_dim), LV_PART_MAIN);
    lv_obj_set_style_text_font(git, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(git, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(git, LV_ALIGN_CENTER, 0, 30);

    /* Lista de Controladores */
    lv_obj_t *drivers = lv_label_create(scr);
    lv_label_set_text(drivers, "LCD: GC9A01 | Touch: CST816S\nSens: MAX30102 / AD8232");
    lv_obj_set_style_text_color(drivers, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(drivers, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(drivers, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(drivers, LV_ALIGN_BOTTOM_MID, 0, -22);

    return scr;
}

void app_about_register(void) {
    ui_route_desc_t desc = {
        .id         = ROUTE_APP_ABOUT,
        .build      = about_build,
        .on_enter   = NULL,
        .on_leave   = NULL,
        .tick       = NULL,
        .on_button  = NULL,
        .on_gesture = NULL,
        .name       = "app_about",
    };
    ui_router_register(&desc);
}

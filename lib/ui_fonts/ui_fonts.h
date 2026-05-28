/**
 * @file ui_fonts.h
 * @brief Fuentes Inter personalizadas para la UI (generadas con lv_font_conv).
 *
 * - ui_font_hero_56:  Inter Light 56px, sólo ASCII + ° (reloj, timer, números grandes)
 * - ui_font_value_28: Inter Medium 28px, ASCII + ° + íconos LV_SYMBOL (valores)
 * - ui_font_label_16: Inter Regular 16px, ASCII + ° + íconos LV_SYMBOL (labels/títulos)
 *
 * Íconos incluidos (FontAwesome 5): LIST, TINT, CHARGE, WARNING, SETTINGS,
 * EYE_CLOSE, REFRESH, POWER, WIFI (solid) y BLUETOOTH (brands).
 */
#ifndef UI_FONTS_H
#define UI_FONTS_H

#include "lvgl.h"

LV_FONT_DECLARE(ui_font_hero_56);
LV_FONT_DECLARE(ui_font_value_28);
LV_FONT_DECLARE(ui_font_label_16);
LV_FONT_DECLARE(ui_font_icon_56);   /* sólo heart (0xF004) + heartbeat (0xF21E), 56px */

/* UTF-8 de los glifos del icon font (para usar en lv_label_set_text) */
#define UI_SYM_HEART      "\xEF\x80\x84"   /* U+F004 fa-heart */
#define UI_SYM_HEARTBEAT  "\xEF\x88\x9E"   /* U+F21E fa-heartbeat */

#endif /* UI_FONTS_H */

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
LV_FONT_DECLARE(ui_font_subhero_36);
LV_FONT_DECLARE(ui_font_value_28);
LV_FONT_DECLARE(ui_font_label_16);
LV_FONT_DECLARE(ui_font_icon_56);

/* UTF-8 de los glifos de FontAwesome 5 */
#define UI_SYM_HEART         "\xEF\x80\x84"   /* U+F004 fa-heart (HR) */
#define UI_SYM_HEARTBEAT     "\xEF\x88\x9E"   /* U+F21E fa-heartbeat (ECG) */
#define UI_SYM_THERMOMETER   "\xEF\x8B\x89"   /* U+F2C9 fa-thermometer-half (Temp) */
#define UI_SYM_BATTERY_FULL  "\xEF\x89\x80"   /* U+F240 fa-battery-full (Battery 100%) */
#define UI_SYM_BATTERY_3     "\xEF\x89\x81"   /* U+F241 fa-battery-three-quarters (75%) */
#define UI_SYM_BATTERY_2     "\xEF\x89\x82"   /* U+F242 fa-battery-half (50%) */
#define UI_SYM_BATTERY_1     "\xEF\x89\x83"   /* U+F243 fa-battery-quarter (25%) */
#define UI_SYM_BATTERY_EMPTY "\xEF\x89\x84"   /* U+F244 fa-battery-empty (0%) */
#define UI_SYM_BOLT          "\xEF\x83\xA7"   /* U+F0E7 fa-bolt (Charging) */
#define UI_SYM_BLUETOOTH     "\xEF\x8A\x93"   /* U+F293 fa-bluetooth (BLE) */
#define UI_SYM_COG           "\xEF\x80\x93"   /* U+F013 fa-cog (Settings) */
#define UI_SYM_POWER         "\xEF\x80\x91"   /* U+F011 fa-power-off (Power) */
#define UI_SYM_INFO          "\xEF\x81\x9A"   /* U+F05A fa-info-circle (About) */
#define UI_SYM_STEPS         "\xEF\x81\xA2"   /* U+F062 fa-arrow-up (Steps) */

/* Iconos de Actividad HAR */
#define UI_SYM_RESTING       "\xEF\x92\xB8"   /* U+F4B8 fa-couch (Resting) */
#define UI_SYM_WALKING       "\xEF\x95\x94"   /* U+F554 fa-walking (Walking) */
#define UI_SYM_RUNNING       "\xEF\x9C\x8C"   /* U+F70C fa-running (Running) */
#define UI_SYM_STAIRS        "\xEF\x8E\xBF"   /* U+F3BF fa-level-up-alt (Stairs) */

#endif /* UI_FONTS_H */

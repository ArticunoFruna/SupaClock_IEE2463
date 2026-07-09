/**
 * @file ui_tiles.c
 * @brief Implementación del carrusel de tiles glanceables (Galaxy-Watch-style).
 */
#include "ui_tiles.h"
#include "ui_router.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "app_state.h"

#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "UI_TILES";

#define TH (ui_theme_get())

/* Meta de pasos */
#define STEPS_GOAL 8000

/* Handles LVGL */
static lv_obj_t *s_scr        = NULL;
static lv_obj_t *s_lbl_title  = NULL;
static lv_obj_t *s_lbl_val    = NULL;
static lv_obj_t *s_lbl_unit   = NULL;
static lv_obj_t *s_lbl_sub    = NULL;
static lv_obj_t *s_arc_tile   = NULL;
static lv_obj_t *s_dots[5]    = {NULL};

/* Estado interno */
static int s_tile_idx = 0;

/* Cache para evitar redibujo en ticks si no cambia */
static int s_last_hr = -1;
static int s_last_spo2 = -1;
static int s_last_steps = -1;
static int s_last_har = -1;
static int s_last_batt = -1;

static inline uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void format_age(char *out, size_t cap, uint32_t updated_ms) {
    if (updated_ms == 0) {
        snprintf(out, cap, "(sin medir)");
        return;
    }
    uint32_t age = (now_ms() - updated_ms) / 1000;
    if (age < 5)        snprintf(out, cap, "ahora");
    else if (age < 60)  snprintf(out, cap, "hace %lus", (unsigned long)age);
    else if (age < 3600)snprintf(out, cap, "hace %lum", (unsigned long)(age / 60));
    else                snprintf(out, cap, "hace %luh", (unsigned long)(age / 3600));
}

static void update_dots(void) {
    for (int i = 0; i < 5; i++) {
        if (!s_dots[i]) continue;
        if (i == s_tile_idx) {
            lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(TH->accent), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_COVER, LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(TH->text_dim), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_60, LV_PART_MAIN);
        }
    }
}

static void update_tile_view(void) {
    if (!s_scr) return;

    /* Ocultar/mostrar arco según el tile */
    if (s_tile_idx == 2 || s_tile_idx == 4) {
        lv_obj_clear_flag(s_arc_tile, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_arc_tile, LV_OBJ_FLAG_HIDDEN);
    }

    uint32_t val_color = TH->text;

    switch (s_tile_idx) {
        case 0:
            lv_label_set_text(s_lbl_title, "RITMO CARDÍACO");
            lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(TH->c_hr), LV_PART_MAIN);
            val_color = TH->c_hr;
            break;
        case 1:
            lv_label_set_text(s_lbl_title, "OXÍGENO EN SANGRE");
            lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(TH->c_spo2), LV_PART_MAIN);
            val_color = TH->c_spo2;
            break;
        case 2:
            lv_label_set_text(s_lbl_title, "PASOS HOY");
            lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(TH->c_steps), LV_PART_MAIN);
            lv_obj_set_style_arc_color(s_arc_tile, lv_color_hex(TH->surface), LV_PART_MAIN);
            lv_obj_set_style_arc_color(s_arc_tile, lv_color_hex(TH->c_steps), LV_PART_INDICATOR);
            val_color = TH->c_steps;
            break;
        case 3:
            lv_label_set_text(s_lbl_title, "ACTIVIDAD (HAR)");
            lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(TH->c_activity), LV_PART_MAIN);
            val_color = TH->c_activity;
            break;
        case 4:
            lv_label_set_text(s_lbl_title, "BATERÍA");
            lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(TH->c_batt), LV_PART_MAIN);
            lv_obj_set_style_arc_color(s_arc_tile, lv_color_hex(TH->surface), LV_PART_MAIN);
            lv_obj_set_style_arc_color(s_arc_tile, lv_color_hex(TH->c_batt), LV_PART_INDICATOR);
            val_color = TH->c_batt;
            break;
    }

    lv_obj_set_style_text_color(s_lbl_val, lv_color_hex(val_color), LV_PART_MAIN);

    /* Forzar refresh de datos */
    s_last_hr = -1;
    s_last_spo2 = -1;
    s_last_steps = -1;
    s_last_har = -1;
    s_last_batt = -1;

    update_dots();
}

static lv_obj_t *tiles_build(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Arco radial concéntrico */
    s_arc_tile = lv_arc_create(scr);
    lv_obj_set_size(s_arc_tile, 220, 220);
    lv_obj_center(s_arc_tile);
    lv_arc_set_rotation(s_arc_tile, 135);
    lv_arc_set_bg_angles(s_arc_tile, 0, 270);
    lv_arc_set_angles(s_arc_tile, 0, 0);
    lv_obj_remove_style(s_arc_tile, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_arc_tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_arc_tile, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc_tile, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_arc_tile, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(s_arc_tile, true, LV_PART_INDICATOR);

    /* Título superior */
    s_lbl_title = lv_label_create(scr);
    lv_label_set_text(s_lbl_title, "");
    lv_obj_set_style_text_font(s_lbl_title, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_title, LV_ALIGN_TOP_MID, 0, 36);

    /* Valor central */
    s_lbl_val = lv_label_create(scr);
    lv_label_set_text(s_lbl_val, "");
    lv_obj_set_style_text_font(s_lbl_val, &ui_font_subhero_36, LV_PART_MAIN);
    lv_obj_align(s_lbl_val, LV_ALIGN_CENTER, 0, -15);

    /* Unidad debajo del valor */
    s_lbl_unit = lv_label_create(scr);
    lv_label_set_text(s_lbl_unit, "");
    lv_obj_set_style_text_font(s_lbl_unit, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_unit, lv_color_hex(TH->text_dim), LV_PART_MAIN);
    lv_obj_align(s_lbl_unit, LV_ALIGN_CENTER, 0, 25);

    /* Subtexto / Edad */
    s_lbl_sub = lv_label_create(scr);
    lv_label_set_text(s_lbl_sub, "");
    lv_obj_set_style_text_font(s_lbl_sub, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_sub, lv_color_hex(TH->text_dim), LV_PART_MAIN);
    lv_obj_align(s_lbl_sub, LV_ALIGN_CENTER, 0, 50);

    /* Dots indicator en la parte inferior */
    lv_obj_t *dots_cnt = lv_obj_create(scr);
    lv_obj_set_size(dots_cnt, 120, 20);
    lv_obj_align(dots_cnt, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_bg_opa(dots_cnt, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dots_cnt, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(dots_cnt, 0, LV_PART_MAIN);
    lv_obj_clear_flag(dots_cnt, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 5; i++) {
        s_dots[i] = lv_obj_create(dots_cnt);
        lv_obj_set_size(s_dots[i], 8, 8);
        lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_align(s_dots[i], LV_ALIGN_CENTER, (i - 2) * 16, 0);
        lv_obj_set_style_border_width(s_dots[i], 0, LV_PART_MAIN);
    }

    s_scr = scr;
    return scr;
}

static void tiles_on_enter(int32_t param) {
    s_tile_idx = (int)param;
    if (s_tile_idx < 0) s_tile_idx = 0;
    if (s_tile_idx > 4) s_tile_idx = 4;
    update_tile_view();
}

static void tiles_tick(void) {
    if (!s_scr) return;

    int hr = -1, spo2 = -1, batt_soc = -1, batt_mv = -1;
    uint32_t steps = 0;
    uint8_t har_state = 0;
    uint32_t hr_up = 0, spo2_up = 0, har_up = 0;

    shared_sensor_data_t *st = app_state_lock(5);
    if (st) {
        hr = st->hr_bpm;
        hr_up = st->hr_updated_ms;
        spo2 = st->spo2_pct;
        spo2_up = st->spo2_updated_ms;
        steps = st->steps_sw;
        har_state = st->har_state;
        har_up = st->har_updated_ms;
        batt_soc = (int)(st->battery_soc + 0.5f);
        batt_mv = st->battery_mv;
        app_state_unlock();
    }

    char buf_val[32] = "";
    char buf_unit[32] = "";
    char buf_sub[64] = "";
    char age_str[24] = "";

    switch (s_tile_idx) {
        case 0: { /* HR */
            uint32_t age_ms = now_ms() - hr_up;
            bool valid = (hr_up != 0 && age_ms < 60000 && hr > 0);
            if (valid) {
                snprintf(buf_val, sizeof(buf_val), "%d", hr);
                snprintf(buf_unit, sizeof(buf_unit), "PPM");
                format_age(age_str, sizeof(age_str), hr_up);
                snprintf(buf_sub, sizeof(buf_sub), "Medido: %s", age_str);
            } else {
                snprintf(buf_val, sizeof(buf_val), "--");
                snprintf(buf_unit, sizeof(buf_unit), "PPM");
                snprintf(buf_sub, sizeof(buf_sub), "Buscando...");
            }
            break;
        }
        case 1: { /* SpO2 */
            uint32_t age_ms = now_ms() - spo2_up;
            bool valid = (spo2_up != 0 && age_ms < 60000 && spo2 > 0);
            if (valid) {
                snprintf(buf_val, sizeof(buf_val), "%d", spo2);
                snprintf(buf_unit, sizeof(buf_unit), "%% SpO2");
                format_age(age_str, sizeof(age_str), spo2_up);
                snprintf(buf_sub, sizeof(buf_sub), "Medido: %s", age_str);
            } else {
                snprintf(buf_val, sizeof(buf_val), "--");
                snprintf(buf_unit, sizeof(buf_unit), "%% SpO2");
                snprintf(buf_sub, sizeof(buf_sub), "Buscando...");
            }
            break;
        }
        case 2: { /* Steps */
            snprintf(buf_val, sizeof(buf_val), "%lu", (unsigned long)steps);
            snprintf(buf_unit, sizeof(buf_unit), "/ %d pasos", STEPS_GOAL);
            int pct = (steps * 100) / STEPS_GOAL;
            if (pct > 100) pct = 100;
            snprintf(buf_sub, sizeof(buf_sub), "%d%% de la meta", pct);
            
            uint16_t span = (uint16_t)((270 * pct) / 100);
            lv_arc_set_angles(s_arc_tile, 0, span);
            break;
        }
        case 3: { /* HAR Activity */
            const char *har_name = "Desconocido";
            switch (har_state) {
                case 0: har_name = "Reposo"; break;
                case 1: har_name = "Caminar"; break;
                case 2: har_name = "Correr"; break;
                case 3: har_name = "Escaleras"; break;
            }
            snprintf(buf_val, sizeof(buf_val), "%s", har_name);
            snprintf(buf_unit, sizeof(buf_unit), "Estado");
            format_age(age_str, sizeof(age_str), har_up);
            snprintf(buf_sub, sizeof(buf_sub), "Inferencia: %s", age_str);
            break;
        }
        case 4: { /* Battery */
            if (batt_soc < 0) batt_soc = 0;
            if (batt_soc > 100) batt_soc = 100;
            snprintf(buf_val, sizeof(buf_val), "%d%%", batt_soc);
            
            int v_int = batt_mv / 1000;
            int v_frac = (batt_mv % 1000) / 10;
            snprintf(buf_unit, sizeof(buf_unit), "%d.%02d V", v_int, v_frac);
            snprintf(buf_sub, sizeof(buf_sub), "Voltaje: %d mV", batt_mv);

            uint16_t span = (uint16_t)((270 * batt_soc) / 100);
            lv_arc_set_angles(s_arc_tile, 0, span);
            break;
        }
    }

    if (s_tile_idx == 3) {
        lv_obj_set_style_text_font(s_lbl_val, &ui_font_value_28, LV_PART_MAIN);
        lv_obj_align(s_lbl_val, LV_ALIGN_CENTER, 0, -10);
    } else {
        lv_obj_set_style_text_font(s_lbl_val, &ui_font_subhero_36, LV_PART_MAIN);
        lv_obj_align(s_lbl_val, LV_ALIGN_CENTER, 0, -15);
    }

    lv_label_set_text(s_lbl_val, buf_val);
    lv_label_set_text(s_lbl_unit, buf_unit);
    lv_label_set_text(s_lbl_sub, buf_sub);
}

static bool tiles_on_gesture(uint8_t g) {
    switch (g) {
        case 0x03: /* SWIPE_LEFT */
            if (s_tile_idx < 4) {
                s_tile_idx++;
                update_tile_view();
                return true;
            }
            return false; /* borde derecho */
        case 0x04: /* SWIPE_RIGHT */
            if (s_tile_idx > 0) {
                s_tile_idx--;
                update_tile_view();
                return true;
            } else {
                /* Borde izquierdo: volver a watchface */
                ui_router_pop(NAV_ANIM_SLIDE_RIGHT);
                return true;
            }
        case 0x01: /* SWIPE_UP */
            /* Volver a watchface */
            ui_router_home();
            return true;
        case 0x05: /* TAP: lanza app asociada */
            switch (s_tile_idx) {
                case 0: /* HR */
                    ui_router_push((ui_route_t){.id = ROUTE_APP_HR_SPOT, .param = 0}, NAV_ANIM_FADE);
                    break;
                case 2: /* Steps */
                case 3: /* Activity HAR */
                    ui_router_push((ui_route_t){.id = ROUTE_APP_ACTIVITY, .param = 0}, NAV_ANIM_FADE);
                    break;
                case 4: /* Battery */
                    ui_router_push((ui_route_t){.id = ROUTE_APP_BATTERY, .param = 0}, NAV_ANIM_FADE);
                    break;
                default:
                    break;
            }
            return true;
        default:
            return false;
    }
}

static bool tiles_on_button(int btn_evt) {
    ESP_LOGI(TAG, "btn_evt %d en tiles", btn_evt);
    return false;
}

void ui_tiles_register(void) {
    ui_route_desc_t desc = {
        .id         = ROUTE_TILES,
        .build      = tiles_build,
        .on_enter   = tiles_on_enter,
        .on_leave   = NULL,
        .tick       = tiles_tick,
        .on_button  = tiles_on_button,
        .on_gesture = tiles_on_gesture,
        .name       = "tiles",
    };
    ui_router_register(&desc);
}

/**
 * @file app_hr_spot.c
 * @brief Aplicación de medición de HR/SpO2 Spot (MAX30102).
 */
#include "app_hr_spot.h"
#include "ui_router.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "max30102.h"
#include "gpio_buttons.h"

#include "lvgl.h"
#include "esp_log.h"

#include <stdio.h>

#define TH (ui_theme_get())

static const char *TAG = "APP_HRSPOT";

/* Handles LVGL */
static lv_obj_t *s_scr             = NULL;
static lv_obj_t *s_lbl_title       = NULL;
static lv_obj_t *s_lbl_instr       = NULL;
static lv_obj_t *s_ring            = NULL;
static lv_obj_t *s_heart           = NULL;
static lv_obj_t *s_lbl_progress    = NULL;
static lv_obj_t *s_lbl_result      = NULL;
static lv_obj_t *s_lbl_quality     = NULL;

/* Estado de animación */
static bool s_heart_beating = false;

/* Callbacks de animación */
static void heart_opa_cb(void *obj, int32_t v) {
    lv_obj_set_style_text_opa((lv_obj_t *)obj, v, LV_PART_MAIN);
}

static void heart_ring_size_cb(void *obj, int32_t v) {
    lv_obj_set_size((lv_obj_t *)obj, v, v);
}

static void heart_ring_opa_cb(void *obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)obj, v, LV_PART_MAIN);
}

static void heart_start_beat(void) {
    if (s_heart_beating) return;
    s_heart_beating = true;
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_heart);
    lv_anim_set_exec_cb(&a, heart_opa_cb);
    lv_anim_set_values(&a, 150, 255);
    lv_anim_set_time(&a, 150);
    lv_anim_set_playback_time(&a, 600);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_t rs;
    lv_anim_init(&rs);
    lv_anim_set_var(&rs, s_ring);
    lv_anim_set_exec_cb(&rs, heart_ring_size_cb);
    lv_anim_set_values(&rs, 40, 140);
    lv_anim_set_time(&rs, 750);
    lv_anim_set_repeat_count(&rs, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&rs, lv_anim_path_ease_out);
    lv_anim_start(&rs);

    lv_anim_t ro;
    lv_anim_init(&ro);
    lv_anim_set_var(&ro, s_ring);
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
    lv_anim_del(s_heart, heart_opa_cb);
    lv_obj_set_style_text_opa(s_heart, 255, LV_PART_MAIN);

    lv_anim_del(s_ring, heart_ring_size_cb);
    lv_anim_del(s_ring, heart_ring_opa_cb);
    lv_obj_set_size(s_ring, 44, 44);
    lv_obj_set_style_opa(s_ring, 0, LV_PART_MAIN);
}

static const char *quality_str(max30102_spot_quality_t q) {
    switch (q) {
        case SPOT_QUALITY_GOOD: return "Calidad: BUENA";
        case SPOT_QUALITY_FAIR: return "Calidad: REGULAR";
        case SPOT_QUALITY_POOR: return "Calidad: POBRE";
        default: return "";
    }
}

static lv_obj_t *hrspot_build(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Título */
    s_lbl_title = lv_label_create(scr);
    lv_label_set_text(s_lbl_title, "MEDIDA HR/SPO2");
    lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_title, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_title, LV_ALIGN_TOP_MID, 0, 18);

    /* Instrucción inicial */
    s_lbl_instr = lv_label_create(scr);
    lv_label_set_text(s_lbl_instr, "Apoye el dedo sobre\nel sensor MAX30102.\n\nPulse SELECT o pantalla\npara iniciar.");
    lv_obj_set_style_text_color(s_lbl_instr, lv_color_hex(TH->text_dim), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_instr, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_lbl_instr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_instr, LV_ALIGN_TOP_MID, 0, 50);

    /* Anillo pulsante */
    s_ring = lv_obj_create(scr);
    lv_obj_set_size(s_ring, 44, 44);
    lv_obj_align(s_ring, LV_ALIGN_CENTER, 0, -38);
    lv_obj_set_style_radius(s_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ring, 4, 0);
    lv_obj_set_style_border_color(s_ring, lv_color_hex(TH->c_hr), 0);
    lv_obj_add_flag(s_ring, LV_OBJ_FLAG_HIDDEN);

    /* Corazón icono */
    s_heart = lv_label_create(scr);
    lv_label_set_text(s_heart, UI_SYM_HEART);
    lv_obj_set_style_text_color(s_heart, lv_color_hex(TH->c_hr), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_heart, &ui_font_icon_56, LV_PART_MAIN);
    lv_obj_align(s_heart, LV_ALIGN_CENTER, 0, -38);
    lv_obj_add_flag(s_heart, LV_OBJ_FLAG_HIDDEN);

    /* Progreso label */
    s_lbl_progress = lv_label_create(scr);
    lv_label_set_text(s_lbl_progress, "");
    lv_obj_set_style_text_color(s_lbl_progress, lv_color_hex(TH->warn), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_progress, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_progress, LV_ALIGN_CENTER, 0, 38);

    /* Resultado */
    s_lbl_result = lv_label_create(scr);
    lv_label_set_text(s_lbl_result, "");
    lv_obj_set_style_text_color(s_lbl_result, lv_color_hex(TH->text), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_result, &ui_font_subhero_36, LV_PART_MAIN);
    lv_obj_align(s_lbl_result, LV_ALIGN_CENTER, 0, 0);

    /* Calidad */
    s_lbl_quality = lv_label_create(scr);
    lv_label_set_text(s_lbl_quality, "");
    lv_obj_set_style_text_color(s_lbl_quality, lv_color_hex(TH->text_dim), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_quality, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_quality, LV_ALIGN_BOTTOM_MID, 0, -32);

    s_scr = scr;
    return scr;
}

static void hrspot_tick(void) {
    if (!s_scr) return;

    max30102_spot_status_t st;
    max30102_spot_get_status(&st);

    bool measuring = (st.state == SPOT_STATE_SETTLING || st.state == SPOT_STATE_MEASURING);
    if (measuring) {
        lv_obj_clear_flag(s_heart, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
        heart_start_beat();
    } else {
        heart_stop_beat();
        lv_obj_add_flag(s_heart, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
    }

    switch (st.state) {
        case SPOT_STATE_IDLE:
            lv_obj_clear_flag(s_lbl_instr, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_lbl_progress, "");
            lv_label_set_text(s_lbl_result, "");
            lv_label_set_text(s_lbl_quality, "");
            break;
        case SPOT_STATE_SETTLING:
            lv_obj_add_flag(s_lbl_instr, LV_OBJ_FLAG_HIDDEN);
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "Estabilizando %u%%", st.progress_pct);
                lv_label_set_text(s_lbl_progress, buf);
            }
            lv_label_set_text(s_lbl_result, "");
            lv_label_set_text(s_lbl_quality, "");
            break;
        case SPOT_STATE_MEASURING:
            lv_obj_add_flag(s_lbl_instr, LV_OBJ_FLAG_HIDDEN);
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "Midiendo %u%%", st.progress_pct);
                lv_label_set_text(s_lbl_progress, buf);
            }
            lv_label_set_text(s_lbl_result, "");
            lv_label_set_text(s_lbl_quality, "Quédese quieto");
            break;
        case SPOT_STATE_DONE:
            lv_obj_add_flag(s_lbl_instr, LV_OBJ_FLAG_HIDDEN);
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.1fs", st.duration_ms / 1000.0f);
                lv_label_set_text(s_lbl_progress, buf);
            }
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%u bpm  %u%%", st.bpm, st.spo2);
                lv_label_set_text(s_lbl_result, buf);
            }
            lv_label_set_text(s_lbl_quality, quality_str(st.quality));
            break;
        case SPOT_STATE_FAILED:
            lv_obj_add_flag(s_lbl_instr, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_lbl_progress, "Sin señal usable");
            lv_label_set_text(s_lbl_result, "");
            lv_label_set_text(s_lbl_quality, "Apoye bien el dedo");
            break;
        case SPOT_STATE_ABORTED:
            lv_obj_add_flag(s_lbl_instr, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_lbl_progress, "Cancelado");
            lv_label_set_text(s_lbl_result, "");
            lv_label_set_text(s_lbl_quality, "");
            break;
    }
}

static bool hrspot_on_button(int btn_evt) {
    if (btn_evt == BTN_EVENT_SELECT_SHORT) {
        max30102_spot_status_t st;
        max30102_spot_get_status(&st);
        if (st.state == SPOT_STATE_SETTLING || st.state == SPOT_STATE_MEASURING) {
            ESP_LOGI(TAG, "Aborting spot measurement via SELECT");
            max30102_spot_abort();
        } else {
            ESP_LOGI(TAG, "Starting spot measurement via SELECT");
            max30102_spot_start();
        }
        return true; /* consumed */
    }
    return false;
}

static bool hrspot_on_gesture(uint8_t g) {
    if (g == 0x05) { /* TAP */
        max30102_spot_status_t st;
        max30102_spot_get_status(&st);
        if (st.state == SPOT_STATE_SETTLING || st.state == SPOT_STATE_MEASURING) {
            ESP_LOGI(TAG, "Aborting spot measurement via TAP");
            max30102_spot_abort();
        } else {
            ESP_LOGI(TAG, "Starting spot measurement via TAP");
            max30102_spot_start();
        }
        return true;
    }
    return false;
}

static void hrspot_on_leave(void) {
    max30102_spot_status_t st;
    max30102_spot_get_status(&st);
    if (st.state == SPOT_STATE_SETTLING || st.state == SPOT_STATE_MEASURING) {
        ESP_LOGI(TAG, "Leaving route, aborting spot measurement");
        max30102_spot_abort();
    }
}

void app_hr_spot_register(void) {
    ui_route_desc_t desc = {
        .id         = ROUTE_APP_HR_SPOT,
        .build      = hrspot_build,
        .on_enter   = NULL,
        .on_leave   = hrspot_on_leave,
        .tick       = hrspot_tick,
        .on_button  = hrspot_on_button,
        .on_gesture = hrspot_on_gesture,
        .name       = "app_hr_spot",
    };
    ui_router_register(&desc);
}

/**
 * @file app_ecg.c
 * @brief Aplicación de ECG (AD8232).
 */
#include "app_ecg.h"
#include "ui_router.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "ble_telemetry.h"
#include "gpio_buttons.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "esp_log.h"

#include <math.h>
#include <stdio.h>

#define TH (ui_theme_get())

static const char *TAG = "APP_ECG";

#define ECG_PTS 61

/* Handles LVGL */
static lv_obj_t *s_scr             = NULL;
static lv_obj_t *s_lbl_title       = NULL;
static lv_obj_t *s_lbl_instr       = NULL;
static lv_obj_t *s_lbl_timer       = NULL;
static lv_obj_t *s_lbl_rec         = NULL;
static lv_obj_t *s_rec_circle      = NULL;
static lv_obj_t *s_wave_line       = NULL;

static lv_point_t s_ecg_pts[ECG_PTS];
static float s_ecg_phase = 0.0f;
static int64_t s_ecg_start_us = 0;
static bool s_rec_beating = false;

/* Callbacks de animación */
static void rec_circle_opa_cb(void *obj, int32_t v) {
    lv_obj_set_style_bg_opa((lv_obj_t *)obj, v, LV_PART_MAIN);
}

static void ecg_start_rec_anim(void) {
    if (s_rec_beating) return;
    s_rec_beating = true;
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_rec_circle);
    lv_anim_set_exec_cb(&a, rec_circle_opa_cb);
    lv_anim_set_values(&a, 30, 255);
    lv_anim_set_time(&a, 500);
    lv_anim_set_playback_time(&a, 500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void ecg_stop_rec_anim(void) {
    if (!s_rec_beating) return;
    s_rec_beating = false;
    lv_anim_del(s_rec_circle, rec_circle_opa_cb);
    lv_obj_set_style_bg_opa(s_rec_circle, LV_OPA_COVER, LV_PART_MAIN);
}

static float ecg_morph(float p) {
    float y = 0.0f;
    y += 0.12f * expf(-powf((p - 0.12f) / 0.030f, 2.0f));  /* P */
    y -= 0.10f * expf(-powf((p - 0.22f) / 0.012f, 2.0f));  /* Q */
    y += 1.00f * expf(-powf((p - 0.25f) / 0.012f, 2.0f));  /* R */
    y -= 0.25f * expf(-powf((p - 0.28f) / 0.014f, 2.0f));  /* S */
    y += 0.22f * expf(-powf((p - 0.45f) / 0.050f, 2.0f));  /* T */
    return y;
}

static void ecg_wave_update(void) {
    static uint32_t last_ms = 0;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    if (last_ms == 0) last_ms = now;
    
    const float W = 180.0f, cycles = 2.0f, amp = 34.0f;
    const float cy = 45.0f;
    s_ecg_phase += (now - last_ms) * 0.0006f;
    last_ms = now;
    if (s_ecg_phase > 1.0f) s_ecg_phase -= 1.0f;
    for (int i = 0; i < ECG_PTS; i++) {
        float fx = (float)i / (ECG_PTS - 1);
        float p = fx * cycles + s_ecg_phase;
        p -= (float)(int)p;
        s_ecg_pts[i].x = (lv_coord_t)(fx * W);
        s_ecg_pts[i].y = (lv_coord_t)(cy - ecg_morph(p) * amp);
    }
    lv_line_set_points(s_wave_line, s_ecg_pts, ECG_PTS);
}

static lv_obj_t *ecg_build(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Título */
    s_lbl_title = lv_label_create(scr);
    lv_label_set_text(s_lbl_title, "MODO ECG");
    lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_title, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_title, LV_ALIGN_TOP_MID, 0, 18);

    /* Instrucción inicial */
    s_lbl_instr = lv_label_create(scr);
    lv_label_set_text(s_lbl_instr, "Apoye los dedos sobre\nlos contactos metálicos.\n\nPulse SELECT o pantalla\npara iniciar.");
    lv_obj_set_style_text_color(s_lbl_instr, lv_color_hex(TH->text_dim), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_instr, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_lbl_instr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_instr, LV_ALIGN_TOP_MID, 0, 50);

    /* Timer */
    s_lbl_timer = lv_label_create(scr);
    lv_label_set_text(s_lbl_timer, "0:00");
    lv_obj_set_style_text_color(s_lbl_timer, lv_color_hex(TH->text), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_timer, &ui_font_subhero_36, LV_PART_MAIN);
    lv_obj_align(s_lbl_timer, LV_ALIGN_CENTER, 0, -25);
    lv_obj_add_flag(s_lbl_timer, LV_OBJ_FLAG_HIDDEN);

    /* Línea de onda */
    s_wave_line = lv_line_create(scr);
    lv_obj_set_size(s_wave_line, 180, 90);
    lv_obj_align(s_wave_line, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_line_color(s_wave_line, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_line_width(s_wave_line, 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(s_wave_line, true, LV_PART_MAIN);
    for (int i = 0; i < ECG_PTS; i++) {
        s_ecg_pts[i].x = (lv_coord_t)(i * 180 / (ECG_PTS - 1));
        s_ecg_pts[i].y = 45;
    }
    lv_line_set_points(s_wave_line, s_ecg_pts, ECG_PTS);
    lv_obj_add_flag(s_wave_line, LV_OBJ_FLAG_HIDDEN);

    /* REC Label */
    s_lbl_rec = lv_label_create(scr);
    lv_label_set_text(s_lbl_rec, "GRAV");
    lv_obj_set_style_text_color(s_lbl_rec, lv_color_hex(TH->alert), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_rec, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_rec, LV_ALIGN_BOTTOM_MID, 10, -32);
    lv_obj_add_flag(s_lbl_rec, LV_OBJ_FLAG_HIDDEN);

    /* REC Círculo */
    s_rec_circle = lv_obj_create(scr);
    lv_obj_set_size(s_rec_circle, 12, 12);
    lv_obj_align_to(s_rec_circle, s_lbl_rec, LV_ALIGN_OUT_LEFT_MID, -6, 0);
    lv_obj_set_style_radius(s_rec_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_rec_circle, lv_color_hex(TH->alert), 0);
    lv_obj_set_style_border_width(s_rec_circle, 0, 0);
    lv_obj_add_flag(s_rec_circle, LV_OBJ_FLAG_HIDDEN);

    s_scr = scr;
    return scr;
}

static void ecg_tick(void) {
    if (!s_scr) return;

    bool rec = ble_telemetry_is_ecg_mode_active();
    if (rec) {
        if (s_ecg_start_us == 0) s_ecg_start_us = esp_timer_get_time();
        uint32_t secs = (uint32_t)((esp_timer_get_time() - s_ecg_start_us) / 1000000ULL);
        char buf[16];
        snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
        lv_label_set_text(s_lbl_timer, buf);
        
        lv_obj_add_flag(s_lbl_instr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_lbl_timer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_lbl_rec, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_rec_circle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_wave_line, LV_OBJ_FLAG_HIDDEN);
        ecg_start_rec_anim();
        ecg_wave_update();
    } else {
        s_ecg_start_us = 0;
        lv_obj_clear_flag(s_lbl_instr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_lbl_timer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_lbl_rec, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_rec_circle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_wave_line, LV_OBJ_FLAG_HIDDEN);
        ecg_stop_rec_anim();
    }
}

static bool ecg_on_button(int btn_evt) {
    if (btn_evt == BTN_EVENT_SELECT_SHORT) {
        bool act = ble_telemetry_is_ecg_mode_active();
        ESP_LOGI(TAG, "Toggling ECG mode via SELECT: %d -> %d", act, !act);
        ble_telemetry_set_ecg_mode(!act);
        return true;
    }
    return false;
}

static bool ecg_on_gesture(uint8_t g) {
    if (g == 0x05) { /* TAP */
        bool act = ble_telemetry_is_ecg_mode_active();
        ESP_LOGI(TAG, "Toggling ECG mode via TAP: %d -> %d", act, !act);
        ble_telemetry_set_ecg_mode(!act);
        return true;
    }
    return false;
}

static void ecg_on_leave(void) {
    ESP_LOGI(TAG, "Leaving route, disabling ECG mode");
    ble_telemetry_set_ecg_mode(false);
}

void app_ecg_register(void) {
    ui_route_desc_t desc = {
        .id         = ROUTE_APP_ECG,
        .build      = ecg_build,
        .on_enter   = NULL,
        .on_leave   = ecg_on_leave,
        .tick       = ecg_tick,
        .on_button  = ecg_on_button,
        .on_gesture = ecg_on_gesture,
        .name       = "app_ecg",
    };
    ui_router_register(&desc);
}

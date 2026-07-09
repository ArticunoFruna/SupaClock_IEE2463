/**
 * @file app_activity.c
 * @brief Aplicación de Actividad con histograma HAR.
 */
#include "app_activity.h"
#include "ui_router.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "app_state.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "esp_log.h"

#include <stdio.h>

#define TH (ui_theme_get())

static const char *TAG = "APP_ACTIVITY";

#define HIST_SIZE 30
static uint8_t s_activity_history[HIST_SIZE] = {0};
static int s_history_head = 0;
static int s_history_count = 0;
static esp_timer_handle_t s_activity_timer = NULL;
static bool s_timer_started = false;

static lv_obj_t *s_chart = NULL;
static lv_chart_series_t *s_ser = NULL;
static lv_obj_t *s_lbl_summary = NULL;

static void activity_timer_cb(void *arg) {
    shared_sensor_data_t *st = app_state_lock(5);
    uint8_t current_state = 0;
    if (st) {
        current_state = st->har_state;
        app_state_unlock();
    }
    s_activity_history[s_history_head] = current_state;
    s_history_head = (s_history_head + 1) % HIST_SIZE;
    if (s_history_count < HIST_SIZE) s_history_count++;
}

static void update_activity_ui(void) {
    if (!s_chart || !s_ser) return;

    int idx = (s_history_head - s_history_count + HIST_SIZE) % HIST_SIZE;
    int resting_count = 0, walking_count = 0, running_count = 0;

    for (int i = 0; i < HIST_SIZE; i++) {
        uint8_t chart_val = 0;
        if (i < s_history_count) {
            uint8_t raw = s_activity_history[idx];
            idx = (idx + 1) % HIST_SIZE;
            if (raw == 0) { resting_count++; chart_val = 1; }
            else if (raw == 1) { walking_count++; chart_val = 2; }
            else if (raw == 2) { running_count++; chart_val = 3; }
            else if (raw == 3) { chart_val = 4; }
        }
        lv_chart_set_value_by_id(s_chart, s_ser, i, chart_val);
    }
    lv_chart_refresh(s_chart);

    if (s_lbl_summary) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Reposo: %d' | Cam: %d'\nCorr: %d'", 
                 resting_count, walking_count, running_count);
        lv_label_set_text(s_lbl_summary, buf);
    }
}

static lv_obj_t *activity_build(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Título */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ACTIVIDAD (30M)");
    lv_obj_set_style_text_color(title, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    /* Crear Chart */
    s_chart = lv_chart_create(scr);
    lv_obj_set_size(s_chart, 140, 90);
    lv_obj_align(s_chart, LV_ALIGN_CENTER, 20, -10);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(s_chart, HIST_SIZE);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 4);
    lv_chart_set_div_line_count(s_chart, 5, 0);

    /* Estilo del chart */
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(s_chart, 2, LV_PART_MAIN);

    /* Series */
    s_ser = lv_chart_add_series(s_chart, lv_color_hex(TH->c_activity), LV_CHART_AXIS_PRIMARY_Y);

    /* Labels de Y en la izquierda */
    lv_obj_t *lbl_r = lv_label_create(scr);
    lv_label_set_text(lbl_r, "Esc\nCorr\nCam\nRep");
    lv_obj_set_style_text_color(lbl_r, lv_color_hex(TH->text_dim), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_r, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_r, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align_to(lbl_r, s_chart, LV_ALIGN_OUT_LEFT_MID, -6, 0);

    /* Summary label */
    s_lbl_summary = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_summary, lv_color_hex(TH->text), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_summary, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_lbl_summary, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_summary, LV_ALIGN_BOTTOM_MID, 0, -20);

    update_activity_ui();

    return scr;
}

static void activity_tick(void) {
    update_activity_ui();
}

void app_activity_register(void) {
    if (!s_timer_started) {
        const esp_timer_create_args_t args = {
            .callback = activity_timer_cb,
            .name = "activity_timer"
        };
        if (esp_timer_create(&args, &s_activity_timer) == ESP_OK) {
            esp_timer_start_periodic(s_activity_timer, 60ULL * 1000000ULL); /* 1 minuto */
            s_timer_started = true;
            ESP_LOGI(TAG, "Activity logging timer started");
        }
    }

    ui_route_desc_t desc = {
        .id         = ROUTE_APP_ACTIVITY,
        .build      = activity_build,
        .on_enter   = NULL,
        .on_leave   = NULL,
        .tick       = activity_tick,
        .on_button  = NULL,
        .on_gesture = NULL,
        .name       = "app_activity",
    };
    ui_router_register(&desc);
}

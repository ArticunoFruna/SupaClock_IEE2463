/**
 * @file app_temp.c
 * @brief Aplicación de Temperatura con sparkline de historial.
 */
#include "app_temp.h"
#include "ui_router.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "app_state.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "esp_log.h"

#include <stdio.h>

#define TH (ui_theme_get())

static const char *TAG = "APP_TEMP";

#define HIST_SIZE 30
static float s_temp_history[HIST_SIZE] = {0.0f};
static int s_history_head = 0;
static int s_history_count = 0;
static esp_timer_handle_t s_temp_timer = NULL;
static bool s_timer_started = false;

static lv_obj_t *s_chart = NULL;
static lv_chart_series_t *s_ser = NULL;
static lv_obj_t *s_lbl_value = NULL;

static void temp_timer_cb(void *arg) {
    shared_sensor_data_t *st = app_state_lock(5);
    float current_val = 0.0f;
    if (st) {
        current_val = st->temperature_c;
        app_state_unlock();
    }
    s_temp_history[s_history_head] = current_val;
    s_history_head = (s_history_head + 1) % HIST_SIZE;
    if (s_history_count < HIST_SIZE) s_history_count++;
}

static void update_temp_ui(void) {
    float cur_t = 0.0f;
    shared_sensor_data_t *st = app_state_lock(5);
    if (st) {
        cur_t = st->temperature_c;
        app_state_unlock();
    }

    if (s_lbl_value) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f °C", cur_t);
        lv_label_set_text(s_lbl_value, buf);
    }

    if (!s_chart || !s_ser) return;

    float min_val = 100.0f, max_val = -100.0f;
    int idx = (s_history_head - s_history_count + HIST_SIZE) % HIST_SIZE;

    for (int i = 0; i < s_history_count; i++) {
        float val = s_temp_history[idx];
        idx = (idx + 1) % HIST_SIZE;
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    if (min_val > max_val) {
        min_val = 35.0f;
        max_val = 38.0f;
    } else if (max_val - min_val < 1.0f) {
        min_val -= 0.5f;
        max_val += 0.5f;
    }

    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, (int)(min_val * 10), (int)(max_val * 10));

    idx = (s_history_head - s_history_count + HIST_SIZE) % HIST_SIZE;
    for (int i = 0; i < HIST_SIZE; i++) {
        if (i < s_history_count) {
            float val = s_temp_history[idx];
            idx = (idx + 1) % HIST_SIZE;
            lv_chart_set_value_by_id(s_chart, s_ser, i, (int)(val * 10));
        } else {
            lv_chart_set_value_by_id(s_chart, s_ser, i, (int)(cur_t * 10));
        }
    }
    lv_chart_refresh(s_chart);
}

static lv_obj_t *temp_build(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Título */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "TEMPERATURA");
    lv_obj_set_style_text_color(title, lv_color_hex(TH->accent), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    /* Valor actual */
    s_lbl_value = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_value, lv_color_hex(TH->c_temp), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_value, &ui_font_subhero_36, LV_PART_MAIN);
    lv_obj_align(s_lbl_value, LV_ALIGN_CENTER, 0, -25);

    /* Sparkline Chart */
    s_chart = lv_chart_create(scr);
    lv_obj_set_size(s_chart, 170, 75);
    lv_obj_align(s_chart, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, HIST_SIZE);
    
    /* Estilo del sparkline */
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_line_width(s_chart, 3, LV_PART_ITEMS);

    /* Series */
    s_ser = lv_chart_add_series(s_chart, lv_color_hex(TH->c_temp), LV_CHART_AXIS_PRIMARY_Y);

    update_temp_ui();

    return scr;
}

static void temp_tick(void) {
    update_temp_ui();
}

void app_temp_register(void) {
    if (!s_timer_started) {
        const esp_timer_create_args_t args = {
            .callback = temp_timer_cb,
            .name = "temp_timer"
        };
        if (esp_timer_create(&args, &s_temp_timer) == ESP_OK) {
            esp_timer_start_periodic(s_temp_timer, 60ULL * 1000000ULL); /* 1 minuto */
            s_timer_started = true;
            ESP_LOGI(TAG, "Temperature logging timer started");
        }
    }

    ui_route_desc_t desc = {
        .id         = ROUTE_APP_TEMP,
        .build      = temp_build,
        .on_enter   = NULL,
        .on_leave   = NULL,
        .tick       = temp_tick,
        .on_button  = NULL,
        .on_gesture = NULL,
        .name       = "app_temp",
    };
    ui_router_register(&desc);
}

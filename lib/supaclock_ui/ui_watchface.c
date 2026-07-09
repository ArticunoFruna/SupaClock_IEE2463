/**
 * @file ui_watchface.c
 * @brief Watchface SPORT — dos rings bicromáticos (STEPS izq / HR der) y
 *        4 complications alrededor del reloj central. Ver plan §5.
 *
 * Colores hardcoded (esta face NO usa TH->accent — cada métrica tiene su
 * color semántico fijo para replicar la ref del usuario):
 *   - STEPS: teal base #4ECDC4, indicador mint #88E5D8
 *   - BATT:  blanco (icono cambia según SoC)
 *   - KCAL:  naranja #FF6B33
 *   - HR:    rojo #FF3333, indicador arc gradient rojo→naranja
 * bg y text/text_dim sí vienen del tema (para respetar el AMOLED vs mono).
 */
#include "ui_watchface.h"
#include "ui_router.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "app_state.h"

#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "UI_WF";

#define TH  (ui_theme_get())

/* Colores semánticos fijos (independientes del theme) */
#define COL_STEPS_BASE   0x1A5030    /* verde bosque (background arc) */
#define COL_STEPS_HIGH   0x30D158    /* verde vibrante (indicador + label) */
#define COL_HR_BASE      0x3B0000
#define COL_HR_HIGH      0xFF3333
#define COL_BATT_OK      0xFFFFFF
#define COL_BATT_LOW     0xFF3B30

/* Metas / rangos para los progress rings */
#define STEPS_GOAL       8000
#define HR_MIN_ZONE      55       /* reposo esperado */
#define HR_MAX_ZONE      170      /* cardio alto */

/* Handles LVGL */
static lv_obj_t *s_scr        = NULL;
static lv_obj_t *s_arc_steps  = NULL;
static lv_obj_t *s_arc_hr     = NULL;
static lv_obj_t *s_lbl_steps  = NULL;
static lv_obj_t *s_lbl_batt   = NULL;
static lv_obj_t *s_ico_batt   = NULL;
static lv_obj_t *s_lbl_time   = NULL;
static lv_obj_t *s_lbl_date   = NULL;
static lv_obj_t *s_lbl_hr     = NULL;
static lv_obj_t *s_ico_har    = NULL;

/* Cache para evitar refresh redundante */
static int   s_last_minute = -1;
static int   s_last_day    = -1;
static int   s_last_batt   = -1;
static int   s_last_charging = -1;   /* -1 = force refresh, 0/1 = last state */
static int   s_last_hr     = -1;
static int   s_last_steps  = -1;
static int   s_last_har    = -1;

static inline uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static const char *dow_short(int wday) {
    static const char *n[7] = {"Dom","Lun","Mar","Mié","Jue","Vie","Sáb"};
    if (wday < 0 || wday > 6) return "---";
    return n[wday];
}

static const char *battery_symbol(int soc, bool charging) {
    /* Cuando carga, sobrepone el rayo — más informativo que el nivel exacto
     * mientras el usuario ve el cable conectado. */
    if (charging) return UI_SYM_BOLT;
    if (soc < 15)  return UI_SYM_BATTERY_EMPTY;
    if (soc < 40)  return UI_SYM_BATTERY_1;
    if (soc < 65)  return UI_SYM_BATTERY_2;
    if (soc < 90)  return UI_SYM_BATTERY_3;
    return UI_SYM_BATTERY_FULL;
}

static lv_obj_t *build_arc(lv_obj_t *parent, bool is_left,
                           uint32_t bg_color, uint32_t indicator_color) {
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, 236, 236);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, is_left ? 90 : 270);
    lv_arc_set_bg_angles(arc, 10, 170);
    lv_arc_set_angles(arc, 10, 10);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(bg_color),        LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(indicator_color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    return arc;
}

static lv_obj_t *wf_build(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Rings decorativos con progress */
    s_arc_steps = build_arc(scr, /*is_left=*/true,  COL_STEPS_BASE, COL_STEPS_HIGH);
    s_arc_hr    = build_arc(scr, /*is_left=*/false, COL_HR_BASE,    COL_HR_HIGH);

    /* Top-left: [icon] STEPS_COUNT */
    s_lbl_steps = lv_label_create(scr);
    lv_label_set_text(s_lbl_steps, UI_SYM_STEPS " 0");
    lv_obj_set_style_text_color(s_lbl_steps, lv_color_hex(COL_STEPS_HIGH), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_steps, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_steps, LV_ALIGN_TOP_MID, -40, 44);

    /* Top-right: [batt icon] % */
    s_ico_batt = lv_label_create(scr);
    lv_label_set_text(s_ico_batt, UI_SYM_BATTERY_FULL);
    lv_obj_set_style_text_color(s_ico_batt, lv_color_hex(COL_BATT_OK), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_ico_batt, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_ico_batt, LV_ALIGN_TOP_MID, 24, 44);

    s_lbl_batt = lv_label_create(scr);
    lv_label_set_text(s_lbl_batt, "--%");
    lv_obj_set_style_text_color(s_lbl_batt, lv_color_hex(COL_BATT_OK), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_batt, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_batt, LV_ALIGN_TOP_MID, 54, 44);

    /* Hero time HH:MM */
    s_lbl_time = lv_label_create(scr);
    lv_label_set_text(s_lbl_time, "--:--");
    lv_obj_set_style_text_color(s_lbl_time, lv_color_hex(TH->text), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_time, &ui_font_hero_56, LV_PART_MAIN);
    lv_obj_align(s_lbl_time, LV_ALIGN_CENTER, 0, -8);

    /* Date debajo */
    s_lbl_date = lv_label_create(scr);
    lv_label_set_text(s_lbl_date, "--/-- ---");
    lv_obj_set_style_text_color(s_lbl_date, lv_color_hex(TH->text_dim), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_date, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_date, LV_ALIGN_CENTER, 0, 30);

    /* Icono HAR debajo de la fecha */
    s_ico_har = lv_label_create(scr);
    lv_label_set_text(s_ico_har, "");
    lv_obj_set_style_text_color(s_ico_har, lv_color_hex(TH->c_activity), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_ico_har, &ui_font_value_28, LV_PART_MAIN);
    lv_obj_align(s_ico_har, LV_ALIGN_CENTER, 0, 52);

    /* Right-mid: HR value + heart placeholder */
    s_lbl_hr = lv_label_create(scr);
    lv_label_set_text(s_lbl_hr, "");
    lv_obj_set_style_text_color(s_lbl_hr, lv_color_hex(COL_HR_HIGH), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_lbl_hr, &ui_font_label_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_hr, LV_ALIGN_RIGHT_MID, -22, 26);

    /* Forzar refresh en el primer tick */
    s_last_minute = -1;
    s_last_day    = -1;
    s_last_batt   = -1;
    s_last_hr     = -1;
    s_last_steps  = -1;
    s_last_har    = -1;

    s_scr = scr;
    return scr;
}

/* ── Tick ── */

static void set_arc_progress(lv_obj_t *arc, int percent, bool is_left) {
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    /* bg_angles = [10, 170] → span útil = 160°. Indicator rellena desde el
     * extremo cercano al top (170 en el left, 10 en el right por simetría
     * del rotation) hacia el fondo. Para dar la impresión de "llenado desde
     * arriba" tanto a izq como a der. */
    uint16_t span = (uint16_t)((160 * percent) / 100);
    if (is_left) {
        /* Left arc: rotation=90, ángulo 170 = top. Rellenar de 170-span → 170. */
        lv_arc_set_angles(arc, (uint16_t)(170 - span), 170);
    } else {
        /* Right arc: rotation=270, ángulo 10 = top. Rellenar 10 → 10+span. */
        lv_arc_set_angles(arc, 10, (uint16_t)(10 + span));
    }
}

static void wf_tick(void) {
    if (!s_scr) return;

    /* Tiempo del sistema */
    time_t now_s = 0;
    time(&now_s);
    struct tm tmv;
    localtime_r(&now_s, &tmv);

    if (tmv.tm_min != s_last_minute) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
        lv_label_set_text(s_lbl_time, buf);
        s_last_minute = tmv.tm_min;
    }
    if (tmv.tm_mday != s_last_day) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%02d/%02d %s",
                 tmv.tm_mday, tmv.tm_mon + 1, dow_short(tmv.tm_wday));
        lv_label_set_text(s_lbl_date, buf);
        s_last_day = tmv.tm_mday;
    }

    int batt = -1, hr = -1;
    uint32_t hr_updated = 0;
    uint32_t steps = 0;
    uint8_t har_val = 0;
    bool charging = false;
    shared_sensor_data_t *st = app_state_lock(5);
    if (st) {
        batt = (int)(st->battery_soc + 0.5f);
        hr = (int)st->hr_bpm;
        hr_updated = st->hr_updated_ms;
        steps = st->steps_sw;
        har_val = st->har_state;
        charging = st->battery_charging;
        app_state_unlock();
    }

    /* HAR activity icon */
    if ((int)har_val != s_last_har) {
        const char *har_sym = "";
        switch (har_val) {
            case 0: har_sym = UI_SYM_RESTING; break;     /* RESTING */
            case 1: har_sym = UI_SYM_WALKING; break;     /* WALKING */
            case 2: har_sym = UI_SYM_RUNNING; break;     /* RUNNING */
            case 3: har_sym = UI_SYM_STAIRS;  break;     /* STAIRS */
            default: har_sym = ""; break;
        }
        lv_label_set_text(s_ico_har, har_sym);
        s_last_har = (int)har_val;
    }

    /* Battery — refresca si cambió soc o estado de carga */
    if (batt >= 0) {
        if (batt > 100) batt = 100;
        int chg = charging ? 1 : 0;
        if (batt != s_last_batt || chg != s_last_charging) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d%%", batt);
            lv_label_set_text(s_lbl_batt, buf);
            lv_label_set_text(s_ico_batt, battery_symbol(batt, charging));
            /* Color: rojo si <15% y NO cargando, verde/accent si cargando,
             * blanco en el resto. */
            uint32_t c;
            if (charging)          c = 0x30D158;  /* verde Apple, indica carga */
            else if (batt < 15)    c = COL_BATT_LOW;
            else                   c = COL_BATT_OK;
            lv_obj_set_style_text_color(s_ico_batt, lv_color_hex(c), LV_PART_MAIN);
            lv_obj_set_style_text_color(s_lbl_batt, lv_color_hex(c), LV_PART_MAIN);
            s_last_batt = batt;
            s_last_charging = chg;
        }
    }

    /* Steps + steps arc */
    int steps_int = (int)steps;
    if (steps_int != s_last_steps) {
        char buf[16];
        snprintf(buf, sizeof(buf), UI_SYM_STEPS " %d", steps_int);
        lv_label_set_text(s_lbl_steps, buf);
        int pct = (steps_int * 100) / STEPS_GOAL;
        set_arc_progress(s_arc_steps, pct, /*is_left=*/true);
        s_last_steps = steps_int;
    }

    /* HR + HR arc */
    uint32_t age_ms = now_ms() - hr_updated;
    bool hr_valid = (hr_updated != 0 && age_ms < 60000 && hr > 0);
    int hr_display = hr_valid ? hr : -1;
    if (hr_display != s_last_hr) {
        if (hr_display > 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d " UI_SYM_HEART, hr_display);
            lv_label_set_text(s_lbl_hr, buf);
            int hr_pct = ((hr_display - HR_MIN_ZONE) * 100) /
                         (HR_MAX_ZONE - HR_MIN_ZONE);
            if (hr_pct < 0)   hr_pct = 0;
            if (hr_pct > 100) hr_pct = 100;
            set_arc_progress(s_arc_hr, hr_pct, /*is_left=*/false);
        } else {
            lv_label_set_text(s_lbl_hr, "");
            set_arc_progress(s_arc_hr, 0, /*is_left=*/false);
        }
        s_last_hr = hr_display;
    }

}

/* ── Gesture / button dispatch (log-only, rutas futuras) ── */

static bool wf_on_gesture(uint8_t g) {
    switch (g) {
        case 0x03: {
            ui_route_t r = { .id = ROUTE_TILES, .param = 0 };
            ui_router_push(r, NAV_ANIM_SLIDE_LEFT);
            return true;
        }
        case 0x04: ESP_LOGI(TAG, "SWIPE_RIGHT → (TODO) notif"); return true;
        case 0x01: {
            ui_route_t r = { .id = ROUTE_APP_DRAWER, .param = 0 };
            ui_router_push(r, NAV_ANIM_SLIDE_UP);
            return true;
        }
        case 0x02: {
            ui_route_t r = { .id = ROUTE_QUICK_PANEL, .param = 0 };
            ui_router_push(r, NAV_ANIM_SLIDE_DOWN);
            return true;
        }
        case 0x05: ESP_LOGI(TAG, "TAP"); return true;
        case 0x0B: ESP_LOGI(TAG, "DOUBLE_TAP → (TODO) HR_SPOT"); return true;
        case 0x0C: ESP_LOGI(TAG, "LONG_PRESS → (TODO) face picker"); return true;
        default:   return false;
    }
}

static bool wf_on_button(int btn_evt) {
    ESP_LOGI(TAG, "btn_evt %d en watchface", btn_evt);
    return false;
}

static void wf_on_enter(int32_t param) {
    (void)param;
    s_last_minute = -1;
    s_last_day    = -1;
    s_last_batt   = -1;
    s_last_hr     = -1;
    s_last_steps  = -1;
}

/* Restyle in-place al cambio de tema. Arcs, batt/hr/kcal/steps son paleta
 * SPORT fija (no cambian). Solo cambia bg, texto tiempo/fecha (van del
 * tema), y el HAR icon (c_activity = accent en mono). */
static void wf_on_theme_change(void) {
    if (!s_scr) return;
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    if (s_lbl_time) lv_obj_set_style_text_color(s_lbl_time, lv_color_hex(TH->text),       LV_PART_MAIN);
    if (s_lbl_date) lv_obj_set_style_text_color(s_lbl_date, lv_color_hex(TH->text_dim),   LV_PART_MAIN);
    if (s_ico_har)  lv_obj_set_style_text_color(s_ico_har,  lv_color_hex(TH->c_activity), LV_PART_MAIN);
    lv_obj_invalidate(s_scr);
}

void ui_watchface_register(void) {
    ui_route_desc_t desc = {
        .id              = ROUTE_WATCHFACE,
        .build           = wf_build,
        .on_enter        = wf_on_enter,
        .on_leave        = NULL,
        .tick            = wf_tick,
        .on_button       = wf_on_button,
        .on_gesture      = wf_on_gesture,
        .on_theme_change = wf_on_theme_change,
        .name            = "watchface_sport",
    };
    ui_router_register(&desc);
}

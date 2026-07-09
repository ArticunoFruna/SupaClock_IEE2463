/**
 * @file supaclock_ui.c
 * @brief UI touch-first (Galaxy-Watch-style) — Fase 2 TEST STUB.
 *
 * Este archivo es la reescritura mínima de la UI para el env main_app. Corre
 * en paralelo con la UI vieja (lib/supaclock_ui_notouch/) que sigue siendo la
 * firmware de producción hasta que esta touch UI madure.
 *
 * Fase 2 (actual): inicializa LVGL + panel + touch indev, muestra una
 * watchface stub con "SUPACLOCK — TOUCH READY", y loguea todos los
 * gestos que recibe. Verificamos el pipeline completo antes de agregar
 * navegación / tiles / apps.
 *
 * API pública preservada (ui_init, ui_tick, etc.) para que supaclock_app.c
 * no requiera cambios de include.
 */
#include "supaclock_ui.h"
#include "ui_router.h"
#include "ui_watchface.h"
#include "ui_tiles.h"
#include "ui_drawer.h"
#include "ui_apps/app_hr_spot.h"
#include "ui_apps/app_ecg.h"
#include "ui_apps/app_activity.h"
#include "ui_apps/app_temp.h"
#include "ui_apps/app_battery.h"
#include "ui_apps/app_ble.h"
#include "ui_apps/app_power.h"
#include "ui_apps/app_about.h"
#include "ui_apps/app_stubs.h"
#include "ui_quickpanel.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "lvgl.h"
#include "gc9a01.h"
#include "cst816s.h"
#include "app_state.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <stdatomic.h>
#include <string.h>

static const char *TAG = "UI";

/* ────── LVGL buffers (RAM interna, mismos tamaños que notouch) ────── */
#define DISP_BUF_SIZE   (240 * 48)
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t         s_buf_1[DISP_BUF_SIZE];
static lv_color_t         s_buf_2[DISP_BUF_SIZE];
static lv_disp_drv_t      s_disp_drv;
static lv_indev_drv_t     s_indev_drv;

/* ────── Touch activity flag (idéntico patrón que notouch) ────── */
static atomic_bool s_touch_activity = ATOMIC_VAR_INIT(false);
static uint8_t     s_last_gesture   = 0;

void ui_notify_touch_activity(void) { atomic_store(&s_touch_activity, true); }
bool ui_take_and_clear_touch_activity(void) {
    return atomic_exchange(&s_touch_activity, false);
}

/* ────── UI actions (power off / battery reset) ────── */
static ui_actions_t s_actions = {0};
void ui_set_actions(const ui_actions_t *actions) {
    if (actions) s_actions = *actions;
}

void ui_action_power_off(void) {
    if (s_actions.on_power_off) s_actions.on_power_off();
}

void ui_action_battery_reset(void) {
    if (s_actions.on_battery_reset) s_actions.on_battery_reset();
}

/* ────── LVGL callbacks ────── */

static void display_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                             lv_color_t *color_p) {
    uint16_t x = area->x1, y = area->y1;
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    gc9a01_draw_bitmap(x, y, w, h, (const uint16_t *)color_p);
    lv_disp_flush_ready(drv);
}

static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    cst816s_touch_t t;
    cst816s_read(&t);

    data->state   = t.pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    data->point.x = t.x;
    data->point.y = t.y;

    /* Gesture edge-dispatch al router. */
    bool gesture_edge = (t.gesture != 0 && t.gesture != s_last_gesture);
    if (gesture_edge) {
        ui_router_deliver_gesture(t.gesture);
    }
    s_last_gesture = t.gesture;

    /* Actividad SOLO si el dedo está apoyado o si acaba de cambiar el gesture.
     * ANTES: `t.pressed || t.gesture` → si el chip devuelve un gesture stale
     * (mismo valor en cada poll), el flag se dispara a 60 Hz y el auto-off
     * nunca vence porque `last_activity_ms` se resetea en cada gui_task cycle. */
    if (t.pressed || gesture_edge) ui_notify_touch_activity();
}

/* ────── Public API ────── */

void ui_init(void) {
    ESP_LOGI(TAG, "ui_init (touch stub Fase 2)");

    lv_init();

    lv_disp_draw_buf_init(&s_draw_buf, s_buf_1, s_buf_2, DISP_BUF_SIZE);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res  = 240;
    s_disp_drv.ver_res  = 240;
    s_disp_drv.flush_cb = display_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&s_indev_drv);

    ui_router_init();
    ui_watchface_register();
    ui_tiles_register();
    ui_drawer_register();
    app_hr_spot_register();
    app_ecg_register();
    app_activity_register();
    app_temp_register();
    app_battery_register();
    app_ble_register();
    app_power_register();
    app_about_register();
    app_stubs_register();
    ui_quickpanel_register();

    ui_route_t r = { .id = ROUTE_WATCHFACE, .param = 0 };
    ui_router_push(r, NAV_ANIM_NONE);
}

void ui_handle_button(btn_event_t ev) {
    if (!ui_router_deliver_button((int)ev)) {
        switch (ev) {
            case BTN_EVENT_SELECT_SHORT:
                ui_router_home();
                break;
            case BTN_EVENT_NEXT_SHORT:
                ui_router_pop(NAV_ANIM_SLIDE_RIGHT);
                break;
            case BTN_EVENT_SELECT_LONG:
                ui_router_push((ui_route_t){.id = ROUTE_APP_POWER, .param = 0}, NAV_ANIM_FADE);
                break;
            case BTN_EVENT_NEXT_LONG:
                if (ui_router_current().id == ROUTE_QUICK_PANEL) {
                    ui_router_pop(NAV_ANIM_SLIDE_UP);
                } else {
                    ui_router_push((ui_route_t){.id = ROUTE_QUICK_PANEL, .param = 0}, NAV_ANIM_SLIDE_DOWN);
                }
                break;
            default:
                break;
        }
    }
}

uint32_t ui_tick(void) {
    ui_router_tick();
    uint32_t next = lv_timer_handler();
    if (next < 5)   next = 5;
    if (next > 100) next = 100;
    return next;
}

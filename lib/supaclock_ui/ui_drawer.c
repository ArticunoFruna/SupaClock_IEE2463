/**
 * @file ui_drawer.c
 * @brief Menú App Drawer (grid de aplicaciones) con soporte para gestos.
 */
#include "ui_drawer.h"
#include "ui_router.h"
#include "ui_theme.h"
#include "ui_fonts.h"

#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "UI_DRAWER";

#define TH (ui_theme_get())

static lv_obj_t *s_scr = NULL;

static void btn_event_cb(lv_event_t *e) {
    ui_route_id_t route_id = (ui_route_id_t)(intptr_t)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Lanzando app route %d", (int)route_id);
    ui_router_push((ui_route_t){.id = route_id, .param = 0}, NAV_ANIM_FADE);
}

static lv_obj_t *drawer_build(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Grid layout 3x3 usando flex row wrap */
    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_set_size(grid, 220, 220);
    lv_obj_center(grid);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(grid, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_row(grid, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_column(grid, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLL_ELASTIC);

    const char *app_symbols[9] = {
        UI_SYM_HEART,        /* HR */
        UI_SYM_HEARTBEAT,    /* ECG */
        UI_SYM_RUNNING,      /* Activity */
        UI_SYM_THERMOMETER,  /* Temp */
        UI_SYM_BATTERY_FULL, /* Battery */
        UI_SYM_BLUETOOTH,    /* BLE */
        UI_SYM_COG,          /* Settings */
        UI_SYM_POWER,        /* Power */
        UI_SYM_INFO          /* About */
    };

    ui_route_id_t app_routes[9] = {
        ROUTE_APP_HR_SPOT,
        ROUTE_APP_ECG,
        ROUTE_APP_ACTIVITY,
        ROUTE_APP_TEMP,
        ROUTE_APP_BATTERY,
        ROUTE_APP_BLE,
        ROUTE_QUICK_PANEL,
        ROUTE_APP_POWER,
        ROUTE_APP_ABOUT
    };

    for (int i = 0; i < 9; i++) {
        lv_obj_t *btn = lv_btn_create(grid);
        lv_obj_set_size(btn, 56, 56);
        lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn, lv_color_hex(TH->surface), LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);

        /* Icono / Símbolo */
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, app_symbols[i]);
        lv_obj_set_style_text_font(lbl, &ui_font_value_28, LV_PART_MAIN);

        /* Color semántico según la app */
        uint32_t icon_col = TH->accent;
        switch (app_routes[i]) {
            case ROUTE_APP_HR_SPOT:
            case ROUTE_APP_ECG:
                icon_col = TH->c_hr;
                break;
            case ROUTE_APP_ACTIVITY:
                icon_col = TH->c_activity;
                break;
            case ROUTE_APP_TEMP:
                icon_col = TH->c_temp;
                break;
            case ROUTE_APP_BATTERY:
                icon_col = TH->c_batt;
                break;
            default:
                icon_col = TH->accent;
                break;
        }
        lv_obj_set_style_text_color(lbl, lv_color_hex(icon_col), LV_PART_MAIN);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)app_routes[i]);
    }

    s_scr = scr;
    return scr;
}

static bool drawer_on_gesture(uint8_t g) {
    switch (g) {
        case 0x02: /* SWIPE_DOWN: volver a watchface */
            ui_router_pop(NAV_ANIM_SLIDE_DOWN);
            return true;
        default:
            return false;
    }
}

void ui_drawer_register(void) {
    ui_route_desc_t desc = {
        .id         = ROUTE_APP_DRAWER,
        .build      = drawer_build,
        .on_enter   = NULL,
        .on_leave   = NULL,
        .tick       = NULL,
        .on_button  = NULL,
        .on_gesture = drawer_on_gesture,
        .name       = "drawer",
    };
    ui_router_register(&desc);
}

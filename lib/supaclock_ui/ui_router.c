#include "ui_router.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UI_ROUTER";

#define ROUTER_STACK_MAX  8

static ui_route_desc_t s_descs[ROUTE_COUNT];
static bool            s_registered[ROUTE_COUNT];
static lv_obj_t       *s_scr[ROUTE_COUNT];

static ui_route_t s_stack[ROUTER_STACK_MAX];
static uint8_t    s_depth = 0;

/* ────── Helpers ────── */

static lv_scr_load_anim_t map_anim(ui_nav_anim_t a) {
    switch (a) {
        case NAV_ANIM_SLIDE_LEFT:  return LV_SCR_LOAD_ANIM_MOVE_LEFT;
        case NAV_ANIM_SLIDE_RIGHT: return LV_SCR_LOAD_ANIM_MOVE_RIGHT;
        case NAV_ANIM_SLIDE_UP:    return LV_SCR_LOAD_ANIM_MOVE_TOP;
        case NAV_ANIM_SLIDE_DOWN:  return LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
        case NAV_ANIM_FADE:        return LV_SCR_LOAD_ANIM_FADE_ON;
        case NAV_ANIM_NONE:
        default:                    return LV_SCR_LOAD_ANIM_NONE;
    }
}

static lv_obj_t *ensure_screen(ui_route_id_t id) {
    if (id >= ROUTE_COUNT) return NULL;
    if (!s_registered[id]) {
        ESP_LOGW(TAG, "route %d no registrada", (int)id);
        return NULL;
    }
    if (!s_scr[id] && s_descs[id].build) {
        s_scr[id] = s_descs[id].build();
    }
    return s_scr[id];
}

static void call_leave(ui_route_id_t id) {
    if (id < ROUTE_COUNT && s_registered[id] && s_descs[id].on_leave) {
        s_descs[id].on_leave();
    }
}

static void call_enter(ui_route_id_t id, int32_t param) {
    if (id < ROUTE_COUNT && s_registered[id] && s_descs[id].on_enter) {
        s_descs[id].on_enter(param);
    }
}

static void load_route(ui_route_t r, ui_nav_anim_t anim) {
    lv_obj_t *scr = ensure_screen(r.id);
    if (!scr) {
        ESP_LOGE(TAG, "no scr para route %d", (int)r.id);
        return;
    }
    call_enter(r.id, r.param);
    lv_scr_load_anim(scr, map_anim(anim), 250, 0, false);
}

/* ────── API ────── */

void ui_router_init(void) {
    memset(s_descs, 0, sizeof(s_descs));
    memset(s_registered, 0, sizeof(s_registered));
    memset(s_scr, 0, sizeof(s_scr));
    memset(s_stack, 0, sizeof(s_stack));
    s_depth = 0;
    ESP_LOGI(TAG, "router init");
}

esp_err_t ui_router_register(const ui_route_desc_t *desc) {
    if (!desc || desc->id >= ROUTE_COUNT) return ESP_ERR_INVALID_ARG;
    s_descs[desc->id]     = *desc;
    s_registered[desc->id] = true;
    ESP_LOGI(TAG, "registered route %d (%s)", (int)desc->id,
             desc->name ? desc->name : "?");
    return ESP_OK;
}

void ui_router_push(ui_route_t r, ui_nav_anim_t anim) {
    if (s_depth >= ROUTER_STACK_MAX) {
        ESP_LOGW(TAG, "stack full, drop push %d", (int)r.id);
        return;
    }
    if (s_depth > 0) call_leave(s_stack[s_depth - 1].id);
    s_stack[s_depth++] = r;
    load_route(r, anim);
}

void ui_router_pop(ui_nav_anim_t anim) {
    if (s_depth <= 1) {
        /* Ya estamos en la raíz (watchface). Pop = no-op. */
        return;
    }
    call_leave(s_stack[s_depth - 1].id);
    s_depth--;
    load_route(s_stack[s_depth - 1], anim);
}

void ui_router_home(void) {
    if (s_depth == 0) return;
    if (s_depth == 1 && s_stack[0].id == ROUTE_WATCHFACE) return;

    while (s_depth > 0) {
        call_leave(s_stack[s_depth - 1].id);
        s_depth--;
    }
    ui_route_t wf = { .id = ROUTE_WATCHFACE, .param = 0 };
    s_stack[s_depth++] = wf;
    load_route(wf, NAV_ANIM_FADE);
}

ui_route_t ui_router_current(void) {
    if (s_depth == 0) {
        ui_route_t none = { .id = ROUTE_WATCHFACE, .param = 0 };
        return none;
    }
    return s_stack[s_depth - 1];
}

uint8_t ui_router_depth(void) { return s_depth; }

bool ui_router_at_root(void) {
    return s_depth <= 1 && (s_depth == 0 || s_stack[0].id == ROUTE_WATCHFACE);
}

bool ui_router_deliver_button(int btn_evt) {
    ui_route_t r = ui_router_current();
    if (s_registered[r.id] && s_descs[r.id].on_button) {
        return s_descs[r.id].on_button(btn_evt);
    }
    return false;
}

bool ui_router_deliver_gesture(uint8_t gesture) {
    ui_route_t r = ui_router_current();
    if (s_registered[r.id] && s_descs[r.id].on_gesture) {
        return s_descs[r.id].on_gesture(gesture);
    }
    return false;
}

void ui_router_tick(void) {
    ui_route_t r = ui_router_current();
    if (s_registered[r.id] && s_descs[r.id].tick) {
        s_descs[r.id].tick();
    }
}

void ui_router_reset_all(void) {
    s_depth = 0;
    ui_route_t wf = { .id = ROUTE_WATCHFACE, .param = 0 };
    s_stack[s_depth++] = wf;
    
    for (int i = 0; i < ROUTE_COUNT; i++) {
        if (s_scr[i]) {
            lv_obj_del(s_scr[i]);
            s_scr[i] = NULL;
        }
    }
    
    load_route(wf, NAV_ANIM_NONE);
}

/**
 * @file ui_router.h
 * @brief Router de pantallas de la UI touch (Galaxy-Watch-style).
 *
 * Modelo mental: un stack de rutas. La watchface es la raíz. Cada gesture o
 * botón que abre una nueva vista hace `push`. Botón BACK (BTN2 short) o
 * swipe-right hace `pop`. BTN1 short (HOME) hace `home()` que vacía el
 * stack y vuelve a la watchface.
 *
 * Las pantallas (`lv_obj_t *scr`) se construyen una vez y quedan residentes
 * en heap por toda la vida de la firmware — el router solo cambia cuál está
 * activa vía `lv_scr_load_anim`. Rebuild total solo en cambio de tema.
 */
#ifndef UI_ROUTER_H
#define UI_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

typedef enum {
    ROUTE_WATCHFACE = 0,
    ROUTE_TILES,           /* payload = tile_idx (0..N-1) */
    ROUTE_APP_DRAWER,
    ROUTE_QUICK_PANEL,
    ROUTE_APP_HR_SPOT,
    ROUTE_APP_ECG,
    ROUTE_APP_ACTIVITY,
    ROUTE_APP_TEMP,
    ROUTE_APP_BATTERY,
    ROUTE_APP_BLE,
    ROUTE_APP_SETTINGS,
    ROUTE_APP_POWER,
    ROUTE_APP_ABOUT,
    ROUTE_COUNT,
} ui_route_id_t;

typedef struct {
    ui_route_id_t id;
    int32_t param;
} ui_route_t;

typedef enum {
    NAV_ANIM_NONE = 0,
    NAV_ANIM_SLIDE_LEFT,
    NAV_ANIM_SLIDE_RIGHT,
    NAV_ANIM_SLIDE_UP,
    NAV_ANIM_SLIDE_DOWN,
    NAV_ANIM_FADE,
} ui_nav_anim_t;

/**
 * @brief Descriptor de una pantalla registrada. Cada módulo (watchface,
 * tiles, drawer, app_*) llena esto y llama `ui_router_register`.
 */
typedef struct {
    ui_route_id_t id;
    lv_obj_t   *(*build)(void);          /* construye scr, retorna handle */
    void        (*on_enter)(int32_t param);  /* nullable */
    void        (*on_leave)(void);            /* nullable */
    void        (*tick)(void);                /* llamado desde ui_tick si activa; nullable */
    bool        (*on_button)(int btn_evt);    /* nullable; return true = consumed */
    bool        (*on_gesture)(uint8_t g);     /* nullable; return true = consumed */
    const char *name;
} ui_route_desc_t;

void ui_router_init(void);
esp_err_t ui_router_register(const ui_route_desc_t *desc);

/** Push una ruta encima del stack. Anima la transición. */
void ui_router_push(ui_route_t r, ui_nav_anim_t anim);
/** Pop la ruta actual, vuelve a la de abajo. En watchface = no-op. */
void ui_router_pop(ui_nav_anim_t anim);
/** Vacía el stack y vuelve al watchface. */
void ui_router_home(void);
void ui_router_reset_all(void);

ui_route_t ui_router_current(void);
uint8_t    ui_router_depth(void);
bool       ui_router_at_root(void);

/** Delivery de eventos globales que el router redistribuye a la vista activa. */
bool ui_router_deliver_button(int btn_evt);
bool ui_router_deliver_gesture(uint8_t gesture);
void ui_router_tick(void);

#endif /* UI_ROUTER_H */

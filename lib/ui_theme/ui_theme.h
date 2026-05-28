/**
 * @file ui_theme.h
 * @brief Temas de color seleccionables para la UI (como "watch faces").
 *
 * El tema activo se persiste en NVS bajo la clave "ui_theme" (namespace
 * "supaclock", el mismo que usa power_modes). El default si no existe es
 * UI_THEME_AMOLED.
 *
 * Los colores se guardan como uint32_t (0xRRGGBB) para evitar problemas de
 * inicialización const con lv_color_t; el consumidor los convierte con
 * lv_color_hex(). Así este módulo NO depende de LVGL.
 *
 * Patrón espejo de power_modes: init() carga NVS, get() devuelve puntero
 * estable al perfil activo, set() persiste. Tras set(), la capa de UI debe
 * refrescar sus estilos (ui_styles_refresh + ui_restyle_metrics en
 * test_general.c).
 */
#ifndef UI_THEME_H
#define UI_THEME_H

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    UI_THEME_AMOLED = 0,  /**< Negro puro + acento teal, casi monocromo */
    UI_THEME_WARM,        /**< Carbón cálido + salvia, bajo deslumbre */
    UI_THEME_SLATE,       /**< Slate-navy frío + cian, look clínico */
    UI_THEME_VIVID,       /**< Esquema colorido original (clásico) */
    UI_THEME_COUNT,
} ui_theme_id_t;

/**
 * @brief Paleta de un tema. Todos los campos son 0xRRGGBB.
 */
typedef struct {
    const char *name;

    /* Chrome / estructura */
    uint32_t bg;        /**< Fondo de pantalla */
    uint32_t surface;   /**< Fondo de tarjetas / superficies elevadas */
    uint32_t text;      /**< Texto primario (números, valores) */
    uint32_t text_dim;  /**< Texto secundario (captions, edades, hints) */
    uint32_t accent;    /**< Títulos, selección, resaltes */
    uint32_t alert;     /**< Batería baja, errores */
    uint32_t ok;        /**< Estado bueno / normal */
    uint32_t warn;      /**< Estado elevado / advertencia */

    /* Colores semánticos por métrica */
    uint32_t c_hr;
    uint32_t c_spo2;
    uint32_t c_temp;
    uint32_t c_steps;
    uint32_t c_batt;
    uint32_t c_activity;
} ui_theme_t;

/**
 * @brief Inicializa el módulo y carga el tema guardado en NVS.
 * Debe llamarse después de nvs_flash_init().
 */
esp_err_t ui_theme_init(void);

/**
 * @brief Devuelve puntero al tema activo (estable durante toda la ejecución;
 * los datos cambian tras ui_theme_set pero la dirección no se invalida).
 */
const ui_theme_t *ui_theme_get(void);

/**
 * @brief Devuelve el id del tema activo.
 */
ui_theme_id_t ui_theme_get_id(void);

/**
 * @brief Cambia el tema activo y lo persiste en NVS.
 * @return ESP_OK si exitoso. La UI debe refrescar sus estilos tras llamar.
 */
esp_err_t ui_theme_set(ui_theme_id_t id);

/**
 * @brief Número de temas disponibles (== UI_THEME_COUNT).
 */
uint8_t ui_theme_count(void);

/**
 * @brief Nombre legible de un tema.
 */
const char *ui_theme_name(ui_theme_id_t id);

#endif /* UI_THEME_H */

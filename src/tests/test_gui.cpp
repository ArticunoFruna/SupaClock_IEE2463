#ifdef ENV_TEST_GUI

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "st7789.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h" // For monotonic time in ESP32

static const char *TAG = "GUI_TEST";

// Búfer Parcial para LVGL (30 filas enteras de alto) -- Pesa solo 14.4 KB
#define DISP_BUF_SIZE (240 * 30 * 1)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[DISP_BUF_SIZE];

/* Callback que alimenta la capa física de ST7789 desde el lienzo digital de LVGL */
void mi_pantalla_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint16_t x = area->x1;
    uint16_t y = area->y1;
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    
    // Nuestro driver st7789 recibe sin problemas este lienzo parcial y lo vuelca usando DMA / Polling
    st7789_draw_bitmap(x, y, w, h, (const uint16_t *)color_p);
    
    // Le indicamos a LVGL que el SPI ya terminó de enviar este lote
    lv_disp_flush_ready(disp_drv);
}

/* Hilo Permanente de LVGL: FreeRTOS ejecutando graficas */
void lv_tick_task(void *arg) {
    while (1) {
        lv_timer_handler(); // Computa posiciones, animaciones, y repinta zonas sucias
        vTaskDelay(pdMS_TO_TICKS(10)); // Alivia la CPU rindiendo el tiempo por 10ms
    }
}

// Callback de Animación para la Barra de UI
static void set_bar_value(void * bar, int32_t v) {
    lv_bar_set_value((lv_obj_t*)bar, v, LV_ANIM_OFF);
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Iniciando Test GUI...");

    // Inicializar Pantalla Hardware
    st7789_init();
    st7789_fill_screen(0x0000); // Negro de fondo

    // Inicializar LVGL Software
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, DISP_BUF_SIZE);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 280;
    disp_drv.flush_cb = mi_pantalla_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // ===================================
    // CONSTRUCCIÓN DEL TABLERO DEMOSTRATIVO
    // ===================================
    lv_obj_t * scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x191923), LV_PART_MAIN); // Fondo Oscuro Elegante

    // Letras! (Textos Profesionales con anti-aliasing)
    lv_obj_t * label_title = lv_label_create(scr);
    lv_label_set_text(label_title, "SupaClock OS");
    lv_obj_set_style_text_color(label_title, lv_color_hex(0xFFDD00), LV_PART_MAIN); // Amarillo electrico
    // En LVGL V8 usamos la fuente compilada por defecto
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t * label_bpm = lv_label_create(scr);
    lv_label_set_text(label_bpm, "98 BPM");
    lv_obj_set_style_text_color(label_bpm, lv_color_hex(0xFF5555), LV_PART_MAIN); // Rojo corazon
    lv_obj_align(label_bpm, LV_ALIGN_CENTER, 0, -20);

    // Barras! (Barra de progreso biológico)
    lv_obj_t * bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 200, 20);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 20);
    
    // Animacion para la barra para correr a 30-60 FPS
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_exec_cb(&a, set_bar_value);
    lv_anim_set_time(&a, 1500); // 1.5s subiendo
    lv_anim_set_playback_time(&a, 1500); // 1.5s bajando
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); // Bucle infinito
    lv_anim_start(&a);
    
    // Le cambiamos el color de "Progreso" a la barra a un Verde Neon
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x00E080), LV_PART_INDICATOR);

    // ===================================
    // INICIAR MOTOR GRAFICO
    // ===================================
    xTaskCreatePinnedToCore(lv_tick_task, "gui", 4096 * 2, NULL, 5, NULL, 0);
}

#endif

// Reemplazo completo de modulos para Single-Core y MAX30205
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "st7789.h"
#include "i2c_bus.h"
#include "max30205.h" // Usamos el correcto para temperatura clínica

#ifdef ENV_MAIN_APP
static const char *TAG = "SupaClock_Main";

SemaphoreHandle_t xGuiSemaphore;

#define DISP_BUF_SIZE (240 * 30 * 1)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[DISP_BUF_SIZE];

static lv_obj_t *label_temp;
static lv_obj_t *label_status;

// Declarar fuentes más grandes compiladas en LVGL
LV_FONT_DECLARE(lv_font_montserrat_40);
LV_FONT_DECLARE(lv_font_montserrat_20);

void main_pantalla_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint16_t x = area->x1;
    uint16_t y = area->y1;
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    
    st7789_draw_bitmap(x, y, w, h, (const uint16_t *)color_p);
    lv_disp_flush_ready(disp_drv);
}

void lv_tick_task(void *arg) {
    while (1) {
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_timer_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void sensor_task(void *arg) {
    float temperatura = 0.0f;
    esp_err_t err;

    while (1) {
        err = max30205_read_temperature(&temperatura); // Leemos el sensor real
        
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            if (err == ESP_OK) {
                int temp_int = (int)temperatura;
                int temp_frac = (int)((temperatura - temp_int) * 100);
                if (temp_frac < 0) temp_frac = -temp_frac;
                lv_label_set_text_fmt(label_temp, "%d.%02d °C", temp_int, temp_frac);
                lv_label_set_text(label_status, "I2C: OK \nMAX30205 Activo");
                lv_obj_set_style_text_color(label_status, lv_color_hex(0x00FF00), LV_PART_MAIN);
            } else {
                lv_label_set_text(label_temp, "--.- °C");
                lv_label_set_text(label_status, "Error I2C\n(MAX30205 Desconectado)");
                lv_obj_set_style_text_color(label_status, lv_color_hex(0xFF0000), LV_PART_MAIN);
                ESP_LOGE(TAG, "Fallo I2C Sensor Temperatura");
            }
            xSemaphoreGive(xGuiSemaphore);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void build_ui() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN); 

    lv_obj_t *label_title = lv_label_create(scr);
    lv_label_set_text(label_title, "SupaClock OS");
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x00D2FF), LV_PART_MAIN); 
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *label_metric = lv_label_create(scr);
    lv_label_set_text(label_metric, "Temperatura Corporal:");
    lv_obj_set_style_text_font(label_metric, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_metric, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(label_metric, LV_ALIGN_CENTER, 0, -40);

    label_temp = lv_label_create(scr);
    lv_label_set_text(label_temp, "--.- °C");
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_temp, lv_color_hex(0xFFDD00), LV_PART_MAIN); 
    lv_obj_align(label_temp, LV_ALIGN_CENTER, 0, 0);

    label_status = lv_label_create(scr);
    lv_label_set_text(label_status, "Buscando I2C...");
    lv_obj_set_style_text_color(label_status, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -30);
}

void app_main(void) {
    ESP_LOGI(TAG, "=== INICIANDO SUPACLOCK FIRMWARE ===");

    xGuiSemaphore = xSemaphoreCreateMutex();

    st7789_init();
    st7789_fill_screen(0x0000); 

    if (i2c_master_init() != ESP_OK) ESP_LOGE(TAG, "I2C Bus failed!");
    
    if (max30205_init() != ESP_OK) ESP_LOGW(TAG, "MAX30205 absent at bootup");

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, DISP_BUF_SIZE);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 280;
    disp_drv.flush_cb = main_pantalla_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    build_ui(); 

    // OJO: ESP32-C3 es de UN SOLO NUCLEO (Core 0). ¡No podemos fijar a Core 1! 
    xTaskCreate(lv_tick_task,   "gui_tick",   4096 * 2, NULL, 5, NULL);
    xTaskCreate(sensor_task,    "max30205",   4096,     NULL, 4, NULL);
}
#endif

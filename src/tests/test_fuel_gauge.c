#ifdef ENV_TEST_FUEL_GAUGE

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "st7789.h"
#include "i2c_bus.h"
#include "max17048.h"

static const char *TAG = "Test_FuelGauge_UI";

SemaphoreHandle_t xGuiSemaphore;

#define DISP_BUF_SIZE (240 * 30 * 1)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[DISP_BUF_SIZE];

static lv_obj_t *label_voltage;
static lv_obj_t *label_soc;
static lv_obj_t *label_status;

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

void fuelgauge_task(void *arg) {
    esp_err_t err;
    uint16_t volt_mv = 0;
    float soc_percent = 0.0f;

    while (1) {
        err = max17048_get_voltage(&volt_mv);
        esp_err_t err_soc = max17048_get_soc(&soc_percent);

        // Filtro EMA para SOC
        static float soc_filtered = -1.0f;
        if (err_soc == ESP_OK) {
            if (soc_filtered < 0.0f) {
                soc_filtered = soc_percent;
            } else {
                soc_filtered = 0.1f * soc_percent + 0.9f * soc_filtered;
            }
            soc_percent = soc_filtered;
        }
        
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            if (err == ESP_OK && err_soc == ESP_OK) {
                float volts = volt_mv / 1000.0f;
                int v_int = (int)volts;
                int v_frac = (int)((volts - v_int) * 100);
                
                int s_int = (int)soc_percent;
                int s_frac = (int)((soc_percent - s_int) * 10);
                if (s_frac < 0) s_frac = -s_frac;

                lv_label_set_text_fmt(label_voltage, "%d.%02d V", v_int, v_frac);
                lv_label_set_text_fmt(label_soc, "%d.%d %%", s_int, s_frac);
                
                lv_label_set_text(label_status, "I2C: OK \nBateria Conectada");
                lv_obj_set_style_text_color(label_status, lv_color_hex(0x00FF00), LV_PART_MAIN);
            } else {
                lv_label_set_text(label_voltage, "-- V");
                lv_label_set_text(label_soc, "-- %");
                lv_label_set_text(label_status, "Error I2C\n(MAX17048 Desconectado)");
                lv_obj_set_style_text_color(label_status, lv_color_hex(0xFF0000), LV_PART_MAIN);
                ESP_LOGE(TAG, "Sensor MAX17048 fallo lectura");
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
    lv_label_set_text(label_title, "Bateria LiPo");
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x00D2FF), LV_PART_MAIN); 
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 20);

    label_soc = lv_label_create(scr);
    lv_label_set_text(label_soc, "-- %");
    lv_obj_set_style_text_font(label_soc, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_soc, lv_color_hex(0x00FF00), LV_PART_MAIN); 
    lv_obj_align(label_soc, LV_ALIGN_CENTER, 0, -20);
    
    label_voltage = lv_label_create(scr);
    lv_label_set_text(label_voltage, "-- V");
    lv_obj_set_style_text_font(label_voltage, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_voltage, lv_color_hex(0xAAAAAA), LV_PART_MAIN); 
    lv_obj_align(label_voltage, LV_ALIGN_CENTER, 0, 30);

    label_status = lv_label_create(scr);
    lv_label_set_text(label_status, "Buscando I2C...");
    lv_obj_set_style_text_color(label_status, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -30);
}

void app_main(void) {
    ESP_LOGI(TAG, "=== INICIANDO PRUEBA MAX17048 ===");

    xGuiSemaphore = xSemaphoreCreateMutex();

    st7789_init();
    st7789_fill_screen(0x0000); 

    if (i2c_master_init() != ESP_OK) ESP_LOGE(TAG, "I2C Bus failed!");
    
    if (max17048_init() != ESP_OK) {
        ESP_LOGE(TAG, "MAX17048 no responde en I2C");
    } else {
        ESP_LOGI(TAG, "MAX17048 I2C Inicializado");
    }

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

    // ESP32-C3 es Single Core
    xTaskCreate(lv_tick_task,   "gui_tick",   4096 * 2, NULL, 5, NULL);
    xTaskCreate(fuelgauge_task, "max17048",   4096,     NULL, 4, NULL);
}
#endif

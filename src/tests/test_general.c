#ifdef ENV_TEST_GENERAL

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
#include "bmi160.h"
#include "max30205.h"
#include "ble_telemetry.h"
#include "step_algorithm.h"

static const char *TAG = "Test_General";

/* Mutex para LVGL (aunque solo hay una tarea que llama lv_timer_handler, es buena práctica) */
static SemaphoreHandle_t xGuiSemaphore;

/* Mutex para datos compartidos entre tareas (datos de sensores) */
static SemaphoreHandle_t xSensorDataMutex;

/* Datos compartidos */
typedef struct {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    uint32_t steps_sw;
    uint16_t steps_hw;
    float temperature_c;
    uint16_t battery_mv;
    float battery_soc;
} shared_sensor_data_t;

static shared_sensor_data_t sensor_data = {0};

/* Objetos LVGL de la GUI */
static lv_obj_t *label_temp;
static lv_obj_t *label_bat;
static lv_obj_t *label_steps;
static lv_obj_t *label_imu;
static lv_obj_t *label_ble;

/* Puntero al buffer de la pantalla */
#define DISP_BUF_SIZE (240 * 30 * 1)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[DISP_BUF_SIZE];

LV_FONT_DECLARE(lv_font_montserrat_20);

static void display_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint16_t x = area->x1;
    uint16_t y = area->y1;
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    
    st7789_draw_bitmap(x, y, w, h, (const uint16_t *)color_p);
    lv_disp_flush_ready(disp_drv);
}

static void build_test_gui(void) {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "SupaClock General Test");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00D2FF), LV_PART_MAIN);
    // Asumiendo que la fuente por defecto es suficiente si no se carga otra
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

    label_temp = lv_label_create(scr);
    lv_label_set_text(label_temp, "Temp: --.- °C");
    lv_obj_set_style_text_color(label_temp, lv_color_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_align(label_temp, LV_ALIGN_TOP_LEFT, 10, 40);

    label_bat = lv_label_create(scr);
    lv_label_set_text(label_bat, "Bat: --.-V / --%");
    lv_obj_set_style_text_color(label_bat, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_align(label_bat, LV_ALIGN_TOP_LEFT, 10, 70);

    label_steps = lv_label_create(scr);
    lv_label_set_text(label_steps, "Steps: SW 0 | HW 0");
    lv_obj_set_style_text_color(label_steps, lv_color_hex(0xFF00FF), LV_PART_MAIN);
    lv_obj_align(label_steps, LV_ALIGN_TOP_LEFT, 10, 100);

    label_imu = lv_label_create(scr);
    lv_label_set_text(label_imu, "IMU:\nAx: 0 Ay: 0 Az: 0");
    lv_obj_set_style_text_color(label_imu, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(label_imu, LV_ALIGN_TOP_LEFT, 10, 130);

    label_ble = lv_label_create(scr);
    lv_label_set_text(label_ble, "BLE: Activo");
    lv_obj_set_style_text_color(label_ble, lv_color_hex(0x0000FF), LV_PART_MAIN);
    lv_obj_align(label_ble, LV_ALIGN_BOTTOM_LEFT, 10, -5);
}

void gui_task(void *pvParameter) {
    while (1) {
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_timer_handler();

            // LVGL printf no soporta floats por defecto, lo separamos en int y frac
            if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY) == pdTRUE) {
                int temp_int = (int)sensor_data.temperature_c;
                int temp_frac = (int)((sensor_data.temperature_c - temp_int) * 100);
                if (temp_frac < 0) temp_frac = -temp_frac;

                int bat_v_int = sensor_data.battery_mv / 1000;
                int bat_v_frac = (sensor_data.battery_mv % 1000) / 10;
                
                int bat_soc_int = (int)sensor_data.battery_soc;
                int bat_soc_frac = (int)((sensor_data.battery_soc - bat_soc_int) * 10);
                if (bat_soc_frac < 0) bat_soc_frac = -bat_soc_frac;

                lv_label_set_text_fmt(label_temp, "Temp: %d.%02d °C", temp_int, temp_frac);
                lv_label_set_text_fmt(label_bat, "Bat: %d.%02dV / %d.%d%%", bat_v_int, bat_v_frac, bat_soc_int, bat_soc_frac);
                lv_label_set_text_fmt(label_steps, "Steps: SW %lu | HW %u", (unsigned long)sensor_data.steps_sw, sensor_data.steps_hw);
                lv_label_set_text_fmt(label_imu, "IMU:\nAx:%d Ay:%d Az:%d", sensor_data.ax, sensor_data.ay, sensor_data.az);
                xSemaphoreGive(xSensorDataMutex);
            }
            
            xSemaphoreGive(xGuiSemaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(33)); // ~30 fps
    }
}

void imu_task(void *pvParameter) {
    int16_t imu_raw[6] = {0}; // ax, ay, az, gx, gy, gz
    
    step_algo_state_t sw_pedometer;
    step_algo_init(&sw_pedometer);

    // 50 Hz (20ms)
    const TickType_t xFrequency = pdMS_TO_TICKS(20);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        esp_err_t err = bmi160_read_accel_gyro(&imu_raw[0], &imu_raw[1], &imu_raw[2], &imu_raw[3], &imu_raw[4], &imu_raw[5]);
        if (err == ESP_OK) {
            uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
            
            uint32_t new_steps = step_algo_update(&sw_pedometer, imu_raw[0], imu_raw[1], imu_raw[2], imu_raw[3], imu_raw[4], imu_raw[5], now_ms);

            // Enviar datos crudos del IMU por BLE (alta frecuencia, 50Hz)
            ble_telemetry_send(imu_raw, sizeof(imu_raw));

            // Actualizar datos compartidos
            if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                sensor_data.ax = imu_raw[0];
                sensor_data.ay = imu_raw[1];
                sensor_data.az = imu_raw[2];
                sensor_data.gx = imu_raw[3];
                sensor_data.gy = imu_raw[4];
                sensor_data.gz = imu_raw[5];
                sensor_data.steps_sw += new_steps;
                xSemaphoreGive(xSensorDataMutex);
            }
        }
    }
}

void sensor_task(void *pvParameter) {
    while (1) {
        float temp_c = 0.0f;
        uint16_t bat_mv = 0;
        float bat_soc = 0.0f;
        uint16_t hw_steps = 0;

        max30205_read_temperature(&temp_c);
        max17048_get_voltage(&bat_mv);
        max17048_get_soc(&bat_soc);
        bmi160_read_step_counter(&hw_steps);

        // Actualizar datos compartidos
        if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY) == pdTRUE) {
            sensor_data.temperature_c = temp_c;
            sensor_data.battery_mv = bat_mv;
            sensor_data.battery_soc = bat_soc;
            sensor_data.steps_hw = hw_steps;
            
            // Reempaquetar para enviar por BLE baja frecuencia
            ble_sensor_packet_t pkt;
            pkt.temperature_x100 = (int16_t)(sensor_data.temperature_c * 100);
            pkt.steps_hw = sensor_data.steps_hw;
            pkt.steps_sw = sensor_data.steps_sw;
            pkt.battery_mv = sensor_data.battery_mv;
            pkt.battery_soc = (uint8_t)sensor_data.battery_soc;
            
            xSemaphoreGive(xSensorDataMutex);

            // Enviar paquete lento (1Hz)
            ble_telemetry_send_sensors(&pkt);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== INICIANDO ENTORNO TEST GENERAL ===");

    xGuiSemaphore = xSemaphoreCreateMutex();
    xSensorDataMutex = xSemaphoreCreateMutex();

    ESP_LOGI(TAG, "Inicializando display...");
    st7789_init();
    st7789_fill_screen(0x0000); 

    ESP_LOGI(TAG, "Inicializando I2C Bus...");
    if (i2c_master_init() != ESP_OK) ESP_LOGE(TAG, "I2C Bus failed!");
    
    ESP_LOGI(TAG, "Inicializando MAC17048...");
    if (max17048_init() != ESP_OK) ESP_LOGW(TAG, "MAX17048 fallo / ausente");
    
    ESP_LOGI(TAG, "Inicializando BMI160...");
    if (bmi160_init() == ESP_OK) {
        bmi160_enable_step_counter();
    } else {
        ESP_LOGE(TAG, "BMI160 falló al inicializar");
    }

    ESP_LOGI(TAG, "Inicializando MAX30205...");
    if (max30205_init() != ESP_OK) ESP_LOGW(TAG, "MAX30205 fallo / ausente");
    
    ESP_LOGI(TAG, "Inicializando BLE...");
    if (ble_telemetry_init() != ESP_OK) ESP_LOGE(TAG, "BLE Stack falló al inicializar");

    ESP_LOGI(TAG, "Inicializando LVGL...");
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, DISP_BUF_SIZE);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 280; // o el q estes usando
    disp_drv.flush_cb = display_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    build_test_gui(); 

    // Tareas
    xTaskCreate(gui_task, "gui_task", 4096, NULL, 5, NULL);
    xTaskCreate(imu_task, "imu_task", 4096, NULL, 6, NULL);
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 4, NULL);
}

#endif

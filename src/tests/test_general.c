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
#include "max30102.h"
#include "ble_telemetry.h"
#include "step_algorithm.h"
#include "gpio_buttons.h"
#include "esp_sleep.h"

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
    uint8_t hr_bpm;
    uint8_t spo2_pct;
    bool finger_present;
} shared_sensor_data_t;

static shared_sensor_data_t sensor_data = {0};

/* ═══════════════════════════════════════════════════════════════════
 *  GUI multi-pantalla con navegación por 2 botones
 *  ─────────────────────────────────────────────────
 *  NEXT (GPIO 10) → cicla pantallas (Home→Bio→ECG→Menú→Home) y dentro
 *                   del Menú scrollea ítems.
 *  SELECT (GPIO 1) → acción contextual (toggle ECG, activar ítem).
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    SCREEN_HOME = 0,
    SCREEN_BIO,
    SCREEN_ECG,
    SCREEN_MENU,
    SCREEN_COUNT,
} ui_screen_t;

#define MENU_ITEM_COUNT 3
static const char *MENU_LABELS[MENU_ITEM_COUNT] = {
    "Reiniciar Pasos",
    "Vincular BLE",
    "Apagar",
};

/* Estado de navegación */
static ui_screen_t current_screen = SCREEN_HOME;
static uint8_t menu_selection = 0;
static int64_t ecg_start_us = 0;   /* timestamp de inicio de grabación ECG */

/* Screens (objetos raíz LVGL, uno por pantalla) */
static lv_obj_t *scr_obj[SCREEN_COUNT];

/* Labels dinámicos por pantalla */
/*  Home */
static lv_obj_t *home_clock, *home_steps, *home_bat, *home_hr, *home_act;
/*  Bio  */
static lv_obj_t *bio_hr, *bio_spo2, *bio_temp, *bio_status;
/*  ECG  */
static lv_obj_t *ecg_instr, *ecg_timer, *ecg_rec;
/*  Menú */
static lv_obj_t *menu_rows[MENU_ITEM_COUNT];

/* Puntero al buffer de la pantalla */
#define DISP_BUF_SIZE (240 * 30 * 1)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_1[DISP_BUF_SIZE];

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_40);

static void display_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint16_t x = area->x1;
    uint16_t y = area->y1;
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;

    st7789_draw_bitmap(x, y, w, h, (const uint16_t *)color_p);
    lv_disp_flush_ready(disp_drv);
}

/* ─────────────────── Helpers de construcción ─────────────────── */

static lv_obj_t *make_screen(const char *title, uint32_t title_color) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(title_color), LV_PART_MAIN);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 6);
    return scr;
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                            uint32_t color_hex, lv_align_t align,
                            int x_ofs, int y_ofs, const char *txt) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(color_hex), LV_PART_MAIN);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_align(l, align, x_ofs, y_ofs);
    return l;
}

/* ─────────────────── Construcción de pantallas ─────────────────── */

static void build_home(void) {
    scr_obj[SCREEN_HOME] = make_screen("SUPACLOCK", 0x00D2FF);
    lv_obj_t *s = scr_obj[SCREEN_HOME];

    /* Reloj grande centrado */
    home_clock = make_label(s, &lv_font_montserrat_40, 0xFFFFFF,
                            LV_ALIGN_TOP_MID, 0, 35, "--:--");

    /* Grid 2×2 */
    make_label(s, &lv_font_montserrat_14, 0x8B949E, LV_ALIGN_TOP_LEFT,  15, 120, "STEPS");
    make_label(s, &lv_font_montserrat_14, 0x8B949E, LV_ALIGN_TOP_RIGHT,-15, 120, "BATTERY");
    make_label(s, &lv_font_montserrat_14, 0x8B949E, LV_ALIGN_TOP_LEFT,  15, 190, "HR");
    make_label(s, &lv_font_montserrat_14, 0x8B949E, LV_ALIGN_TOP_RIGHT,-15, 190, "ACTIVITY");

    home_steps = make_label(s, &lv_font_montserrat_20, 0x3FB950,
                            LV_ALIGN_TOP_LEFT, 15, 140, "0");
    home_bat   = make_label(s, &lv_font_montserrat_20, 0xF0C34E,
                            LV_ALIGN_TOP_RIGHT,-15, 140, "--%");
    home_hr    = make_label(s, &lv_font_montserrat_20, 0xFF3B6E,
                            LV_ALIGN_TOP_LEFT, 15, 210, "-- bpm");
    home_act   = make_label(s, &lv_font_montserrat_20, 0x3F9BFF,
                            LV_ALIGN_TOP_RIGHT,-15, 210, "Reposo");
}

static void build_bio(void) {
    scr_obj[SCREEN_BIO] = make_screen("BIOMETRIA", 0x00D2FF);
    lv_obj_t *s = scr_obj[SCREEN_BIO];

    bio_hr     = make_label(s, &lv_font_montserrat_20, 0xFF3B6E,
                            LV_ALIGN_TOP_LEFT, 20, 60,  "HR:   -- bpm");
    bio_spo2   = make_label(s, &lv_font_montserrat_20, 0x3F9BFF,
                            LV_ALIGN_TOP_LEFT, 20, 110, "SpO2: --%");
    bio_temp   = make_label(s, &lv_font_montserrat_20, 0xF0883E,
                            LV_ALIGN_TOP_LEFT, 20, 160, "Temp: --.- C");
    bio_status = make_label(s, &lv_font_montserrat_20, 0x3FB950,
                            LV_ALIGN_TOP_LEFT, 20, 210, "Estado: --");
}

static void build_ecg(void) {
    scr_obj[SCREEN_ECG] = make_screen("MODO ECG", 0x3FB950);
    lv_obj_t *s = scr_obj[SCREEN_ECG];

    ecg_instr = make_label(s, &lv_font_montserrat_14, 0xE6EDF3,
                           LV_ALIGN_TOP_MID, 0, 50,
                           "Presione los electrodos\n"
                           "laterales con la mano\n"
                           "opuesta.\n\n"
                           "Pulsa SELECT para iniciar");
    lv_obj_set_style_text_align(ecg_instr, LV_TEXT_ALIGN_CENTER, 0);

    ecg_timer = make_label(s, &lv_font_montserrat_40, 0xFFFFFF,
                           LV_ALIGN_CENTER, 0, 0, "0:00");
    lv_obj_add_flag(ecg_timer, LV_OBJ_FLAG_HIDDEN);

    ecg_rec = make_label(s, &lv_font_montserrat_20, 0xDA3633,
                         LV_ALIGN_BOTTOM_MID, 0, -20, "● REC");
    lv_obj_add_flag(ecg_rec, LV_OBJ_FLAG_HIDDEN);
}

static void build_menu(void) {
    scr_obj[SCREEN_MENU] = make_screen("MENU", 0x00D2FF);
    lv_obj_t *s = scr_obj[SCREEN_MENU];

    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        menu_rows[i] = lv_label_create(s);
        lv_obj_set_style_text_font(menu_rows[i], &lv_font_montserrat_20, LV_PART_MAIN);
        lv_label_set_text(menu_rows[i], MENU_LABELS[i]);
        lv_obj_set_width(menu_rows[i], 220);
        lv_obj_set_style_pad_all(menu_rows[i], 8, LV_PART_MAIN);
        lv_obj_set_style_radius(menu_rows[i], 6, LV_PART_MAIN);
        lv_obj_align(menu_rows[i], LV_ALIGN_TOP_MID, 0, 50 + i * 60);
    }
}

static void render_menu_selection(void) {
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        if (i == menu_selection) {
            lv_obj_set_style_bg_color(menu_rows[i], lv_color_hex(0x1F6FEB), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(menu_rows[i], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_text_color(menu_rows[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_opa(menu_rows[i], LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_text_color(menu_rows[i], lv_color_hex(0xAAAAAA), LV_PART_MAIN);
        }
    }
}

/* ─────────────────── Actualizadores por pantalla ─────────────────── */

static void update_home_screen(const shared_sensor_data_t *d) {
    /* Reloj desde uptime (no hay RTC externo) */
    uint32_t s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    lv_label_set_text_fmt(home_clock, "%lu:%02lu",
                          (unsigned long)((s / 60) % 100),
                          (unsigned long)(s % 60));

    lv_label_set_text_fmt(home_steps, "%lu", (unsigned long)d->steps_sw);

    int soc = (int)d->battery_soc;
    lv_label_set_text_fmt(home_bat, "%d%%", soc);

    if (d->finger_present && d->hr_bpm > 0) {
        lv_label_set_text_fmt(home_hr, "%u bpm", d->hr_bpm);
    } else {
        lv_label_set_text(home_hr, "-- bpm");
    }

    /* Heurística simple de actividad: magnitud de accel
     * (placeholder hasta integrar el clasificador ML) */
    int32_t amag = (int32_t)d->ax * d->ax + (int32_t)d->ay * d->ay + (int32_t)d->az * d->az;
    const char *act = (amag > 300000000L) ? "Activo" : "Reposo";
    lv_label_set_text(home_act, act);
}

static void update_bio_screen(const shared_sensor_data_t *d) {
    if (d->finger_present && d->hr_bpm > 0)
        lv_label_set_text_fmt(bio_hr, "HR:   %u bpm", d->hr_bpm);
    else
        lv_label_set_text(bio_hr, "HR:   -- bpm");

    if (d->finger_present && d->spo2_pct > 0)
        lv_label_set_text_fmt(bio_spo2, "SpO2: %u%%", d->spo2_pct);
    else
        lv_label_set_text(bio_spo2, "SpO2: --%");

    int temp_int  = (int)d->temperature_c;
    int temp_frac = (int)((d->temperature_c - temp_int) * 10);
    if (temp_frac < 0) temp_frac = -temp_frac;
    lv_label_set_text_fmt(bio_temp, "Temp: %d.%d C", temp_int, temp_frac);

    const char *st;
    uint32_t   col;
    if (!d->finger_present) { st = "Estado: Sin dedo"; col = 0x8B949E; }
    else if (d->hr_bpm > 100) { st = "Estado: Alto";    col = 0xF0883E; }
    else if (d->hr_bpm > 0)   { st = "Estado: Normal";  col = 0x3FB950; }
    else                      { st = "Estado: Midiendo";col = 0xF0C34E; }
    lv_label_set_text(bio_status, st);
    lv_obj_set_style_text_color(bio_status, lv_color_hex(col), LV_PART_MAIN);
}

static void update_ecg_screen(void) {
    bool rec = ble_telemetry_is_ecg_mode_active();
    if (rec) {
        if (ecg_start_us == 0) ecg_start_us = esp_timer_get_time();
        uint32_t secs = (uint32_t)((esp_timer_get_time() - ecg_start_us) / 1000000ULL);
        lv_label_set_text_fmt(ecg_timer, "%lu:%02lu",
                              (unsigned long)(secs / 60),
                              (unsigned long)(secs % 60));

        lv_obj_add_flag(ecg_instr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ecg_timer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ecg_rec, LV_OBJ_FLAG_HIDDEN);
    } else {
        ecg_start_us = 0;
        lv_obj_clear_flag(ecg_instr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ecg_timer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ecg_rec, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ─────────────────── Navegación & acciones ─────────────────── */

static void switch_to(ui_screen_t s) {
    current_screen = s;
    lv_scr_load(scr_obj[s]);
    if (s == SCREEN_MENU) render_menu_selection();
}

static void menu_execute_selected(void) {
    switch (menu_selection) {
        case 0: /* Reiniciar Pasos */
            if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                sensor_data.steps_sw = 0;
                xSemaphoreGive(xSensorDataMutex);
            }
            ESP_LOGI(TAG, "Menú: pasos reiniciados");
            break;
        case 1: /* Vincular BLE — placeholder informativo */
            ESP_LOGI(TAG, "Menú: BLE advertising activo (vinculación automática al conectar)");
            break;
        case 2: /* Apagar → deep sleep, despierta con BTN_SELECT */
            ESP_LOGI(TAG, "Menú: entrando en deep sleep. Pulsa SELECT para despertar.");
            vTaskDelay(pdMS_TO_TICKS(200));
            esp_deep_sleep_enable_gpio_wakeup((1ULL << BTN_SELECT_PIN),
                                              ESP_GPIO_WAKEUP_GPIO_LOW);
            esp_deep_sleep_start();
            break;
    }
}

static void handle_button(btn_event_t ev) {
    if (ev == BTN_EVENT_NONE) return;

    switch (ev) {
        case BTN_EVENT_NEXT_SHORT:
            if (current_screen == SCREEN_MENU) {
                /* En menú NEXT scrollea ítems; al pasar el último, salir a Home */
                if (menu_selection + 1 >= MENU_ITEM_COUNT) {
                    menu_selection = 0;
                    render_menu_selection();
                    switch_to(SCREEN_HOME);
                } else {
                    menu_selection++;
                    render_menu_selection();
                }
            } else {
                switch_to((current_screen + 1) % SCREEN_COUNT);
            }
            break;
        case BTN_EVENT_NEXT_LONG:
            /* Long NEXT: salir del menú hacia Home, o ir atrás entre screens */
            switch_to((current_screen + SCREEN_COUNT - 1) % SCREEN_COUNT);
            break;
        case BTN_EVENT_SELECT_SHORT:
            if (current_screen == SCREEN_ECG) {
                ble_telemetry_set_ecg_mode(!ble_telemetry_is_ecg_mode_active());
            } else if (current_screen == SCREEN_MENU) {
                menu_execute_selected();
            }
            break;
        case BTN_EVENT_SELECT_LONG:
            /* Reservado: por ahora vuelve a Home como atajo */
            switch_to(SCREEN_HOME);
            break;
        default: break;
    }
}

static void build_ui(void) {
    build_home();
    build_bio();
    build_ecg();
    build_menu();
    render_menu_selection();
    lv_scr_load(scr_obj[SCREEN_HOME]);
}

void gui_task(void *pvParameter) {
    while (1) {
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {

            /* 1) Procesar eventos de botones */
            btn_event_t ev;
            while ((ev = gpio_buttons_poll()) != BTN_EVENT_NONE) {
                handle_button(ev);
            }

            /* 2) Auto-switch a pantalla ECG cuando el PC inicia modo ECG
             *    (para que el usuario vea el timer de grabación). */
            if (ble_telemetry_is_ecg_mode_active() && current_screen != SCREEN_ECG) {
                switch_to(SCREEN_ECG);
            }

            /* 3) Actualizar contenido de la pantalla visible */
            if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                shared_sensor_data_t snap = sensor_data;
                xSemaphoreGive(xSensorDataMutex);

                switch (current_screen) {
                    case SCREEN_HOME: update_home_screen(&snap); break;
                    case SCREEN_BIO:  update_bio_screen(&snap);  break;
                    case SCREEN_ECG:  update_ecg_screen();       break;
                    case SCREEN_MENU: /* estático, nada que refrescar */ break;
                    default: break;
                }
            }

            /* 4) Render LVGL */
            lv_timer_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS + poll de botones
    }
}

#include "ad8232.h"
#include "esp_adc/adc_continuous.h"

void ecg_task(void *pvParameter) {
    uint8_t dma_buf[AD8232_READ_LEN];
    uint32_t ret_num = 0;
    
    #define ECG_DOWNSAMPLE_RATIO 40
    #define ECG_BLE_CHUNK_SIZE 10
    
    int16_t ble_chunk[ECG_BLE_CHUNK_SIZE];
    int chunk_idx = 0;
    uint32_t sum = 0;
    int count = 0;

    while (1) {
        if (!ble_telemetry_is_ecg_mode_active()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            // Resetear contadores si se apaga el modo
            chunk_idx = 0;
            sum = 0;
            count = 0;
            continue;
        }

        // Leer DMA de forma continua (bloqueante hasta que haya datos o timeout 10ms)
        esp_err_t ret = adc_continuous_read(ad8232_get_adc_handle(), dma_buf, AD8232_READ_LEN, &ret_num, 10);
        if (ret == ESP_OK) {
            for (int i = 0; i < ret_num; i += sizeof(adc_digi_output_data_t)) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t*)&dma_buf[i];
                uint16_t raw_val = p->type2.data;
                
                sum += raw_val;
                count++;
                
                if (count >= ECG_DOWNSAMPLE_RATIO) {
                    ble_chunk[chunk_idx++] = (int16_t)(sum / ECG_DOWNSAMPLE_RATIO);
                    sum = 0;
                    count = 0;
                    
                    if (chunk_idx >= ECG_BLE_CHUNK_SIZE) {
                        ble_telemetry_send_ecg(ble_chunk, sizeof(ble_chunk));
                        chunk_idx = 0;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Ceder CPU
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

void hrm_task(void *pvParameter) {
    /* MAX30102 a 25 Hz efectivos (100 sps / sample_avg = 4).
     * FIFO físico = 32 muestras = 1.28 s. Polling a 200 ms → típicamente
     * 5 muestras/burst con 6× de margen antes del overflow, robusto frente
     * a jitter del scheduler (GUI/BLE/mutex I2C). */
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // Polling más rápido (100 ms) para reducir riesgo de overflow
    TickType_t xLastWakeTime = xTaskGetTickCount();

    max30102_sample_t samples[32]; // Capacidad máxima del FIFO

    /* Flushear muestras acumuladas durante el arranque (Fases 2+3 toman
     * ~1.5 s entre init del sensor y arranque de esta task → el FIFO
     * de 1.28 s ya estaba saturado). */
    max30102_flush_fifo();

    uint32_t last_ovf_logged = 0;

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        uint8_t n = 0;
        if (max30102_read_samples(samples, 32, &n) == ESP_OK && n > 0) {
            for (uint8_t i = 0; i < n; i++) {
                max30102_process_sample(samples[i].red, samples[i].ir);
            }

            uint8_t bpm = 0, spo2 = 0;
            max30102_get_hr(&bpm);
            max30102_get_spo2(&spo2);
            bool finger = max30102_finger_present();

            if (xSemaphoreTake(xSensorDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                sensor_data.hr_bpm = bpm;
                sensor_data.spo2_pct = spo2;
                sensor_data.finger_present = finger;
                xSemaphoreGive(xSensorDataMutex);
            }
        }

        /* Sanity-check: si se empiezan a acumular overflows, la task está
         * siendo preempted demasiado — subir prioridad o bajar xFrequency. */
        uint32_t ovf = max30102_get_overflow_count();
        if (ovf - last_ovf_logged >= 10) {
            ESP_LOGW(TAG, "MAX30102 FIFO overflows acumulados: %lu", (unsigned long)ovf);
            last_ovf_logged = ovf;
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
            pkt.hr_bpm = sensor_data.hr_bpm;
            pkt.spo2_pct = sensor_data.spo2_pct;

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

    /*
     * ═══ FASE 1: I2C y Sensores (bajo consumo, ~5mA) ═══
     * Inicializar primero los periféricos de bajo consumo para que el
     * fuel gauge y el IMU estén listos antes de encender la pantalla.
     */
    ESP_LOGI(TAG, "[Fase 1] Inicializando I2C Bus y sensores...");
    if (i2c_master_init() != ESP_OK) ESP_LOGE(TAG, "I2C Bus failed!");

    if (gpio_buttons_init() != ESP_OK) ESP_LOGW(TAG, "gpio_buttons_init fallo");
    
    if (max17048_init() != ESP_OK) ESP_LOGW(TAG, "MAX17048 fallo / ausente");
    
    if (bmi160_init() == ESP_OK) {
        bmi160_enable_step_counter();
    } else {
        ESP_LOGE(TAG, "BMI160 falló al inicializar");
    }

    if (max30205_init() != ESP_OK) ESP_LOGW(TAG, "MAX30205 fallo / ausente");

    if (max30102_init_hrm() != ESP_OK) ESP_LOGW(TAG, "MAX30102 fallo / ausente");

    if (ad8232_init_dma() == ESP_OK) {
        ad8232_start_dma();
        ESP_LOGI(TAG, "AD8232 inicializado y DMA iniciado");
    } else {
        ESP_LOGW(TAG, "AD8232 fallo / ausente");
    }

    vTaskDelay(pdMS_TO_TICKS(500)); // Delay más largo (0.5s) para estabilizar LDO

    /*
     * ═══ FASE 2: Display (pico de corriente SPI + DMA, ~40mA) ═══
     */
    ESP_LOGI(TAG, "[Fase 2] Inicializando display...");
    st7789_init();
    vTaskDelay(pdMS_TO_TICKS(100)); // Delay post-init display
    st7789_fill_screen(0x0000); 

    ESP_LOGI(TAG, "Inicializando LVGL...");
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, DISP_BUF_SIZE);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 280;
    disp_drv.flush_cb = display_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    build_ui();

    vTaskDelay(pdMS_TO_TICKS(1000)); // Esperar 1 segundo completo antes del BLE

    /*
     * ═══ FASE 3: BLE (pico de corriente radio, ~130mA) ═══
     * El radio BLE es el que más corriente consume al arrancar.
     * Encenderlo último y con la pantalla ya estable evita que ambos
     * picos se sumen y provoquen brownout.
     */
    ESP_LOGI(TAG, "[Fase 3] Inicializando BLE...");
    if (ble_telemetry_init() != ESP_OK) ESP_LOGE(TAG, "BLE Stack falló al inicializar");

    // Tareas
    xTaskCreate(gui_task, "gui_task", 4096, NULL, 5, NULL);
    xTaskCreate(imu_task, "imu_task", 4096, NULL, 6, NULL);
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 4, NULL);
    xTaskCreate(hrm_task, "hrm_task", 4096, NULL, 5, NULL);
    xTaskCreate(ecg_task, "ecg_task", 4096, NULL, 7, NULL); // Mayor prioridad para no perder DMA

    ESP_LOGI(TAG, "=== SISTEMA INICIADO CORRECTAMENTE ===");
}


#endif

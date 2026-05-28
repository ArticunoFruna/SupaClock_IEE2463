#include "supaclock_app.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "nvs_flash.h"
#include "st7789.h"
#include "i2c_bus.h"
#include "max17048.h"
#include "bmi160.h"
#include "max30205.h"
#include "max30102.h"
#include "ble_telemetry.h"
#include "step_algorithm.h"
#include "gpio_buttons.h"
#include "ad8232.h"
#include "esp_adc/adc_continuous.h"
#include "power_modes.h"
#include "ui_theme.h"
#include "app_state.h"
#include "supaclock_ui.h"

static const char *TAG = "SUPA_APP";

static inline uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* ───────────────────────── Mutexes ───────────────────────── */
static SemaphoreHandle_t xGuiSemaphore;
/* El estado de sensores vive en lib/app_state (struct + mutex + now_ms). */

/* ───────────────────────── Backlight / inactividad (lo maneja gui_task) ── */
static bool backlight_on = false;
static uint32_t last_activity_ms = 0;

/* ───────────────────────── PM lock del ECG ─────────────────────────
 * Bloquea light sleep mientras el ADC continuo está activo (evita que el
 * APB se reconfigure entre frames del DMA y produzca escalones sobre la
 * traza). El backlight tiene su propio lock dentro de st7789. */
#if CONFIG_PM_ENABLE
static esp_pm_lock_handle_t s_ecg_pm_lock = NULL;
#endif

/* ───────────────────────── Acciones de sistema para la UI ─────────────────
 * La UI llama estos callbacks (registrados con ui_set_actions) para acciones
 * que tocan hardware/IDF que la lib UI no debe conocer (deep sleep, MAX17048).
 */
static void on_power_off(void) {
    vTaskDelay(pdMS_TO_TICKS(200));
    /* Wake-up por BTN_SELECT (activo en bajo).
     * S3 (Xtensa): solo los RTC-GPIOs (0..21) despiertan de deep sleep
     *              → ext1 con TRIGGER_LOW. BTN_SELECT = GPIO8 = RTC. */
#if CONFIG_IDF_TARGET_ESP32S3
    esp_sleep_enable_ext1_wakeup_io((1ULL << BTN_SELECT_PIN), ESP_EXT1_WAKEUP_ANY_LOW);
#else
    esp_deep_sleep_enable_gpio_wakeup((1ULL << BTN_SELECT_PIN), ESP_GPIO_WAKEUP_GPIO_LOW);
#endif
    esp_deep_sleep_start();
}

static void on_battery_reset(void) {
    max17048_reset();
    ESP_LOGI(TAG, "Bateria reseteada (POR + Quick Start)");
}

static const ui_actions_t s_ui_actions = {
    .on_power_off     = on_power_off,
    .on_battery_reset = on_battery_reset,
};

/* ═══════════════════════════════════════════════════════════════════
 *  TASKS
 * ═══════════════════════════════════════════════════════════════════ */

void gui_task(void *pvParameter) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    st7789_set_brightness(power_get_display_brightness(power_get_mode()));
    backlight_on = true;
    last_activity_ms = now_ms();

    /* Frames de inactividad acumulados (frame ≈ 33 ms) */
    while (1) {
        uint32_t time_till_next = 100;
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {

            btn_event_t ev;
            bool action_taken = false;
            while ((ev = gpio_buttons_poll()) != BTN_EVENT_NONE) {
                action_taken = true;
                last_activity_ms = now_ms();
                if (!backlight_on) {
                    st7789_set_brightness(power_get_display_brightness(power_get_mode()));
                    backlight_on = true;
                } else {
                    ui_handle_button(ev);
                }
            }

            if (!action_taken) {
                /* auto-off según modo activo */
                uint16_t off_s = power_get_display_off_s(power_get_mode());
                if (now_ms() - last_activity_ms >= off_s * 1000 && backlight_on) {
                    st7789_set_brightness(0);
                    backlight_on = false;
                }
            }

            /* Si la pantalla está encendida, refrescar UI y animaciones. */
            if (backlight_on) {
                time_till_next = ui_tick();
            }
            xSemaphoreGive(xGuiSemaphore);
        }
        
        uint32_t delay_ms;
        if (backlight_on) {
            delay_ms = time_till_next;
            if (delay_ms < 5) delay_ms = 5;
            /* Incrementar el techo de 30ms a 200ms permite que el CPU duerma
               mucho más si no hay animaciones pendientes. */
            if (delay_ms > 200) delay_ms = 200;
        } else {
            delay_ms = 100;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

/* ─────────────── ECG task ─────────────── */
void ecg_task(void *pvParameter) {
    uint8_t dma_buf[AD8232_READ_LEN];
    uint32_t ret_num = 0;

    #define ECG_DOWNSAMPLE_RATIO 40
    #define ECG_BLE_CHUNK_SIZE 10

    int16_t ble_chunk[ECG_BLE_CHUNK_SIZE];
    int chunk_idx = 0;
    uint32_t sum = 0;
    int count = 0;
    bool is_dma_running = false;

    while (1) {
        if (!ble_telemetry_is_ecg_mode_active()) {
            if (is_dma_running) {
                ad8232_stop_dma();
                is_dma_running = false;
#if CONFIG_PM_ENABLE
                if (s_ecg_pm_lock) esp_pm_lock_release(s_ecg_pm_lock);
#endif
            }
            vTaskDelay(pdMS_TO_TICKS(500)); /* Si no hay ECG, dormir profundamente este hilo */
            chunk_idx = 0; sum = 0; count = 0;
            continue;
        } else if (!is_dma_running) {
#if CONFIG_PM_ENABLE
            if (s_ecg_pm_lock) esp_pm_lock_acquire(s_ecg_pm_lock);
#endif
            ad8232_start_dma();
            is_dma_running = true;
        }

        esp_err_t ret = adc_continuous_read(ad8232_get_adc_handle(), dma_buf,
                                            AD8232_READ_LEN, &ret_num, pdMS_TO_TICKS(100));
        if (ret == ESP_OK) {
            for (int i = 0; i < ret_num; i += sizeof(adc_digi_output_data_t)) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t*)&dma_buf[i];
                uint16_t raw_val = p->type2.data;
                sum += raw_val; count++;
                if (count >= ECG_DOWNSAMPLE_RATIO) {
                    ble_chunk[chunk_idx++] = (int16_t)(sum / ECG_DOWNSAMPLE_RATIO);
                    sum = 0; count = 0;
                    if (chunk_idx >= ECG_BLE_CHUNK_SIZE) {
                        ble_telemetry_send_ecg(ble_chunk, sizeof(ble_chunk));
                        chunk_idx = 0;
                    }
                }
            }
        }
    }
}

/* ─────────────── IMU task: lee + step_algo + jerk + envío directo BLE ─────────────── */
void imu_task(void *pvParameter) {
    int16_t imu_raw[6] = {0};
    step_algo_state_t sw_pedometer;
    step_algo_init(&sw_pedometer);

    int16_t prev_ax = 0, prev_ay = 0, prev_az = 0;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1) {
        const power_profile_t *p = power_get_profile();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(p->imu_poll_ms));

        esp_err_t err = bmi160_read_accel_gyro(&imu_raw[0], &imu_raw[1], &imu_raw[2],
                                                &imu_raw[3], &imu_raw[4], &imu_raw[5]);
        if (err == ESP_OK) {
            uint32_t now = now_ms();

            /* Cálculo de jerk simple: |Δa| escalado a 0..255 */
            int32_t dx = imu_raw[0] - prev_ax;
            int32_t dy = imu_raw[1] - prev_ay;
            int32_t dz = imu_raw[2] - prev_az;
            int32_t mag2 = dx*dx + dy*dy + dz*dz;
            /* Umbral: ±2g ≈ 16384 LSB. Δa de 4000 = movimiento moderado.
             * mag2 ~ 16e6 → jerk_score ~ 80 (justo el threshold). */
            uint32_t jerk = mag2 / 200000;
            if (jerk > 255) jerk = 255;
            max30102_set_motion_level((uint8_t)jerk);
            prev_ax = imu_raw[0]; prev_ay = imu_raw[1]; prev_az = imu_raw[2];

            uint32_t new_steps = step_algo_update(&sw_pedometer,
                imu_raw[0], imu_raw[1], imu_raw[2],
                imu_raw[3], imu_raw[4], imu_raw[5], now);

            /* Envío IMU directo (no agregado): SPORT 50Hz, NORMAL 25Hz, SAVER 12.5Hz */
            if (app_state_imu_tx_enabled()) {
                ble_telemetry_send_imu(imu_raw, sizeof(imu_raw));
            }

            shared_sensor_data_t *sd = app_state_lock(10);
            if (sd) {
                sd->ax = imu_raw[0]; sd->ay = imu_raw[1]; sd->az = imu_raw[2];
                sd->gx = imu_raw[3]; sd->gy = imu_raw[4]; sd->gz = imu_raw[5];
                sd->steps_sw += new_steps;
                app_state_unlock();
            }
        }
    }
}

/* ─────────────── HRM task: SM con modos energéticos ─────────────── */

static max30102_spot_state_t s_last_spot_state_handled = SPOT_STATE_IDLE;
static uint32_t s_last_auto_spot_ms = 0;
static uint32_t s_last_continuous_publish_ms = 0;

static void publish_hr_tlv(uint8_t bpm, uint8_t quality) {
    uint8_t rec[4] = {0};
    /* delta_ms = 0 (relativo al header del agg flush) */
    rec[2] = bpm;
    rec[3] = quality;
    ble_tx_push(BLE_TLV_TYPE_HR, rec, sizeof(rec), 0xFF);
}

static void publish_spo2_tlv(uint8_t pct, uint8_t quality) {
    uint8_t rec[4] = {0};
    rec[2] = pct;
    rec[3] = quality;
    ble_tx_push(BLE_TLV_TYPE_SPO2, rec, sizeof(rec), 0xFF);
}

static void publish_spot_result(const max30102_spot_status_t *st) {
    uint8_t rec[6];
    rec[0] = st->bpm;
    rec[1] = st->spo2;
    rec[2] = (uint8_t)(st->duration_ms & 0xFF);
    rec[3] = (uint8_t)((st->duration_ms >> 8) & 0xFF);
    rec[4] = (uint8_t)st->quality;
    rec[5] = (st->state == SPOT_STATE_ABORTED || st->state == SPOT_STATE_FAILED) ? 1 : 0;
    /* Forzar flush con resultado SPOT (alta prioridad) */
    ble_tx_push(BLE_TLV_TYPE_SPOT_RESULT, rec, sizeof(rec), (uint8_t)power_get_mode());
}

void hrm_task(void *pvParameter) {
    max30102_sample_t samples[32];
    max30102_flush_fifo();

    uint32_t last_ovf_logged = 0;

    while (1) {
        const power_profile_t *p = power_get_profile();
        bool continuous = (p->hrm_auto_period_ms == 0);

        // Si nunca ha medido, tomamos una medición rápida
        static bool has_measured_once = false;
        if (!continuous && !has_measured_once) {
            has_measured_once = true;
            s_last_auto_spot_ms = now_ms() - p->hrm_auto_period_ms; // Forzar que arranque la primera vez
        }

        max30102_spot_status_t spot_st;
        max30102_spot_get_status(&spot_st);
        bool spot_active = (spot_st.state == SPOT_STATE_SETTLING ||
                            spot_st.state == SPOT_STATE_MEASURING);

        bool sensor_should_be_on = continuous || spot_active;

        /* Auto-spot en modos NORMAL/SAVER cuando vence el período */
        if (!continuous && !spot_active &&
            (now_ms() - s_last_auto_spot_ms) >= p->hrm_auto_period_ms) {
            if (!max30102_is_awake()) max30102_wake();
            max30102_spot_start();
            s_last_auto_spot_ms = now_ms();
            sensor_should_be_on = true;
            ESP_LOGI(TAG, "HRM auto-spot iniciado (modo %s)", power_mode_name(power_get_mode()));
        }

        if (sensor_should_be_on && !max30102_is_awake()) {
            max30102_wake();
        } else if (!sensor_should_be_on && max30102_is_awake() && p->hrm_shdn_between) {
            max30102_shutdown();
        }

        /* Si el sensor está activo, leer FIFO y procesar */
        if (max30102_is_awake()) {
            uint8_t n = 0;
            if (max30102_read_samples(samples, 32, &n) == ESP_OK && n > 0) {
                for (uint8_t i = 0; i < n; i++) {
                    max30102_process_sample(samples[i].red, samples[i].ir);
                }
            }

            /* Publicar a sensor_data + BLE */
            uint8_t bpm = 0, spo2 = 0;
            max30102_get_hr(&bpm);
            max30102_get_spo2(&spo2);
            bool finger = max30102_finger_present();

            shared_sensor_data_t *sd = app_state_lock(10);
            if (sd) {
                sd->finger_present = finger;
                if (bpm > 0)  { sd->hr_bpm = bpm;     sd->hr_updated_ms = now_ms(); }
                if (spo2 > 0) { sd->spo2_pct = spo2;  sd->spo2_updated_ms = now_ms(); }
                if (!finger)  { sd->hr_bpm = 0; sd->spo2_pct = 0; }
                app_state_unlock();
            }

            /* En SPORT publica HR/SpO2 cada 1 s al stream agregado */
            if (continuous && bpm > 0 &&
                (now_ms() - s_last_continuous_publish_ms) >= 1000) {
                publish_hr_tlv(bpm, 1);
                if (spo2 > 0) publish_spo2_tlv(spo2, 1);
                s_last_continuous_publish_ms = now_ms();
            }

            /* Manejo de transición SPOT terminado */
            max30102_spot_get_status(&spot_st);
            if (spot_st.state != s_last_spot_state_handled &&
                (spot_st.state == SPOT_STATE_DONE   ||
                 spot_st.state == SPOT_STATE_FAILED ||
                 spot_st.state == SPOT_STATE_ABORTED)) {
                publish_spot_result(&spot_st);
                if (spot_st.state == SPOT_STATE_DONE) {
                    shared_sensor_data_t *sd = app_state_lock(10);
                    if (sd) {
                        sd->hr_bpm          = spot_st.bpm;
                        sd->spo2_pct        = spot_st.spo2;
                        sd->hr_updated_ms   = now_ms();
                        sd->spo2_updated_ms = now_ms();
                        app_state_unlock();
                    }
                }
                s_last_spot_state_handled = spot_st.state;

                /* Si era auto-spot en NORMAL/SAVER, dormir el sensor */
                if (!continuous) max30102_shutdown();
            }
            /* Si volvimos a IDLE (caller hizo spot_start de nuevo), resetear handled */
            if (spot_st.state == SPOT_STATE_IDLE ||
                spot_st.state == SPOT_STATE_SETTLING ||
                spot_st.state == SPOT_STATE_MEASURING) {
                s_last_spot_state_handled = SPOT_STATE_IDLE;
            }

            uint32_t ovf = max30102_get_overflow_count();
            if (ovf - last_ovf_logged >= 10) {
                ESP_LOGW(TAG, "MAX30102 overflows: %lu", (unsigned long)ovf);
                last_ovf_logged = ovf;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(p->hrm_poll_ms));
    }
}

/* ─────────────── system_task: temp + batería con cadencia por modo ─────────────── */
void system_task(void *pvParameter) {
    uint32_t last_temp_ms = 0;
    uint32_t last_bat_ms  = 0;
    uint32_t last_steps_pub_ms = 0;

    while (1) {
        const power_profile_t *p = power_get_profile();
        uint32_t now = now_ms();

        if (now - last_bat_ms >= p->bat_period_ms) {
            uint16_t bat_mv = 0;
            float bat_soc_raw = 0.0f;
            max17048_get_voltage(&bat_mv);
            esp_err_t err_soc = max17048_get_soc(&bat_soc_raw);

            // Filtro EMA para SOC
            static float bat_soc_filtered = -1.0f;
            if (err_soc == ESP_OK) {
                if (bat_soc_filtered < 0.0f) {
                    bat_soc_filtered = bat_soc_raw;
                } else {
                    bat_soc_filtered = 0.1f * bat_soc_raw + 0.9f * bat_soc_filtered;
                }
            }

            shared_sensor_data_t *sd = app_state_lock(10);
            if (sd) {
                sd->battery_mv     = bat_mv;
                sd->battery_soc    = (bat_soc_filtered >= 0.0f) ? bat_soc_filtered : 0.0f;
                sd->bat_updated_ms = now;
                app_state_unlock();
            }

            uint8_t rec[5] = {0};
            /* delta_ms relativo al header (pongo 0; el header trae el ts base) */
            memcpy(&rec[2], &bat_mv, 2);
            rec[4] = (uint8_t)((bat_soc_filtered >= 0.0f) ? bat_soc_filtered : 0.0f);
            ble_tx_push(BLE_TLV_TYPE_BAT, rec, sizeof(rec), 0xFF);
            last_bat_ms = now;
        }

        if (now - last_temp_ms >= p->temp_period_ms) {
            float t = 0.0f;
            if (max30205_read_temperature(&t) == ESP_OK) {
                shared_sensor_data_t *sd = app_state_lock(10);
                if (sd) {
                    sd->temperature_c   = t;
                    sd->temp_updated_ms = now;
                    app_state_unlock();
                }
                int16_t tx100 = (int16_t)(t * 100.0f);
                uint8_t rec[4] = {0};
                memcpy(&rec[2], &tx100, 2);
                ble_tx_push(BLE_TLV_TYPE_TEMP, rec, sizeof(rec), 0xFF);
            }
            last_temp_ms = now;
        }

        /* Pasos cada 30 s siempre (es info muy resumida) */
        if (now - last_steps_pub_ms >= 30 * 1000) {
            uint32_t steps = 0;
            shared_sensor_data_t *sd = app_state_lock(10);
            if (sd) {
                steps = sd->steps_sw;
                app_state_unlock();
            }
            uint8_t rec[4];
            memcpy(rec, &steps, 4);
            ble_tx_push(BLE_TLV_TYPE_STEPS, rec, sizeof(rec), 0xFF);
            last_steps_pub_ms = now;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* ─────────────── ble_tx_task: flush periódico del buffer agregado ─────────────── */
void ble_tx_task(void *pvParameter) {
    while (1) {
        const power_profile_t *p = power_get_profile();
        vTaskDelay(pdMS_TO_TICKS(p->ble_agg_flush_ms));
        ble_tx_flush((uint8_t)power_get_mode());
    }
}

/* ═══════════════════════════════════════════════════════════════════
 *  app_main
 * ═══════════════════════════════════════════════════════════════════ */

void perf_monitor_task(void *pvParameter) {
    while(1) {
        ESP_LOGI("PERF", "--- Rendimiento ---");
        ESP_LOGI("PERF", "Heap Libre: %lu bytes", (unsigned long)esp_get_free_heap_size());
        ESP_LOGI("PERF", "Heap Min Libre: %lu bytes", (unsigned long)esp_get_minimum_free_heap_size());
        
#if 1
        char stats_buffer[1024];
        vTaskList(stats_buffer);
        ESP_LOGI("PERF", "=== Lista de Tareas (Estado, Prioridad, Pila (Libre), Task_Num) ===\n%s", stats_buffer);
        
        char runtime_buffer[1024];
        vTaskGetRunTimeStats(runtime_buffer);
        ESP_LOGI("PERF", "=== Uso de CPU ===\n%s", runtime_buffer);
#endif

#if CONFIG_PM_PROFILING
        ESP_LOGI("PERF", "=== Power Manager Locks ===");
        esp_pm_dump_locks(stdout);   
#endif
        
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void supaclock_app_run(void) {
    ESP_LOGI(TAG, "=== INICIANDO ENTORNO TEST GENERAL ===");

    /* Inicializar Power Management (Light Sleep Dinámico).
     * esp_pm_config_t es el tipo portable en IDF 5.x; en S3 podemos
     * subir el máximo a 240 MHz, en C3 nos quedamos en 160 MHz. */
#if CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {
#if CONFIG_IDF_TARGET_ESP32S3
        .max_freq_mhz = 240,
        .min_freq_mhz = 80,
#else
        .max_freq_mhz = 160,
        .min_freq_mhz = 10,
#endif
        .light_sleep_enable = true
    };
    if (esp_pm_configure(&pm_config) == ESP_OK) {
        ESP_LOGI(TAG, "Power Management: Automático Light Sleep HABILIADO!");
    }
    if (esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "ecg", &s_ecg_pm_lock) != ESP_OK) {
        ESP_LOGW(TAG, "ECG PM lock no se pudo crear — light sleep podría meter ruido al ECG");
        s_ecg_pm_lock = NULL;
    }
#endif

    /* Bajar verbosidad del stack BLE (tags más comunes) */
    esp_log_level_set("NimBLE",     ESP_LOG_WARN);
    esp_log_level_set("NimBLE_GAP", ESP_LOG_WARN);
    esp_log_level_set("BLE_GAP",    ESP_LOG_WARN);
    esp_log_level_set("BLE_GATT",   ESP_LOG_WARN);
    esp_log_level_set("BTDM_INIT",  ESP_LOG_WARN);
    esp_log_level_set("phy_init",   ESP_LOG_WARN);

    /* NVS para persistir modo y settings */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    power_modes_init();
    ui_theme_init();
    ESP_LOGI(TAG, "Modo inicial: %s, tema: %s",
             power_mode_name(power_get_mode()), ui_theme_name(ui_theme_get_id()));

    xGuiSemaphore = xSemaphoreCreateMutex();
    app_state_init();

    /* ── FASE 1: I2C y sensores ── */
    ESP_LOGI(TAG, "[Fase 1] I2C + sensores...");
    if (i2c_master_init() != ESP_OK) ESP_LOGE(TAG, "I2C Bus failed!");
    if (gpio_buttons_init() != ESP_OK) ESP_LOGW(TAG, "gpio_buttons_init falló");

    if (max17048_init() != ESP_OK) ESP_LOGW(TAG, "MAX17048 ausente");

    /* BMI160: NO habilitar step counter HW (no irá en producción) */
    if (bmi160_init() != ESP_OK) ESP_LOGE(TAG, "BMI160 init falló");

    if (max30205_init() != ESP_OK) ESP_LOGW(TAG, "MAX30205 ausente");
    if (max30102_init_hrm() != ESP_OK) ESP_LOGW(TAG, "MAX30102 ausente");

    if (ad8232_init_dma() == ESP_OK) {
        ESP_LOGI(TAG, "AD8232 DMA configurado (No iniciado, modo bajo consumo activo)");
    } else {
        ESP_LOGW(TAG, "AD8232 ausente");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    /* ── FASE 2: Display + UI ── */
    ESP_LOGI(TAG, "[Fase 2] Display + UI...");
    st7789_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    st7789_fill_screen(0x0000);
    ui_init();
    ui_set_actions(&s_ui_actions);
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* ── FASE 3: BLE ── */
    ESP_LOGI(TAG, "[Fase 3] BLE...");
    if (ble_telemetry_init() != ESP_OK) ESP_LOGE(TAG, "BLE Stack falló");

    /* Tasks */
    xTaskCreate(gui_task,    "gui_task",    4096, NULL, 5, NULL);
    xTaskCreate(imu_task,    "imu_task",    5120, NULL, 6, NULL);
    xTaskCreate(hrm_task,    "hrm_task",    4096, NULL, 5, NULL);
    xTaskCreate(system_task, "system_task", 4096, NULL, 3, NULL);
    xTaskCreate(ble_tx_task, "ble_tx_task", 4096, NULL, 4, NULL);  /* +1024: el HWM medido era 960 B */
    xTaskCreate(ecg_task,    "ecg_task",    4096, NULL, 7, NULL);
    xTaskCreate(perf_monitor_task, "perf_task", 6144, NULL, 2, NULL); /* vTaskList+RunTimeStats consumen ~3.8 KB */

    ESP_LOGI(TAG, "=== SISTEMA INICIADO ===");
}


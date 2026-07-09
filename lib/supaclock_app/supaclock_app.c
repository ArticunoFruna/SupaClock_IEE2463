#include "supaclock_app.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "gc9a01.h"
#include "cst816s.h"
#include "i2c_bus.h"
#include "supaclock_pinmap.h"
#include "max17048.h"
#include "bmi160.h"
#include "max30205.h"
#include "max30102.h"
#include "ble_telemetry.h"
#include "step_algorithm.h"
#include "har_cnn1d.h"
#include "gpio_buttons.h"
#include "ad8232.h"
#include "esp_adc/adc_continuous.h"
#include "power_modes.h"
#include "ui_theme.h"
#include "app_state.h"
#ifdef ENV_MAIN_NOTOUCH
  #include "supaclock_ui_notouch.h"
#else
  #include "supaclock_ui.h"
#endif

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
 * traza). El backlight tiene su propio lock dentro de gc9a01. */
#if CONFIG_PM_ENABLE
static esp_pm_lock_handle_t s_ecg_pm_lock = NULL;
#endif

/* ───────────────────────── Acciones de sistema para la UI ─────────────────
 * La UI llama estos callbacks (registrados con ui_set_actions) para acciones
 * que tocan hardware/IDF que la lib UI no debe conocer (deep sleep, MAX17048).
 */
static void on_power_off(void) {
    ESP_LOGI(TAG, "Entering Ordered Shutdown / Deep Sleep Sequence...");

    // 1. Apagar peripherals de UI + ECG
    gc9a01_set_brightness(0);
    cst816s_shutdown();
    ad8232_power_down();

    // 2. Apagar sensores I2C: BMI160 (IMU), MAX30102 (HR/SpO2), MAX30205 (temp).
    //    MAX17048 (fuel gauge) queda activo — auto-hibernate cuando crate<3%.
    //    Consumo total pre-sleep: ~950+600+600=~2 mA → post-sleep: <10 µA total.
    bmi160_suspend();
    max30102_shutdown();
    max30205_shutdown();

    // 3. Delay para completar transmisiones y apagado físico
    vTaskDelay(pdMS_TO_TICKS(200));

#if CONFIG_IDF_TARGET_ESP32S3
    /* 3. Desactivar TODAS las wake sources posibles para no despertar por
     *    fantasmas (timer del PM, otros GPIOs floating, etc.). Solo ext1
     *    queda como wake válido. */
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    /* 4. GPIO8 (BTN_SELECT) como RTC-GPIO con pull-up retenido en sleep.
     *    Antes usábamos gpio_config + gpio_hold_en (digital hold), que
     *    no retiene bien el pull-up sin RTC_PERIPH ON → pin flotaba →
     *    despertar espúreo a los ~7s. La secuencia correcta es:
     *      rtc_gpio_init → set_direction INPUT → pullup_en → hold_en. */
    rtc_gpio_deinit(BTN_SELECT_PIN);
    rtc_gpio_init(BTN_SELECT_PIN);
    rtc_gpio_set_direction(BTN_SELECT_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(BTN_SELECT_PIN);
    rtc_gpio_pullup_en(BTN_SELECT_PIN);
    rtc_gpio_hold_en(BTN_SELECT_PIN);

    /* 5. Mantener el dominio RTC_PERIPH ON para retener el pull-up interno.
     *    Sin esto el dominio se apaga → pull-up muere → GPIO flota. */
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    /* 6. Wake source: ext1 en BTN_SELECT, activo bajo (botón pull-up). */
    esp_sleep_enable_ext1_wakeup_io((1ULL << BTN_SELECT_PIN), ESP_EXT1_WAKEUP_ANY_LOW);
#else
    esp_deep_sleep_enable_gpio_wakeup((1ULL << BTN_SELECT_PIN), ESP_GPIO_WAKEUP_GPIO_LOW);
#endif

    ESP_LOGI(TAG, "Starting Deep Sleep — press SELECT >=3s to power on, >=6s force reboot");
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
    gc9a01_set_brightness(power_get_display_brightness(power_get_mode()));
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
                    gc9a01_set_brightness(power_get_display_brightness(power_get_mode()));
                    backlight_on = true;
                } else {
                    ui_handle_button(ev);
                }
            }

            /* El touch alimenta LVGL vía indev; el flag lo pone touch_read_cb
             * cuando detecta un press y se limpia al leerlo aquí. Sirve para
             * despertar el backlight y refrescar el contador de auto-off sin
             * duplicar el path del botón. */
            if (ui_take_and_clear_touch_activity()) {
                action_taken = true;
                last_activity_ms = now_ms();
                if (!backlight_on) {
                    gc9a01_set_brightness(power_get_display_brightness(power_get_mode()));
                    backlight_on = true;
                }
            }

            if (!action_taken) {
                /* auto-off según modo activo */
                uint16_t off_s = power_get_display_off_s(power_get_mode());
                if (now_ms() - last_activity_ms >= off_s * 1000 && backlight_on) {
                    gc9a01_set_brightness(0);
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

/* ─────────────── IMU task: drena FIFO @50Hz + step_algo + jerk + BLE ─────────────── */
void imu_task(void *pvParameter) {
    step_algo_state_t sw_pedometer;
    step_algo_init(&sw_pedometer);

    int16_t prev_ax = 0, prev_ay = 0, prev_az = 0;
    /* Timestamp uniforme por muestra (50 Hz). El BMI160 muestrea el FIFO a ODR
     * constante, así que asignamos t0 + i*20 ms en vez de now_ms() por muestra:
     * esto garantiza espaciado uniforme para la FFT del pedómetro. */
    uint32_t sample_ts = now_ms();

    bmi160_fifo_frame_t frames[32];

    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1) {
        const power_profile_t *p = power_get_profile();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(p->imu_poll_ms));

        size_t n = 0;
        esp_err_t err = bmi160_read_fifo(frames, 32, &n);

        /* DEBUG: contar frames/seg reales y polls/seg para medir el feed. */
        static uint32_t s_fps = 0, s_polls = 0;
        s_polls++;
        s_fps += n;

        if (err != ESP_OK || n == 0) {
            /* Aún así emitir el log si toca, para ver polls aunque n=0 */
            static uint32_t s_dbg0 = 0;
            uint32_t d0 = now_ms();
            if (d0 - s_dbg0 >= 1000) {
                s_dbg0 = d0;
                ESP_LOGD(TAG, "PED feed: polls/s=%lu frames/s=%lu (n=0 ahora) idx=%u",
                         (unsigned long)s_polls, (unsigned long)s_fps,
                         sw_pedometer.sample_index);
                s_polls = 0; s_fps = 0;
            }
            continue;
        }

        uint32_t new_steps_total = 0;
        int16_t last[6] = {0};

        for (size_t i = 0; i < n; i++) {
            bmi160_fifo_frame_t *f = &frames[i];
            new_steps_total += step_algo_update(&sw_pedometer,
                f->ax, f->ay, f->az, f->gx, f->gy, f->gz, sample_ts);
            /* Mismo stream de 50 Hz alimenta el ring del HAR (un solo lector del
             * BMI160 → sin contención I2C ni FIFO overflow). */
            har_cnn1d_push_sample(f->ax, f->ay, f->az, f->gx, f->gy, f->gz);
            sample_ts += 20;  /* 50 Hz uniforme */

            /* Envío IMU directo (preserva el stream a 50 Hz) */
            if (app_state_imu_tx_enabled()) {
                int16_t raw[6] = { f->ax, f->ay, f->az, f->gx, f->gy, f->gz };
                ble_telemetry_send_imu(raw, sizeof(raw));
            }
            last[0]=f->ax; last[1]=f->ay; last[2]=f->az;
            last[3]=f->gx; last[4]=f->gy; last[5]=f->gz;
        }

        /* Jerk con la última muestra del burst (gating del HRM): |Δa| → 0..255 */
        int32_t dx = last[0] - prev_ax;
        int32_t dy = last[1] - prev_ay;
        int32_t dz = last[2] - prev_az;
        int32_t mag2 = dx*dx + dy*dy + dz*dz;
        uint32_t jerk = mag2 / 200000;
        if (jerk > 255) jerk = 255;
        max30102_set_motion_level((uint8_t)jerk);
        prev_ax = last[0]; prev_ay = last[1]; prev_az = last[2];

        uint32_t steps_now = 0;
        shared_sensor_data_t *sd = app_state_lock(10);
        if (sd) {
            sd->ax = last[0]; sd->ay = last[1]; sd->az = last[2];
            sd->gx = last[3]; sd->gy = last[4]; sd->gz = last[5];
            sd->steps_sw += new_steps_total;
            steps_now = sd->steps_sw;
            app_state_unlock();
        }

        /* DEBUG pedómetro: 1/seg por USB y por BLE (TLV 0x10 → app). Quitar tras calibrar. */
        static uint32_t s_dbg_last = 0;
        uint32_t dbg = now_ms();
        if (dbg - s_dbg_last >= 1000) {
            s_dbg_last = dbg;
            bool gate_active = (sample_ts < sw_pedometer.walk_gate_expiry_ms);
            float amp = sw_pedometer.peak_env - sw_pedometer.valley_env;

            ESP_LOGD(TAG, "PED fps=%lu idx=%u ffts=%u pkHz=%.2f amp=%.0f prom=%.1f ratio=%.2f gate=%d cad=%.2f cons=%u prov=%u steps=%lu",
                     (unsigned long)s_fps, sw_pedometer.sample_index,
                     sw_pedometer.dbg_fft_runs, (double)sw_pedometer.dbg_peak_hz,
                     (double)amp, (double)sw_pedometer.dbg_prominence,
                     (double)sw_pedometer.dbg_ratio, gate_active,
                     (double)sw_pedometer.cadence_hz, sw_pedometer.consecutive_steps,
                     sw_pedometer.provisional_steps, (unsigned long)steps_now);
            s_polls = 0; s_fps = 0;

            /* Empaquetar para la app (BLE_TLV_TYPE_PED_DBG, little-endian) */
            uint8_t rec[13];
            uint16_t amp_u16  = amp > 65535.0f ? 65535 : (uint16_t)amp;
            float promx = sw_pedometer.dbg_prominence * 10.0f;
            uint16_t prom_u16 = promx > 65535.0f ? 65535 : (uint16_t)promx;
            uint8_t ratio_u8 = (uint8_t)(sw_pedometer.dbg_ratio * 100.0f);
            float pkx = sw_pedometer.dbg_peak_hz * 10.0f;
            uint8_t pkhz_u8 = pkx > 255.0f ? 255 : (uint8_t)pkx;
            float cadx = sw_pedometer.cadence_hz * 10.0f;
            uint8_t cad_u8 = cadx > 255.0f ? 255 : (uint8_t)cadx;
            memcpy(&rec[0], &amp_u16, 2);
            memcpy(&rec[2], &prom_u16, 2);
            rec[4] = ratio_u8;
            rec[5] = pkhz_u8;
            rec[6] = cad_u8;
            rec[7] = gate_active ? 1 : 0;
            rec[8] = sw_pedometer.consecutive_steps;
            memcpy(&rec[9], &steps_now, 4);
            ble_tx_push(BLE_TLV_TYPE_PED_DBG, rec, sizeof(rec), 0xFF);
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

        /* Con pantalla encendida forzamos período corto (1 s) para que el
         * indicador de "cargando" reaccione al instante cuando el usuario
         * enchufa el USB. Con backlight off usamos el periodo largo del
         * profile (30 s) para ahorrar energía. */
        uint32_t bat_period = backlight_on ? 1000 : p->bat_period_ms;
        if (now - last_bat_ms >= bat_period) {
            uint16_t bat_mv = 0;
            float bat_soc_raw = 0.0f;
            float bat_crate = 0.0f;
            max17048_get_voltage(&bat_mv);
            esp_err_t err_soc = max17048_get_soc(&bat_soc_raw);
            max17048_get_crate(&bat_crate);

            // Filtro EMA para SOC
            static float bat_soc_filtered = -1.0f;
            if (err_soc == ESP_OK) {
                if (bat_soc_filtered < 0.0f) {
                    bat_soc_filtered = bat_soc_raw;
                } else {
                    bat_soc_filtered = 0.1f * bat_soc_raw + 0.9f * bat_soc_filtered;
                }
            }

            /* Detección de charging robusta (dos señales, cada una filtrada):
             *  1) Trend de voltaje: comparar avg(muestras recientes) vs
             *     avg(muestras antiguas) en un buffer de 30 s. Filtra los
             *     spikes de carga del CPU que dan falsos positivos si solo
             *     miramos delta puntual.
             *  2) crate en %/h con umbrales anchos: gran deadband para no
             *     togglear en CV-phase (crate ~ 0 cerca del 100%).
             * Fusión: OR para prender, requiere ambas para apagar (evita
             * flapping cuando solo una queda en deadband). */
            #define BAT_HIST_N   30
            #define BAT_AVG_WIN  5
            static uint16_t bat_mv_hist[BAT_HIST_N] = {0};
            static uint8_t  bat_mv_hist_idx = 0;
            static uint8_t  bat_mv_hist_cnt = 0;
            bat_mv_hist[bat_mv_hist_idx] = bat_mv;
            bat_mv_hist_idx = (bat_mv_hist_idx + 1) % BAT_HIST_N;
            if (bat_mv_hist_cnt < BAT_HIST_N) bat_mv_hist_cnt++;

            int mv_trend = 0;
            if (bat_mv_hist_cnt >= BAT_HIST_N) {
                /* avg de las 5 más recientes vs 5 más antiguas */
                uint32_t sum_new = 0, sum_old = 0;
                for (int k = 0; k < BAT_AVG_WIN; ++k) {
                    int i_new = (bat_mv_hist_idx + BAT_HIST_N - 1 - k) % BAT_HIST_N;
                    int i_old = (bat_mv_hist_idx + k) % BAT_HIST_N;
                    sum_new += bat_mv_hist[i_new];
                    sum_old += bat_mv_hist[i_old];
                }
                mv_trend = (int)(sum_new / BAT_AVG_WIN) - (int)(sum_old / BAT_AVG_WIN);
            }

            static bool bat_is_charging = false;
            bool by_crate_on  = (bat_crate >  0.5f);
            bool by_crate_off = (bat_crate < -0.5f);
            bool by_mv_on     = (mv_trend  >  3);   /* subió ≥3 mV avg-a-avg */
            bool by_mv_off    = (mv_trend  < -2);   /* bajó ≥2 mV avg-a-avg */
            if      (by_crate_on  || by_mv_on)   bat_is_charging = true;
            /* Solo apaga si AMBAS señales lo confirman — evita false-off por
             * caídas transitorias de voltaje bajo load. */
            else if (by_crate_off && by_mv_off)  bat_is_charging = false;

            /* Log throttled: cada 10 lecturas */
            static uint32_t bat_log_ctr = 0;
            if (++bat_log_ctr % 10 == 0) {
                ESP_LOGI(TAG, "BAT: mv=%u trend=%+d soc=%.1f%% crate=%+.2f%%/h chg=%d",
                         (unsigned)bat_mv, mv_trend, (double)bat_soc_filtered,
                         (double)bat_crate, bat_is_charging);
            }

            shared_sensor_data_t *sd = app_state_lock(10);
            if (sd) {
                sd->battery_mv       = bat_mv;
                sd->battery_soc      = (bat_soc_filtered >= 0.0f) ? bat_soc_filtered : 0.0f;
                sd->battery_crate    = bat_crate;
                sd->battery_charging = bat_is_charging;
                sd->bat_updated_ms   = now;
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

/* ─────────────── HAR: callback de inferencia (corre en har_task, core 1) ───────────────
 *
 * El modelo entrega probs[4] crudas por ventana (cada 2 s). Aquí aplicamos el
 * mismo post-proceso documentado en ble_har_protocol.md §2.4:
 *   1. EMA (α=0.5) sobre las probs, inicializado dominante en RESTING.
 *   2. Consolidación: el estado solo cambia tras 3 ventanas consecutivas con el
 *      mismo argmax(EMA) → histéresis anti-parpadeo.
 * El estado consolidado se publica a app_state (UI local) y al TLV 0x08 (BLE).
 * No bloquear aquí: es el callback de la task del HAR. */
/* Callback del canal de comando BLE (opcode 0x02 SYNC_TIME).
 * Recibimos un unix ts en segundos desde la app y lo aplicamos con
 * settimeofday para que time(NULL) apunte al reloj de pared. */
static void on_ble_sync_time(uint32_t unix_ts) {
    struct timeval tv = { .tv_sec = (time_t)unix_ts, .tv_usec = 0 };
    if (settimeofday(&tv, NULL) != 0) {
        ESP_LOGW(TAG, "settimeofday fallo (ts=%u)", (unsigned)unix_ts);
    } else {
        ESP_LOGI(TAG, "RTC sincronizado desde BLE (ts=%u)", (unsigned)unix_ts);
    }
}

static void on_har_result(const har_result_t *result, void *user) {
    (void)user;
    static float       s_ema[HAR_NUM_CLASSES] = {1.0f, 0.0f, 0.0f, 0.0f};
    static har_state_t s_candidate    = HAR_STATE_RESTING;
    static har_state_t s_consolidated = HAR_STATE_RESTING;
    static int         s_consec       = 0;

    /* EMA sobre todas las salidas, pero argmax SOLO sobre las clases activas
     * (escaleras/idx 3 deshabilitada hasta tener dataset → ver HAR_ACTIVE_CLASSES). */
    int argmax = 0;
    for (int i = 0; i < HAR_NUM_CLASSES; ++i) {
        s_ema[i] = 0.5f * result->probs[i] + 0.5f * s_ema[i];
        if (i < HAR_ACTIVE_CLASSES && s_ema[i] > s_ema[argmax]) argmax = i;
    }

    if (argmax == (int)s_candidate) {
        if (s_consec < 3) s_consec++;
    } else {
        s_candidate = (har_state_t)argmax;
        s_consec = 1;
    }
    if (s_consec >= 3) s_consolidated = s_candidate;

    shared_sensor_data_t *sd = app_state_lock(10);
    if (sd) {
        sd->har_state = (uint8_t)s_consolidated;
        sd->har_updated_ms = now_ms();
        app_state_unlock();
    }

    /* TLV 0x08 al buffer agregado (sin forzar flush: 0xFF). */
    uint8_t state_val = (uint8_t)s_consolidated;
    ble_tx_push(BLE_TLV_TYPE_HAR_STATE, &state_val, 1, 0xFF);
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
#if CONFIG_IDF_TARGET_ESP32S3
    /* Liberar el hold del RTC-GPIO y volver a modo digital normal para que
     * gpio_buttons y el resto del sistema puedan usar SELECT como I/O. */
    rtc_gpio_hold_dis(BTN_SELECT_PIN);
    rtc_gpio_deinit(BTN_SELECT_PIN);
#endif
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "=== SUPACLOCK BOOT / WAKEUP ===");
    ESP_LOGI(TAG, "Wakeup Cause: %d", cause);
    ESP_LOGI(TAG, "=========================================");

    // 1. Si venimos de deep sleep, validar hold-to-boot (3s) o force reboot (6s).
    if (cause == ESP_SLEEP_WAKEUP_EXT1 || cause == ESP_SLEEP_WAKEUP_GPIO) {
        // Reconfigurar GPIO8 como digital input con pull-up para leerlo.
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << BTN_SELECT_PIN),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);

        const int BOOT_HOLD_MS   = 3000;   // >=3s = arranca
        const int REBOOT_HOLD_MS = 6000;   // >=6s = restart forzado
        const int MAX_HOLD_MS    = 6500;   // cap para no bloquear infinito
        int held_ms = 0;
        ESP_LOGI(TAG, "Checking SELECT button hold (>=3s=boot, >=6s=force reboot)...");

        // Espera activa mientras el botón esté presionado (activo bajo = 0).
        while (gpio_get_level(BTN_SELECT_PIN) == 0 && held_ms < MAX_HOLD_MS) {
            vTaskDelay(pdMS_TO_TICKS(50));
            held_ms += 50;
            if (held_ms % 500 == 0) {
                ESP_LOGI(TAG, "SELECT held %d ms...", held_ms);
            }
        }

        if (held_ms >= REBOOT_HOLD_MS) {
            ESP_LOGW(TAG, "SELECT held >=6s → force reboot");
            esp_restart();
        } else if (held_ms >= BOOT_HOLD_MS) {
            ESP_LOGI(TAG, "SELECT held %d ms → booting", held_ms);
        } else {
            ESP_LOGI(TAG, "SELECT released early (%d ms) → back to sleep", held_ms);
            on_power_off();
        }
    }

    /* Delay de arranque: el reset por RTS del esptool re-enumera la USB CDC,
     * lo que toma ~1-2s. Sin este delay, los logs del boot temprano se pierden
     * porque el monitor todavia no reconectó. 3s da margen para ver el scan
     * I2C y el init del CST816S al capturar logs con:
     *   pio run -e main_notouch -t upload -t monitor
     */
    for (int i = 3; i > 0; i--) {
        ESP_LOGI(TAG, "Boot en %d...", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
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

    /* Timezone Chile continental (UTC-4 estándar, UTC-3 en DST).
     * Sin esto, localtime_r devuelve UTC porque la app envía unix_ts UTC
     * vía SYNC_TIME. Regla POSIX: DST empieza 1er sábado de septiembre 24:00
     * y termina 1er sábado de abril 24:00. Ajustar si cambia la ley horaria. */
    setenv("TZ", "<-04>4<-03>,M9.1.6/24,M4.1.6/24", 1);
    tzset();
    power_modes_init();
    ui_theme_init();
    ESP_LOGI(TAG, "Modo inicial: %s, tema: %s",
             power_mode_name(power_get_mode()), ui_theme_name(ui_theme_get_id()));

    xGuiSemaphore = xSemaphoreCreateMutex();
    app_state_init();

    /* ── FASE 1: I2C y sensores ── */
    ESP_LOGI(TAG, "[Fase 1] I2C + sensores...");
    if (i2c_master_init() != ESP_OK) ESP_LOGE(TAG, "I2C Bus failed!");
    /* Diagnóstico del bus: lista addresses que ACKean.
     * Esperado: 0x15 (CST816S touch), 0x36 (MAX17048 fuel gauge),
     *           0x40 (MAX30205 temp), 0x57 (MAX30102 HR/SpO2),
     *           0x69 (BMI160 IMU). */
    i2c_scan();
    if (gpio_buttons_init() != ESP_OK) ESP_LOGW(TAG, "gpio_buttons_init falló");

    if (max17048_init() != ESP_OK) ESP_LOGW(TAG, "MAX17048 ausente");

    /* BMI160: NO usar el step counter HW (prohibido por requisito de proyecto).
     * ODR 50 Hz + FIFO para muestreo uniforme del pedómetro FFT+tiempo. */
    if (bmi160_init() != ESP_OK) ESP_LOGE(TAG, "BMI160 init falló");
    else if (bmi160_fifo_enable() != ESP_OK) ESP_LOGE(TAG, "BMI160 FIFO falló");

    if (max30205_init() != ESP_OK) ESP_LOGW(TAG, "MAX30205 ausente");
    if (max30102_init_hrm() != ESP_OK) ESP_LOGW(TAG, "MAX30102 ausente");

    if (ad8232_init_dma() == ESP_OK) {
        ESP_LOGI(TAG, "AD8232 DMA configurado (No iniciado, modo bajo consumo activo)");
    } else {
        ESP_LOGW(TAG, "AD8232 ausente");
    }

    /* Primera lectura sync de sensores → poblar app_state ANTES de que la UI
     * arranque, así el watchface no muestra 0% batería / --°C / --- pasos
     * por 2s hasta que el system_task haga su primera pasada. */
    {
        uint16_t bat_mv = 0;
        float    bat_soc = 0.0f;
        float    bat_crate = 0.0f;
        float    temp_c = 0.0f;
        max17048_get_voltage(&bat_mv);
        max17048_get_soc(&bat_soc);
        max17048_get_crate(&bat_crate);
        max30205_read_temperature(&temp_c);

        shared_sensor_data_t *sd = app_state_lock(50);
        if (sd) {
            sd->battery_mv       = bat_mv;
            sd->battery_soc      = bat_soc;
            sd->battery_crate    = bat_crate;
            sd->battery_charging = (bat_crate > 1.0f);
            sd->bat_updated_ms   = now_ms();
            sd->temperature_c    = temp_c;
            sd->temp_updated_ms  = now_ms();
            app_state_unlock();
        }
        ESP_LOGI(TAG, "Primera lectura: bat=%d%% mv=%u crate=%.2f%%/h temp=%.1fC",
                 (int)bat_soc, (unsigned)bat_mv, (double)bat_crate, (double)temp_c);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    /* ── FASE 2: Display + UI ── */
    ESP_LOGI(TAG, "[Fase 2] Display + UI...");
    gc9a01_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    gc9a01_fill_screen(0x0000);
    /* Touch capacitivo (CST816S) sobre el mismo bus I2C compartido. Si el
     * chip no responde, la UI sigue funcional con botones. */
    esp_err_t terr = cst816s_init(SUPA_PIN_TOUCH_INT, SUPA_PIN_TOUCH_RST);
    if (terr != ESP_OK) {
        ESP_LOGW(TAG, "cst816s_init: %s (touch deshabilitado)", esp_err_to_name(terr));
    }
    ui_init();
    ui_set_actions(&s_ui_actions);
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* ── FASE 3: BLE ── */
    ESP_LOGI(TAG, "[Fase 3] BLE...");
    if (ble_telemetry_init() != ESP_OK) ESP_LOGE(TAG, "BLE Stack falló");
    /* SYNC_TIME (opcode 0x02): la app manda unix ts en segundos y lo
     * volcamos al reloj del sistema. La UI usa esp_timer_get_time() para el
     * clock por ahora, pero settimeofday deja el time_t global listo para
     * cuando la home screen migre a localtime_r. */
    ble_telemetry_set_time_sync_cb(on_ble_sync_time);

    /* ── FASE 4: HAR (TinyML, core 1) ──
     * Detección de actividad: reposo/caminar/correr (escaleras pendiente dataset).
     * La har_task se crea pinned a core 1 (prio 4) como consumidor de inferencia;
     * NO lee el sensor: imu_task lo alimenta vía har_cnn1d_push_sample() con el
     * mismo stream de 50 Hz del FIFO → un solo lector del BMI160, sin contención
     * I2C ni FIFO overflow. Va después del BLE porque on_har_result() empuja 0x08. */
    ESP_LOGI(TAG, "[Fase 4] HAR (TinyML)...");
    if (har_cnn1d_init(on_har_result, NULL) != ESP_OK) {
        ESP_LOGW(TAG, "HAR init falló (sigue sin clasificación de actividad)");
    }

    /* Tasks */
    xTaskCreate(gui_task,    "gui_task",    4096, NULL, 5, NULL);
    xTaskCreate(imu_task,    "imu_task",    6144, NULL, 6, NULL);  /* +1KB: FFT del pedómetro usa ~1.8 KB en stack */
    xTaskCreate(hrm_task,    "hrm_task",    4096, NULL, 5, NULL);
    xTaskCreate(system_task, "system_task", 4096, NULL, 3, NULL);
    xTaskCreate(ble_tx_task, "ble_tx_task", 4096, NULL, 4, NULL);  /* +1024: el HWM medido era 960 B */
    xTaskCreate(ecg_task,    "ecg_task",    4096, NULL, 7, NULL);
    xTaskCreate(perf_monitor_task, "perf_task", 6144, NULL, 2, NULL); /* vTaskList+RunTimeStats consumen ~3.8 KB */

    ESP_LOGI(TAG, "=== SISTEMA INICIADO ===");
}


import re

with open('src/tests/test_general.c', 'r') as f:
    lines = f.readlines()

out = []

out.append("""#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "lvgl.h"
#include "sdl/sdl.h"

#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)

#define BLE_TLV_TYPE_MODE_EVT 1
static uint32_t esp_timer_get_time() { return SDL_GetTicks() * 1000; }

typedef enum { SPOT_QUALITY_GOOD, SPOT_QUALITY_FAIR, SPOT_QUALITY_POOR } max30102_spot_quality_t;
typedef enum { SPOT_STATE_IDLE, SPOT_STATE_SETTLING, SPOT_STATE_MEASURING, SPOT_STATE_DONE, SPOT_STATE_FAILED, SPOT_STATE_ABORTED } max30102_spot_state_t;
typedef struct { max30102_spot_state_t state; uint8_t progress_pct; uint32_t duration_ms; uint8_t bpm; uint8_t spo2; max30102_spot_quality_t quality; } max30102_spot_status_t;
typedef enum { POWER_MODE_SPORT = 0, POWER_MODE_NORMAL, POWER_MODE_SAVER } power_mode_t;
typedef enum { BTN_EVENT_NONE, BTN_EVENT_NEXT_SHORT, BTN_EVENT_NEXT_LONG, BTN_EVENT_SELECT_SHORT, BTN_EVENT_SELECT_LONG } btn_event_t;

static max30102_spot_status_t mock_spot = {SPOT_STATE_IDLE, 0, 0, 0, 0, SPOT_QUALITY_GOOD};
void max30102_spot_get_status(max30102_spot_status_t *st) { *st = mock_spot; }
void max30102_spot_abort() { mock_spot.state = SPOT_STATE_IDLE; }
void max30102_spot_start() { mock_spot.state = SPOT_STATE_MEASURING; mock_spot.progress_pct = 0; }
power_mode_t power_get_mode() { return POWER_MODE_SPORT; }
void power_set_mode(power_mode_t m) {}
const char* power_mode_name(power_mode_t m) { return "SPORT"; }
uint16_t power_get_display_off_s(power_mode_t m) { return 15; }
void power_set_display_off_s(power_mode_t m, uint16_t s) {}
static bool ecg_active = false;
bool ble_telemetry_is_ecg_mode_active() { return ecg_active; }
void ble_telemetry_set_ecg_mode(bool act) { ecg_active = act; }
void ble_tx_push(int t, void* d, int l, int m) {}
void max17048_reset() {}
""")

start_idx = 0
for i, line in enumerate(lines):
    if "static const char *TAG" in line:
        out.append(line)
        start_idx = i + 1
        break

# skip mutexes until shared_sensor_data_t
for i in range(start_idx, len(lines)):
    if "typedef struct {" in lines[i] and "ax, ay, az;" in lines[i+1]:
        start_idx = i
        break

end_idx = 0
for i in range(start_idx, len(lines)):
    if "void gui_task(" in lines[i]:
        end_idx = i
        break

# Clean up code slightly (remove FreeRTOS semaphore locks)
for i in range(start_idx, end_idx):
    line = lines[i]
    if "xSemaphoreTake" in line or "xSemaphoreGive" in line:
        continue
    # remove esp_deep_sleep stuff
    if "esp_deep_sleep_enable_gpio_wakeup" in line or "esp_deep_sleep_start" in line:
        continue
    if "vTaskDelay" in line:
        continue
    # we don't have st7789 natively, remove it
    if "st7789_draw_bitmap" in line:
        line = "    // st7789_draw_bitmap(x, y, w, h, (const uint16_t *)color_p);\n"
    out.append(line)

# Now add the SDL main loop
out.append("""
int main(void) {
    lv_init();
    sdl_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[DISP_BUF_SIZE];
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 280;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);

    build_ui();
    
    // Simulate some sensor data
    sensor_data.steps_sw = 1234;
    sensor_data.battery_soc = 85.0f;
    sensor_data.finger_present = true;
    sensor_data.hr_bpm = 72;
    sensor_data.spo2_pct = 98;
    sensor_data.temperature_c = 36.5f;

    while(1) {
        update_home_screen(&sensor_data);
        update_bio_screen(&sensor_data);
        update_ecg_screen();
        update_hrspot_screen();

        lv_task_handler();
        SDL_Delay(5);
    }
    return 0;
}
""")

with open('pc_simulator/src/main.cpp', 'w') as f:
    f.writelines(out)
print("GUI extracted to pc_simulator/src/main.cpp")

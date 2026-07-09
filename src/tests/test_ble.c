#ifdef ENV_TEST_BLE

/*
 * test_ble — smoke test aislado del stack BLE del SupaClock.
 *
 * Objetivo: probar Just Works pairing + sync_time SIN encender ningún
 * sensor, display o driver. Sirve para comparar dos placas iguales
 * (XIAO ESP32-S3) y aislar si un problema es del BLE o de otra soldadura
 * del carrier.
 *
 * Qué hace:
 *   1. Inicializa NVS (necesario para persistir bonds).
 *   2. Llama a ble_telemetry_init() — arranca NimBLE, GATT, adv y bond.
 *   3. Registra el callback SYNC_TIME (opcode 0x02) → settimeofday.
 *   4. Loop infinito: cada 5 s imprime el reloj de pared y si hay bond.
 *
 * Cómo probarlo:
 *   pio run -e test_ble -t upload
 *   pio device monitor -e test_ble
 *   En Android: emparejá "SupaClock_BLE" (icono reloj). Debe salir prompt
 *   Just Works. Después, desde la app (Debug BLE → Sync time), mandá el
 *   comando 0x02 y observá el log: RTC sincronizado → hora sale del uptime
 *   ficticio hacia la hora real.
 */

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "nvs_flash.h"

#include "ble_telemetry.h"

static const char *TAG = "test_ble";

static void on_sync_time(uint32_t unix_ts) {
    struct timeval tv = { .tv_sec = (time_t)unix_ts, .tv_usec = 0 };
    if (settimeofday(&tv, NULL) != 0) {
        ESP_LOGW(TAG, "settimeofday fallo (ts=%u)", (unsigned)unix_ts);
        return;
    }
    ESP_LOGI(TAG, "RTC sincronizado por BLE. ts=%u", (unsigned)unix_ts);
}

static void log_status_task(void *arg) {
    while (1) {
        time_t now = time(NULL);
        struct tm tm_utc;
        gmtime_r(&now, &tm_utc);
        ESP_LOGI(TAG, "[%04d-%02d-%02d %02d:%02d:%02d UTC] paired=%d",
                 tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
                 tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec,
                 ble_telemetry_is_paired() ? 1 : 0);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== SupaClock BLE-only smoke test ===");

    /* Bajamos verbosidad del stack NimBLE (misma línea que usa el main_app). */
    esp_log_level_set("NimBLE",     ESP_LOG_WARN);
    esp_log_level_set("NimBLE_GAP", ESP_LOG_WARN);
    esp_log_level_set("BTDM_INIT",  ESP_LOG_WARN);
    esp_log_level_set("phy_init",   ESP_LOG_WARN);

    /* NVS: obligatorio para que ble_store_config_init persista bonds. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrupto, borrando y reintentando");
        nvs_flash_erase();
        nvs_flash_init();
    }

    if (ble_telemetry_init() != ESP_OK) {
        ESP_LOGE(TAG, "ble_telemetry_init falló — HW BLE roto?");
        return;
    }

    /* Fuerza el BT controller a NO dormir (modem sleep off) para descartar
     * timing/wake-up del XTAL como causa de connect timeouts. Sube el
     * consumo — solo para diagnóstico. */
    esp_err_t sd = esp_bt_sleep_disable();
    if (sd == ESP_OK) {
        ESP_LOGI(TAG, "BT modem sleep DESACTIVADO (radio always-on)");
    } else {
        ESP_LOGW(TAG, "esp_bt_sleep_disable rc=%d", sd);
    }

    ble_telemetry_set_time_sync_cb(on_sync_time);

    ESP_LOGI(TAG, "BLE listo. Buscá 'SupaClock_BLE' en Android y emparejá.");
    ESP_LOGI(TAG, "Bond persistido en NVS: %d", ble_telemetry_is_paired() ? 1 : 0);

    xTaskCreate(log_status_task, "status", 4096, NULL, 3, NULL);
}

#endif /* ENV_TEST_BLE */

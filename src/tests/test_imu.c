#ifdef ENV_TEST_IMU

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "i2c_bus.h"
#include "bmi160.h"
#include "ble_telemetry.h"

static const char *TAG = "Test_IMU_BLE";

void imu_task(void *pvParameters) {
  int16_t imu_raw[6] = {0};
  TickType_t xLastWakeTime = xTaskGetTickCount();
  uint32_t last_log_ms = 0;
  uint32_t samples_since_log = 0;

  while (1) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));

    esp_err_t err = bmi160_read_accel_gyro(&imu_raw[0], &imu_raw[1], &imu_raw[2],
                                            &imu_raw[3], &imu_raw[4], &imu_raw[5]);
    if (err == ESP_OK) {
      ble_telemetry_send_imu(imu_raw, sizeof(imu_raw));
      samples_since_log++;
    }

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    if (now - last_log_ms >= 2000) {
      ESP_LOGI(TAG, "IMU @100Hz: ax=%d ay=%d az=%d  gx=%d gy=%d gz=%d  (%lu samples/2s)",
               imu_raw[0], imu_raw[1], imu_raw[2],
               imu_raw[3], imu_raw[4], imu_raw[5],
               (unsigned long)samples_since_log);
      samples_since_log = 0;
      last_log_ms = now;
    }
  }
}

void app_main(void) {
  vTaskDelay(pdMS_TO_TICKS(2000));
  ESP_LOGI(TAG, "=== test_imu: BMI160 + BLE streaming para captura ML ===");

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  esp_log_level_set("NimBLE",     ESP_LOG_WARN);
  esp_log_level_set("NimBLE_GAP", ESP_LOG_WARN);
  esp_log_level_set("BLE_GAP",    ESP_LOG_WARN);
  esp_log_level_set("BLE_GATT",   ESP_LOG_WARN);
  esp_log_level_set("BTDM_INIT",  ESP_LOG_WARN);
  esp_log_level_set("phy_init",   ESP_LOG_WARN);

  if (i2c_master_init() != ESP_OK) {
    ESP_LOGE(TAG, "I2C init fallo - abortando");
    return;
  }
  if (bmi160_init() != ESP_OK) {
    ESP_LOGE(TAG, "BMI160 init fallo (revisa SDO/SA0=GND para 0x68, cables, 3V3)");
    return;
  }
  ESP_LOGI(TAG, "BMI160 OK: accel 100 Hz +/- 2 g, gyro 100 Hz +/- 2000 dps");

  if (ble_telemetry_init() != ESP_OK) {
    ESP_LOGE(TAG, "BLE stack init fallo");
    return;
  }
  ESP_LOGI(TAG, "BLE advertising 'SupaClock_BLE' - conecta desde la app Flutter");

  xTaskCreate(imu_task, "imu_task", 4096, NULL, 6, NULL);
}

#endif

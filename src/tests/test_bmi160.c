#ifdef ENV_TEST_IMU

#include "bmi160.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "driver/i2c.h"
#include "step_algorithm.h"
#include <stdio.h>

static const char *TAG = "Test_BMI160_Steps";

void bmi160_task(void *pvParameters) {
  ESP_LOGI(TAG, "Inicializando sensor BMI160...");
  esp_err_t err = bmi160_init();
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "FALLO INICIALIZAR BMI160! Check cables o Pin SA0. Error: %s", esp_err_to_name(err));
  } else {
      ESP_LOGI(TAG, "BMI160 I2C Reconocido OK.");
  }
  vTaskDelay(pdMS_TO_TICKS(100));

  // Habilitar podómetro de hardware
  if (bmi160_enable_step_counter() == ESP_OK) {
    ESP_LOGI(TAG, "Podómetro de hardware BMI160 HABILITADO.");
  } else {
    ESP_LOGE(TAG, "Error al habilitar podómetro de HW.");
  }

  // Inicializar algoritmo de software
  step_algo_state_t sw_pedometer;
  step_algo_init(&sw_pedometer);
  uint32_t sw_step_count = 0;

  int16_t ax = -999, ay = -999, az = -999;
  int16_t gx = -999, gy = -999, gz = -999;
  uint32_t last_print_time = 0;

  while (1) {
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

    esp_err_t sample_err = bmi160_read_accel_gyro(&ax, &ay, &az, &gx, &gy, &gz);
    if (sample_err == ESP_OK) {
      // Ejecutar algoritmo de software
      sw_step_count += step_algo_update(&sw_pedometer, ax, ay, az, gx, gy, gz, now_ms);
    }

    // Imprimir comparación cada 1 segundo
    if ((now_ms - last_print_time) >= 1000) {
      uint16_t hw_step_count = 0;
      bmi160_read_step_counter(&hw_step_count);

      ESP_LOGI(TAG, "PASOS -> HW BMI160: %u  |  SW ESP32: %lu", hw_step_count,
               (unsigned long)sw_step_count);
      ESP_LOGI(TAG, "Ax: %d, Ay: %d, Az: %d", ax, ay, az);

      last_print_time = now_ms;
    }

    // 50Hz de tasa de muestreo (20ms)
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void app_main(void) {
  // Retraso masivo para dar tiempo a abrir la ventana del monitor serial
  vTaskDelay(pdMS_TO_TICKS(4000));
  
  ESP_LOGI(TAG, "=== Sistema Doble Podómetro ===");
  if (i2c_master_init() == ESP_OK) {
    ESP_LOGI(TAG, "I2C Inicializado. Escaneando bus I2C...");
    
    // Forzamos un dummy read a la address 0x68 antes de hacer nada para "despertar" el I2C del BOSCH
    uint8_t dummy;
    i2c_read_bytes(BMI160_I2C_ADDR, 0x7F, &dummy, 1);
    vTaskDelay(pdMS_TO_TICKS(2));

    // I2C SCANNER
    int devices_found = 0;
    for(uint8_t i = 1; i < 127; i++) {
        esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, i, NULL, 0, 100 / portTICK_PERIOD_MS);
        if(ret == ESP_OK) {
            ESP_LOGW(TAG, ">>> ¡DISPOSITIVO I2C ENCONTRADO EN LA DIRECCION: 0x%02X ! <<<", i);
            devices_found++;
        }
    }
    if (devices_found == 0) {
        ESP_LOGE(TAG, ">>> BUS MUERTO: No se encontró ningún dispositivo. Revisa cables, VCC a 3.3V, o Pull-Ups. <<<");
    }

    xTaskCreate(bmi160_task, "BmiTask", 4096, NULL, 5, NULL);
  } else {
    ESP_LOGE(TAG, "Fallo inicializando I2C.");
  }
}
#endif

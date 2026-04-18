#include "i2c_bus.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "I2C_BUS";

/* Mutex global para serializar acceso al bus I2C */
SemaphoreHandle_t i2c_bus_mutex = NULL;
// Esta parte se encarga de inicializar el bus I2C y proporcionar funciones para
// leer y escribir bytes a dispositivos conectados al bus. Utiliza la API de I2C
// proporcionada por ESP-IDF para configurar el bus, crear comandos, y manejar
// errores de manera adecuada.
esp_err_t i2c_master_init(void) {
  int i2c_master_port = I2C_MASTER_NUM;

  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_MASTER_FREQ_HZ,
  };
  i2c_param_config(i2c_master_port, &conf);
  i2c_set_timeout(i2c_master_port, 0xFFFFF);

  esp_err_t err =
      i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE,
                         I2C_MASTER_TX_BUF_DISABLE, 0);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "I2C initialized successfully on SDA:%d SCL:%d at %d Hz",
             I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);
  } else {
    ESP_LOGE(TAG, "Failed to initialize I2C: %s", esp_err_to_name(err));
    return err;
  }

  /* Crear el mutex para acceso exclusivo al bus */
  i2c_bus_mutex = xSemaphoreCreateMutex();
  if (i2c_bus_mutex == NULL) {
    ESP_LOGE(TAG, "Failed to create I2C bus mutex");
    return ESP_FAIL;
  }

  return err;
}

esp_err_t i2c_write_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data,
                          size_t len) {
  if (i2c_bus_mutex != NULL) xSemaphoreTake(i2c_bus_mutex, portMAX_DELAY);

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, reg_addr, true);
  if (len > 0 && data != NULL) {
    i2c_master_write(cmd, data, len, true);
  }
  i2c_master_stop(cmd);
  esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd,
                                       pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
  i2c_cmd_link_delete(cmd);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C Write Failed - Dev Addr: 0x%02x, Reg: 0x%02x, Err: %s",
             dev_addr, reg_addr, esp_err_to_name(err));
  }

  if (i2c_bus_mutex != NULL) xSemaphoreGive(i2c_bus_mutex);
  return err;
}

/*
 * Este código implementa la función `i2c_read_bytes` que lee una cantidad
 * específica de bytes desde un dispositivo I2C. La función toma la dirección
 * del dispositivo, la dirección del registro, un puntero a un buffer donde se
 * almacenarán los datos leídos, y la longitud de los datos a leer. Utiliza
 * comandos I2C para iniciar la comunicación, escribir la dirección del
 * registro, y luego leer los datos, manejando adecuadamente los ACK/NACK según
 * la cantidad de bytes a leer. Finalmente, maneja los errores y libera los
 * recursos utilizados para la comunicación.
 */
esp_err_t i2c_read_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data,
                         size_t len) {
  if (len == 0 || data == NULL)
    return ESP_OK;

  if (i2c_bus_mutex != NULL) xSemaphoreTake(i2c_bus_mutex, portMAX_DELAY);

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, reg_addr, true);

  i2c_master_start(cmd); // Repeated start
  i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);

  if (len > 1) {
    i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
  }
  i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);

  i2c_master_stop(cmd);
  esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd,
                                       pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
  i2c_cmd_link_delete(cmd);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C Read Failed - Dev Addr: 0x%02x, Reg: 0x%02x, Err: %s",
             dev_addr, reg_addr, esp_err_to_name(err));
  }

  if (i2c_bus_mutex != NULL) xSemaphoreGive(i2c_bus_mutex);
  return err;
}

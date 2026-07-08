#include "gc9a01.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "GC9A01_Driver";
static spi_device_handle_t spi;

/* Tracker on/off del panel + backlight, mismo diseño que el ST7789:
 * la primera vez que set_brightness recibe > 0 despertamos el panel
 * y bloqueamos light sleep; cuando baja a 0 dormimos el panel y
 * soltamos el lock. */
static bool s_blk_on = false;

#if CONFIG_PM_ENABLE
static esp_pm_lock_handle_t s_blk_pm_lock = NULL;
#endif

static void lcd_cmd(const uint8_t cmd) {
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = 8;
  t.tx_buffer = &cmd;
  gpio_set_level(GC9A01_DC_PIN, 0);
  ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
}

static void lcd_data(const uint8_t *data, int len) {
  if (len == 0) return;
  spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = len * 8;
  t.tx_buffer = data;
  gpio_set_level(GC9A01_DC_PIN, 1);
  ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
}

/* Helper para las secuencias vendor: comando + N bytes de payload en línea */
static void lcd_cmd_data(uint8_t cmd, const uint8_t *data, int len) {
  lcd_cmd(cmd);
  if (len > 0) lcd_data(data, len);
}

static void set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  /* GC9A01: RAM 240x240 exacta, sin offset como el ST7789 (240x320). */
  uint8_t caset_data[4] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
  lcd_cmd_data(GC9A01_CASET, caset_data, 4);

  uint8_t raset_data[4] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};
  lcd_cmd_data(GC9A01_RASET, raset_data, 4);

  lcd_cmd(GC9A01_RAMWR);
}

/* Secuencia de inicialización vendor GC9A01 (Waveshare / Galaxycore).
 * Unlock de inner registers (0xFE, 0xEF), power/gamma, TE, INV, sleep out,
 * display on. Sin esta cadena el panel enciende con noise o colores
 * invertidos; con ella queda limpio a 240x240 16bpp. */
static void gc9a01_send_init_sequence(void) {
  static const uint8_t d_EB[]  = {0x14};
  static const uint8_t d_84[]  = {0x40};
  static const uint8_t d_85[]  = {0xFF};
  static const uint8_t d_86[]  = {0xFF};
  static const uint8_t d_87[]  = {0xFF};
  static const uint8_t d_88[]  = {0x0A};
  static const uint8_t d_89[]  = {0x21};
  static const uint8_t d_8A[]  = {0x00};
  static const uint8_t d_8B[]  = {0x80};
  static const uint8_t d_8C[]  = {0x01};
  static const uint8_t d_8D[]  = {0x01};
  static const uint8_t d_8E[]  = {0xFF};
  static const uint8_t d_8F[]  = {0xFF};
  static const uint8_t d_B6[]  = {0x00, 0x00};
  /* MADCTL: MX | BGR = 0x48. El Waveshare 1.28" con esta cadena vendor sale
   * horizontalmente espejado si no se setea el bit MX (columnas de derecha a
   * izquierda). Con 0x48 las X coinciden con lo que dibuja LVGL. */
  static const uint8_t d_36[]  = {0x48};
  static const uint8_t d_3A[]  = {0x05};              /* COLMOD — 16bpp RGB565       */
  static const uint8_t d_90[]  = {0x08, 0x08, 0x08, 0x08};
  static const uint8_t d_BD[]  = {0x06};
  static const uint8_t d_BC[]  = {0x00};
  static const uint8_t d_FF[]  = {0x60, 0x01, 0x04};
  static const uint8_t d_C3[]  = {0x13};              /* Power Control */
  static const uint8_t d_C4[]  = {0x13};
  static const uint8_t d_C9[]  = {0x22};
  static const uint8_t d_BE[]  = {0x11};
  static const uint8_t d_E1[]  = {0x10, 0x0E};
  static const uint8_t d_DF[]  = {0x21, 0x0C, 0x02};
  /* Gamma — dos pares de curvas positive/negative                   */
  static const uint8_t d_F0[]  = {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A};
  static const uint8_t d_F1[]  = {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F};
  static const uint8_t d_F2[]  = {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A};
  static const uint8_t d_F3[]  = {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F};
  static const uint8_t d_ED[]  = {0x1B, 0x0B};
  static const uint8_t d_AE[]  = {0x77};
  static const uint8_t d_CD[]  = {0x63};
  static const uint8_t d_70[]  = {0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03};
  static const uint8_t d_E8[]  = {0x34};              /* Frame Rate */
  static const uint8_t d_62[]  = {0x18, 0x0D, 0x71, 0xED, 0x70, 0x70,
                                  0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70};
  static const uint8_t d_63[]  = {0x18, 0x11, 0x71, 0xF1, 0x70, 0x70,
                                  0x18, 0x13, 0x71, 0xF3, 0x70, 0x70};
  static const uint8_t d_64[]  = {0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07};
  static const uint8_t d_66[]  = {0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45,
                                  0x10, 0x00, 0x00, 0x00};
  static const uint8_t d_67[]  = {0x00, 0x3C, 0x00, 0x00, 0x00, 0x01,
                                  0x54, 0x10, 0x32, 0x98};
  static const uint8_t d_74[]  = {0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00};
  static const uint8_t d_98[]  = {0x3E, 0x07};

  lcd_cmd(0xFE);                                       /* Inner-register unlock */
  lcd_cmd(0xEF);
  lcd_cmd_data(0xEB, d_EB, sizeof(d_EB));
  lcd_cmd_data(0x84, d_84, sizeof(d_84));
  lcd_cmd_data(0x85, d_85, sizeof(d_85));
  lcd_cmd_data(0x86, d_86, sizeof(d_86));
  lcd_cmd_data(0x87, d_87, sizeof(d_87));
  lcd_cmd_data(0x88, d_88, sizeof(d_88));
  lcd_cmd_data(0x89, d_89, sizeof(d_89));
  lcd_cmd_data(0x8A, d_8A, sizeof(d_8A));
  lcd_cmd_data(0x8B, d_8B, sizeof(d_8B));
  lcd_cmd_data(0x8C, d_8C, sizeof(d_8C));
  lcd_cmd_data(0x8D, d_8D, sizeof(d_8D));
  lcd_cmd_data(0x8E, d_8E, sizeof(d_8E));
  lcd_cmd_data(0x8F, d_8F, sizeof(d_8F));
  lcd_cmd_data(0xB6, d_B6, sizeof(d_B6));
  lcd_cmd_data(GC9A01_MADCTL, d_36, sizeof(d_36));
  lcd_cmd_data(GC9A01_COLMOD, d_3A, sizeof(d_3A));
  lcd_cmd_data(0x90, d_90, sizeof(d_90));
  lcd_cmd_data(0xBD, d_BD, sizeof(d_BD));
  lcd_cmd_data(0xBC, d_BC, sizeof(d_BC));
  lcd_cmd_data(0xFF, d_FF, sizeof(d_FF));
  lcd_cmd_data(0xC3, d_C3, sizeof(d_C3));
  lcd_cmd_data(0xC4, d_C4, sizeof(d_C4));
  lcd_cmd_data(0xC9, d_C9, sizeof(d_C9));
  lcd_cmd_data(0xBE, d_BE, sizeof(d_BE));
  lcd_cmd_data(0xE1, d_E1, sizeof(d_E1));
  lcd_cmd_data(0xDF, d_DF, sizeof(d_DF));
  lcd_cmd_data(0xF0, d_F0, sizeof(d_F0));
  lcd_cmd_data(0xF1, d_F1, sizeof(d_F1));
  lcd_cmd_data(0xF2, d_F2, sizeof(d_F2));
  lcd_cmd_data(0xF3, d_F3, sizeof(d_F3));
  lcd_cmd_data(0xED, d_ED, sizeof(d_ED));
  lcd_cmd_data(0xAE, d_AE, sizeof(d_AE));
  lcd_cmd_data(0xCD, d_CD, sizeof(d_CD));
  lcd_cmd_data(0x70, d_70, sizeof(d_70));
  lcd_cmd_data(0xE8, d_E8, sizeof(d_E8));
  lcd_cmd_data(0x62, d_62, sizeof(d_62));
  lcd_cmd_data(0x63, d_63, sizeof(d_63));
  lcd_cmd_data(0x64, d_64, sizeof(d_64));
  lcd_cmd_data(0x66, d_66, sizeof(d_66));
  lcd_cmd_data(0x67, d_67, sizeof(d_67));
  lcd_cmd_data(0x74, d_74, sizeof(d_74));
  lcd_cmd_data(0x98, d_98, sizeof(d_98));

  lcd_cmd(GC9A01_TEON);
  lcd_cmd(GC9A01_INVON);
  lcd_cmd(GC9A01_SLPOUT);
  vTaskDelay(pdMS_TO_TICKS(120));                      /* SLPOUT settling */
  lcd_cmd(GC9A01_DISPON);
  vTaskDelay(pdMS_TO_TICKS(20));
}

esp_err_t gc9a01_init(void) {
  ESP_LOGI(TAG, "Inicializando pines y SPI para GC9A01");

  uint64_t pin_mask = (1ULL << GC9A01_DC_PIN);
#if GC9A01_RST_PIN >= 0
  pin_mask |= (1ULL << GC9A01_RST_PIN);
#endif
  gpio_config_t gc = {.pin_bit_mask = pin_mask,
                      .mode = GPIO_MODE_OUTPUT,
                      .pull_up_en = GPIO_PULLUP_DISABLE,
                      .pull_down_en = GPIO_PULLDOWN_DISABLE,
                      .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&gc);

  /* Backlight PWM sobre RC_FAST + pm_lock para tolerar light sleep
   * (mismo esquema comprobado en el driver ST7789). */
  ledc_timer_config_t ledc_timer = {
      .speed_mode      = LEDC_LOW_SPEED_MODE,
      .timer_num       = LEDC_TIMER_0,
      .duty_resolution = LEDC_TIMER_8_BIT,
      .freq_hz         = 500,
      .clk_cfg         = LEDC_USE_RC_FAST_CLK
  };
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  ledc_channel_config_t ledc_channel = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel    = LEDC_CHANNEL_0,
      .timer_sel  = LEDC_TIMER_0,
      .intr_type  = LEDC_INTR_DISABLE,
      .gpio_num   = GC9A01_BLK_PIN,
      .duty       = 0,
      .hpoint     = 0
  };
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  spi_bus_config_t buscfg = {.miso_io_num = -1,
                             .mosi_io_num = GC9A01_MOSI_PIN,
                             .sclk_io_num = GC9A01_SCK_PIN,
                             .quadwp_io_num = -1,
                             .quadhd_io_num = -1,
                             .max_transfer_sz = 240 * 240 * 2 + 8};

  /* GC9A01 spec: SCK máx ~60 MHz — 40 MHz es la práctica común y estable.
   * (El ST7789 corría a 80 MHz; el controlador GC9A01 no lo tolera.) */
  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 40 * 1000 * 1000,
      .mode = 0,
      .spics_io_num = GC9A01_CS_PIN,
      .queue_size = 7};

  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
  ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi));

#if GC9A01_RST_PIN >= 0
  ESP_LOGI(TAG, "Hardware reset");
  gpio_set_level(GC9A01_RST_PIN, 0);
  vTaskDelay(pdMS_TO_TICKS(20));
  gpio_set_level(GC9A01_RST_PIN, 1);
  vTaskDelay(pdMS_TO_TICKS(150));
#else
  ESP_LOGI(TAG, "RST no cableado → SWRESET");
  lcd_cmd(GC9A01_SWRESET);
  vTaskDelay(pdMS_TO_TICKS(150));
#endif

  ESP_LOGI(TAG, "Enviando secuencia de init GC9A01");
  gc9a01_send_init_sequence();

#if CONFIG_PM_ENABLE
  if (esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "blk", &s_blk_pm_lock) != ESP_OK) {
    ESP_LOGW(TAG, "BLK PM lock no se pudo crear — el backlight puede parpadear con light sleep");
    s_blk_pm_lock = NULL;
  }
#endif

  ESP_LOGI(TAG, "Driver GC9A01 Init OK");
  return ESP_OK;
}

void gc9a01_send_buffer(const uint8_t *buffer, size_t size) {
  set_addr_window(0, 0, 239, 239);
  size_t chunk_size = 240 * 40 * 2;
  size_t offset = 0;
  while (offset < size) {
    size_t current = (size - offset > chunk_size) ? chunk_size : (size - offset);
    lcd_data(buffer + offset, current);
    offset += current;
  }
}

void gc9a01_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        const uint16_t *bitmap) {
  if (x >= 240 || y >= 240 || w == 0 || h == 0) return;

  uint16_t xE = x + w - 1;
  uint16_t yE = y + h - 1;
  uint16_t draw_w = w, draw_h = h;
  if (xE >= 240) { xE = 239; draw_w = 240 - x; }
  if (yE >= 240) { yE = 239; draw_h = 240 - y; }

  set_addr_window(x, y, xE, yE);

  if (draw_w == w && draw_h == h) {
    lcd_data((const uint8_t *)bitmap, (size_t)w * h * 2);
  } else {
    for (int row = 0; row < draw_h; row++) {
      const uint16_t *row_ptr = bitmap + (row * w);
      lcd_data((const uint8_t *)row_ptr, draw_w * 2);
    }
  }
}

void gc9a01_fill_screen(uint16_t color) {
  ESP_LOGI(TAG, "Fill color: 0x%04X", color);
  set_addr_window(0, 0, 239, 239);

  uint8_t hi = color >> 8, lo = color & 0xFF;
  size_t chunk_pixels = 240 * 40;
  size_t chunk_bytes  = chunk_pixels * 2;

  uint8_t *buffer = malloc(chunk_bytes);
  if (!buffer) {
    ESP_LOGE(TAG, "No hay heap para el buffer de fill");
    return;
  }
  for (size_t i = 0; i < chunk_bytes; i += 2) {
    buffer[i]     = hi;
    buffer[i + 1] = lo;
  }

  int chunks = (240 * 240) / chunk_pixels;
  for (int i = 0; i < chunks; i++) lcd_data(buffer, chunk_bytes);
  free(buffer);
}

void gc9a01_set_brightness(uint8_t percent) {
  if (percent > 100) percent = 100;
  uint32_t duty = (percent * 255) / 100;
  bool want_on = (duty > 0);

  /* off → on: primero PM lock + wake del panel; después el backlight. */
  if (want_on && !s_blk_on) {
#if CONFIG_PM_ENABLE
    if (s_blk_pm_lock) esp_pm_lock_acquire(s_blk_pm_lock);
#endif
    lcd_cmd(GC9A01_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_cmd(GC9A01_DISPON);
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (duty == 0) {
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
  } else {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  }

  /* on → off: apagar panel y soltar el PM lock último. */
  if (!want_on && s_blk_on) {
    lcd_cmd(GC9A01_DISPOFF);
    lcd_cmd(GC9A01_SLPIN);
#if CONFIG_PM_ENABLE
    if (s_blk_pm_lock) esp_pm_lock_release(s_blk_pm_lock);
#endif
  }

  s_blk_on = want_on;
}

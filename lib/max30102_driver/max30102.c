#include "max30102.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include <math.h>
#include <string.h>

static const char *TAG = "MAX30102";

/* ════════════════════════════════════════════════════════════════
 *  Estado interno del algoritmo HR / SpO2
 * ════════════════════════════════════════════════════════════════ */

/* Frecuencia efectiva ≈ 25 Hz (100 sps / sample_avg=4).
 * A 25 Hz, un latido normal (40-200 BPM) dura 7 a 37 muestras. */
#define HRM_FS_HZ 25

/* Ventana AC/DC para cálculo de SpO2 (≈ 2 s) */
#define AC_DC_WIN 50

/* Número de intervalos RR promediados para BPM estable */
#define RR_WIN 4

/* Umbral DC IR para detectar presencia de dedo (calibrado empíricamente).
 * Sin piel/dedo el IR suele < 10_000; con dedo > 50_000. */
#define FINGER_IR_THRESHOLD 50000UL

/* Constantes empíricas para SpO2: SpO2 ≈ A - B * R */
#define SPO2_CALIB_A 110.0f
#define SPO2_CALIB_B 25.0f

/* Límites de DC para el Control Automático de Ganancia (AGC) */
#define AGC_DC_MIN 50000.0f
#define AGC_DC_MAX 200000.0f

/* Refractario mínimo entre picos (muestras).
 * 7 muestras @ 25 Hz = 280 ms → cap fisiológico de ~214 bpm.
 * Evita que picos dicróticos o ruido post-sístole se cuenten como latido. */
#define PEAK_REFRACTORY_SAMPLES 7

/* Umbral adaptativo de pico: fracción de la amplitud AC máxima reciente.
 * running_max_ac decae lentamente para seguir cambios del AGC/LED PA. */
#define PEAK_THRESHOLD_FRAC   0.30f
#define RUNNING_MAX_DECAY     0.995f   /* ~0.5% por muestra → τ≈200 muestras (8 s) */
#define PEAK_THRESHOLD_FLOOR  15.0f    /* piso absoluto para señales débiles */

typedef struct {
  /* DC baseline (EMA α = 1/16) */
  float dc_ir;
  float dc_red;

  /* Señal AC filtrada (IR) — media móvil corta */
  float ac_ir_lp[4];
  uint8_t lp_idx;
  float prev_ac;
  float prev_prev_ac;

  /* Ventana para AC_rms / DC (SpO2) */
  float ac_ir_sq_sum;
  float ac_red_sq_sum;
  float dc_ir_sum;
  float dc_red_sum;
  uint16_t win_count;

  /* Detección de picos + intervalos RR */
  uint32_t samples_since_last_peak;
  uint32_t rr_buf_ms[RR_WIN];
  uint8_t rr_idx;
  uint8_t rr_valid_count;

  /* Umbral adaptativo de pico (seguimiento de amplitud AC máxima) */
  float running_max_ac;

  /* Diagnóstico: overflow del FIFO acumulado desde el último init */
  uint32_t overflow_count;

  /* Estimaciones válidas */
  uint8_t hr_bpm;
  uint8_t spo2_pct;

  /* Control Automático de Ganancia */
  uint8_t current_led_pa;

  /* Presencia de dedo */
  bool finger;
} hrm_state_t;

static hrm_state_t s_hrm = {0};

/* ════════════════════════════════════════════════════════════════
 *  Helpers de escritura / lectura registro
 * ════════════════════════════════════════════════════════════════ */
static esp_err_t wr_reg(uint8_t reg, uint8_t val) {
  return i2c_write_bytes(MAX30102_I2C_ADDR, reg, &val, 1);
}

static esp_err_t rd_reg(uint8_t reg, uint8_t *val) {
  return i2c_read_bytes(MAX30102_I2C_ADDR, reg, val, 1);
}

/* ════════════════════════════════════════════════════════════════
 *  Init
 * ════════════════════════════════════════════════════════════════ */

esp_err_t max30102_init(void) {
  /* Wrapper histórico: simplemente llama al init completo. */
  return max30102_init_hrm();
}

esp_err_t max30102_init_hrm(void) {
  uint8_t part_id = 0;
  esp_err_t err;

  /* ── 1. Verificar PART ID ── */
  err = rd_reg(MAX30102_REG_PART_ID, &part_id);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "No se puede leer PART_ID (err=%s)", esp_err_to_name(err));
    return err;
  }
  if (part_id != MAX30102_PART_ID_VALUE) {
    ESP_LOGE(TAG, "PART_ID inesperado: 0x%02X (esperado 0x%02X)", part_id,
             MAX30102_PART_ID_VALUE);
    return ESP_ERR_NOT_FOUND;
  }

  /* ── 2. Soft reset ── */
  wr_reg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_RESET);
  vTaskDelay(pdMS_TO_TICKS(10));

  /* ── 3. Limpiar punteros FIFO ── */
  wr_reg(MAX30102_REG_FIFO_WR_PTR, 0x00);
  wr_reg(MAX30102_REG_OVF_COUNTER, 0x00);
  wr_reg(MAX30102_REG_FIFO_RD_PTR, 0x00);

  /* ── 4. FIFO_CONFIG ──
   *   SMP_AVE[7:5] = 010 → promedio de 4 muestras
   *   FIFO_ROLLOVER_EN[4] = 1
   *   FIFO_A_FULL[3:0]    = 0xF → IRQ cuando queden 17 muestras
   *   → 0b0101_1111 = 0x5F
   */
  wr_reg(MAX30102_REG_FIFO_CONFIG, 0x5F);

  /* ── 5. SPO2_CONFIG ──
   *   ADC_RGE[6:5] = 01 → 4096 nA full scale
   *   SR[4:2]      = 001 → 100 sps
   *   LED_PW[1:0]  = 11 → 411 us, resolución 18 bits
   *   → 0b0010_0111 = 0x27
   */
  wr_reg(MAX30102_REG_SPO2_CONFIG, 0x27);

  /* ── 6. LEDx_PA (0x24 ≈ 7.2 mA, seguro y suficiente para dedo) ── */
  wr_reg(MAX30102_REG_LED1_PA, 0x24); /* RED */
  wr_reg(MAX30102_REG_LED2_PA, 0x24); /* IR  */

  /* ── 7. MODE_CONFIG = SpO2 (Red + IR) ── */
  wr_reg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2);

  vTaskDelay(pdMS_TO_TICKS(10));

  /* ── 8. Volver a borrar punteros LUEGO de entrar en modo SpO2
   * (según recomienda el datasheet) ── */
  wr_reg(MAX30102_REG_FIFO_WR_PTR, 0x00);
  wr_reg(MAX30102_REG_OVF_COUNTER, 0x00);
  wr_reg(MAX30102_REG_FIFO_RD_PTR, 0x00);

  /* ── Readback de configuración para diagnosticar desvíos ── */
  uint8_t fifo_rb = 0, spo2_rb = 0, mode_rb = 0, led1_rb = 0, led2_rb = 0;
  rd_reg(MAX30102_REG_FIFO_CONFIG, &fifo_rb);
  rd_reg(MAX30102_REG_SPO2_CONFIG, &spo2_rb);
  rd_reg(MAX30102_REG_MODE_CONFIG, &mode_rb);
  rd_reg(MAX30102_REG_LED1_PA,     &led1_rb);
  rd_reg(MAX30102_REG_LED2_PA,     &led2_rb);
  ESP_LOGI(TAG,
           "Regs readback: FIFO=0x%02X(esp 0x5F)  SPO2=0x%02X(esp 0x27)  "
           "MODE=0x%02X(esp 0x03)  LED1=0x%02X LED2=0x%02X",
           fifo_rb, spo2_rb, mode_rb, led1_rb, led2_rb);

  /* ── Estado algoritmo ── */
  memset(&s_hrm, 0, sizeof(s_hrm));
  s_hrm.current_led_pa = 0x24;

  ESP_LOGI(TAG,
           "MAX30102 inicializado en modo SpO2 (HR+SpO2, 25 Hz efectivos)");
  return ESP_OK;
}

/* ════════════════════════════════════════════════════════════════
 *  Lectura FIFO
 * ════════════════════════════════════════════════════════════════ */

esp_err_t max30102_read_samples(max30102_sample_t *out, uint8_t max_samples,
                                uint8_t *n_read) {
  if (!out || !n_read || max_samples == 0)
    return ESP_ERR_INVALID_ARG;
  *n_read = 0;

  uint8_t wr_ptr = 0, rd_ptr = 0, ovf = 0;
  esp_err_t err;
  if ((err = rd_reg(MAX30102_REG_FIFO_WR_PTR, &wr_ptr)) != ESP_OK)
    return err;
  if ((err = rd_reg(MAX30102_REG_OVF_COUNTER, &ovf)) != ESP_OK)
    return err;
  if ((err = rd_reg(MAX30102_REG_FIFO_RD_PTR, &rd_ptr)) != ESP_OK)
    return err;

  uint8_t pre_wr = wr_ptr, pre_rd = rd_ptr, pre_ovf = ovf;

  /* En algunos clones del MAX30102, el registro OVF_COUNTER (0x05) no actúa
   * como contador de overflow real, sino que reporta la cantidad de muestras
   * pendientes por leer (unread samples). Dado esto, no podemos usar ovf > 0
   * como síntoma de overflow de FIFO. 
   * Asumimos overflow sólo si ovf llega a 31 (su valor máximo). */
  bool actually_overflowed = (ovf == 0x1F);
  
  if (actually_overflowed) {
    ESP_LOGE(TAG, "MAX_FIFO OVF! wr=%d rd=%d ovf=%d", wr_ptr, rd_ptr, ovf);
    s_hrm.overflow_count++;
    s_hrm.rr_valid_count = 0;
    s_hrm.samples_since_last_peak = 0;
    s_hrm.win_count = 0;
    s_hrm.ac_ir_sq_sum = s_hrm.ac_red_sq_sum = 0;
    s_hrm.dc_ir_sum = s_hrm.dc_red_sum = 0;
    wr_reg(MAX30102_REG_OVF_COUNTER, 0); // Restore manual clear just in case!
  }

  /* Cálculo de muestras pendientes en un FIFO circular de 32 entradas. */
  int pending;
  if (wr_ptr == rd_ptr) {
    // Si los punteros son iguales, el OVF original decía lleno si ovf > 0.
    // Con este clon, si wr == rd, el unread count también sería 0 o 32.
    // OVF_COUNTER in Maxim MAX30102 counts dropped samples. In clones it MIGHT BE the unread sample count or true overflow.
    // Si ovf == 31 lo asume overflow pero si no, y wr==rd.. asumiremos vacio
    pending = actually_overflowed ? 32 : 0;
  } else {
    pending = (int)wr_ptr - (int)rd_ptr;
    if (pending < 0) pending += 32;
  }
  
  // Clon OVF behaviour workaround: sometimes ovf is the unread sample count. We can trust ovf if it's less than 32 AND matches pointer math.
  // Actually, let's just stick to pointer math if there wasn't a confirmed full overflow.
  
  if (pending == 0)
    return ESP_OK;
  if (pending > max_samples)
    pending = max_samples;

  /* Cada muestra = 6 bytes (3 Red + 3 IR). Lectura en burst del FIFO_DATA. */
  uint8_t buf[32 * 6];
  int total_bytes = pending * 6;
  err = i2c_read_bytes(MAX30102_I2C_ADDR, MAX30102_REG_FIFO_DATA, buf,
                       total_bytes);
  if (err != ESP_OK)
    return err;

  // Actualizar fake overflow logger to suppress unread-count-reporting "clones".
  // Remove earlier log parsing.

  for (int i = 0; i < pending; i++) {
    int o = i * 6;
    uint32_t red = ((uint32_t)buf[o + 0] << 16) | ((uint32_t)buf[o + 1] << 8) |
                   (uint32_t)buf[o + 2];
    red &= 0x03FFFF;
    uint32_t ir = ((uint32_t)buf[o + 3] << 16) | ((uint32_t)buf[o + 4] << 8) |
                  (uint32_t)buf[o + 5];
    ir &= 0x03FFFF;
    out[i].red = red;
    out[i].ir = ir;
  }
  *n_read = (uint8_t)pending;
  return ESP_OK;
}

esp_err_t max30102_read_fifo(uint32_t *red, uint32_t *ir) {
  max30102_sample_t s;
  uint8_t n = 0;
  esp_err_t err = max30102_read_samples(&s, 1, &n);
  if (err != ESP_OK)
    return err;
  if (n == 0) {
    if (red)
      *red = 0;
    if (ir)
      *ir = 0;
    return ESP_OK;
  }
  if (red)
    *red = s.red;
  if (ir)
    *ir = s.ir;
  return ESP_OK;
}

/* ════════════════════════════════════════════════════════════════
 *  Algoritmo HR + SpO2
 * ════════════════════════════════════════════════════════════════ */

static void push_rr(uint32_t rr_ms) {
  s_hrm.rr_buf_ms[s_hrm.rr_idx] = rr_ms;
  s_hrm.rr_idx = (s_hrm.rr_idx + 1) % RR_WIN;
  if (s_hrm.rr_valid_count < RR_WIN)
    s_hrm.rr_valid_count++;

  /* Promedio simple → BPM */
  uint32_t sum = 0;
  for (uint8_t i = 0; i < s_hrm.rr_valid_count; i++)
    sum += s_hrm.rr_buf_ms[i];
  uint32_t avg_ms = sum / s_hrm.rr_valid_count;
  if (avg_ms > 0) {
    uint32_t bpm = 60000U / avg_ms;
    if (bpm >= 40 && bpm <= 200)
      s_hrm.hr_bpm = (uint8_t)bpm;
  }
}

void max30102_process_sample(uint32_t red, uint32_t ir) {
  /* ── Detección dedo ── */
  s_hrm.finger = (ir > FINGER_IR_THRESHOLD);
  if (!s_hrm.finger) {
    s_hrm.hr_bpm = 0;
    s_hrm.spo2_pct = 0;
    s_hrm.rr_valid_count = 0;
    s_hrm.samples_since_last_peak = 0;
    s_hrm.win_count = 0;
    s_hrm.ac_ir_sq_sum = s_hrm.ac_red_sq_sum = 0;
    s_hrm.dc_ir_sum = s_hrm.dc_red_sum = 0;
    s_hrm.running_max_ac = 0.0f;
    s_hrm.dc_ir = ir;
    s_hrm.dc_red = red;
    return;
  }

  /* ── Remoción de DC (High-Pass) y Promedio EMA ── */
  if (s_hrm.dc_ir == 0) {
    s_hrm.dc_ir = ir;
    s_hrm.dc_red = red;
  } else {
    // Rastreador lento de DC (alpha paramétrico)
    s_hrm.dc_ir = s_hrm.dc_ir + 0.05f * ((float)ir - s_hrm.dc_ir);
    s_hrm.dc_red = s_hrm.dc_red + 0.05f * ((float)red - s_hrm.dc_red);
  }

  float ac_ir_raw = (float)ir - s_hrm.dc_ir;
  float ac_red_raw = (float)red - s_hrm.dc_red;

  /* ── Filtro de paso bajo IIR (Atenuador de ruido) ── */
  // EMA sencillo para smoothing de la señal AC
  static float filtered_ac_ir = 0;
  filtered_ac_ir = filtered_ac_ir + 0.3f * (ac_ir_raw - filtered_ac_ir);
  float ac_filt = filtered_ac_ir;

  /* ── Umbral adaptativo acotado ── */
  // Le damos un decaimiento más rápido al pico viejo para adaptarse a bajas amplitudes
  s_hrm.running_max_ac *= 0.98f; 
  
  // Condicionamos el crecimiento repentino del threshold para ignorar bloqueos por artifacts
  if (ac_filt > s_hrm.running_max_ac) {
    float diff = ac_filt - s_hrm.running_max_ac;
    // Si el salto es masivo (ruido), asimilarlo lentamente (slew rate limite)
    if (diff > 500.0f) {
      s_hrm.running_max_ac += 100.0f;
    } else {
      s_hrm.running_max_ac = ac_filt;
    }
  }

  float peak_threshold = s_hrm.running_max_ac * 0.5f; // Exigimos 50% de prominencia
  if (peak_threshold < 20.0f) peak_threshold = 20.0f; // Piso base robusto

  s_hrm.samples_since_last_peak++;

  /* ── Peak detection: detectar cruce por cero en la derivada (máximo local) ── */
  bool peak = (s_hrm.prev_ac > s_hrm.prev_prev_ac) &&
              (s_hrm.prev_ac > ac_filt) &&
              (s_hrm.prev_ac > peak_threshold);

  if (peak && s_hrm.samples_since_last_peak >= PEAK_REFRACTORY_SAMPLES) {
    uint32_t rr_ms = s_hrm.samples_since_last_peak * (1000U / HRM_FS_HZ);
    
    if (rr_ms >= 300 && rr_ms <= 1500) { // Fisiológico: 40 - 200 BPM
      push_rr(rr_ms);
    } else {
      s_hrm.rr_valid_count = 0; // Descartar racha por anomalía
    }
    s_hrm.samples_since_last_peak = 0;
  }

  s_hrm.prev_prev_ac = s_hrm.prev_ac;
  s_hrm.prev_ac = ac_filt;

  /* ── Acumular AC_rms y DC para SpO2 ── */
  // Para SpO2 usamos la señal raw sin el lpf duro para no perder amplitud relativa
  s_hrm.ac_ir_sq_sum += ac_ir_raw * ac_ir_raw;
  s_hrm.ac_red_sq_sum += ac_red_raw * ac_red_raw;
  s_hrm.dc_ir_sum += s_hrm.dc_ir;
  s_hrm.dc_red_sum += s_hrm.dc_red;
  s_hrm.win_count++;

  if (s_hrm.win_count >= AC_DC_WIN) {
    float ac_ir_rms = sqrtf(s_hrm.ac_ir_sq_sum / s_hrm.win_count);
    float ac_red_rms = sqrtf(s_hrm.ac_red_sq_sum / s_hrm.win_count);
    float dc_ir_avg = s_hrm.dc_ir_sum / s_hrm.win_count;
    float dc_red_avg = s_hrm.dc_red_sum / s_hrm.win_count;

    if (dc_ir_avg > 1000.0f && dc_red_avg > 1000.0f && ac_ir_rms > 1.0f) {
      float r = (ac_red_rms / dc_red_avg) / (ac_ir_rms / dc_ir_avg);
      float spo2 = SPO2_CALIB_A - SPO2_CALIB_B * r;
      if (spo2 > 100.0f) spo2 = 100.0f;
      if (spo2 < 50.0f) spo2 = 50.0f;
      s_hrm.spo2_pct = (uint8_t)(spo2 + 0.5f);
    }

    /* AGC (Control Automático de Ganancia) mejorado */
    if (dc_ir_avg > AGC_DC_MAX || dc_red_avg > AGC_DC_MAX) {
      if (s_hrm.current_led_pa > 0x05) {
        s_hrm.current_led_pa -= 0x05;
        wr_reg(MAX30102_REG_LED1_PA, s_hrm.current_led_pa);
        wr_reg(MAX30102_REG_LED2_PA, s_hrm.current_led_pa);
        s_hrm.dc_ir = s_hrm.dc_red = 0; // Forzar reinicio de tracker de DC para evitar artefacto gigantesco
      }
    } else if (dc_ir_avg < AGC_DC_MIN || dc_red_avg < AGC_DC_MIN) {
      if (s_hrm.current_led_pa < 0x50 && s_hrm.finger) {
        s_hrm.current_led_pa += 0x05;
        wr_reg(MAX30102_REG_LED1_PA, s_hrm.current_led_pa);
        wr_reg(MAX30102_REG_LED2_PA, s_hrm.current_led_pa);
        s_hrm.dc_ir = s_hrm.dc_red = 0; // Forzar reinicio de tracker de DC para evitar artefacto gigantesco
      }
    }

    s_hrm.ac_ir_sq_sum = s_hrm.ac_red_sq_sum = 0;
    s_hrm.dc_ir_sum = s_hrm.dc_red_sum = 0;
    s_hrm.win_count = 0;
  }
}

esp_err_t max30102_get_hr(uint8_t *bpm) {
  if (!bpm)
    return ESP_ERR_INVALID_ARG;
  *bpm = s_hrm.hr_bpm;
  return (s_hrm.hr_bpm != 0 && s_hrm.finger) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t max30102_get_spo2(uint8_t *spo2) {
  if (!spo2)
    return ESP_ERR_INVALID_ARG;
  *spo2 = s_hrm.spo2_pct;
  return (s_hrm.spo2_pct != 0 && s_hrm.finger) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

bool max30102_finger_present(void) { return s_hrm.finger; }

uint32_t max30102_get_overflow_count(void) { return s_hrm.overflow_count; }

esp_err_t max30102_flush_fifo(void) {
  esp_err_t err = ESP_OK;
  /* Limpiar punteros hardware */
  err |= wr_reg(MAX30102_REG_FIFO_WR_PTR, 0x00);
  err |= wr_reg(MAX30102_REG_OVF_COUNTER, 0x00);
  err |= wr_reg(MAX30102_REG_FIFO_RD_PTR, 0x00);

  /* Limpiar estado interno del detector: al descartar muestras viejas
   * no hay relación temporal válida con muestras futuras. */
  s_hrm.rr_valid_count = 0;
  s_hrm.samples_since_last_peak = 0;
  s_hrm.win_count = 0;
  s_hrm.ac_ir_sq_sum = s_hrm.ac_red_sq_sum = 0;
  s_hrm.dc_ir_sum = s_hrm.dc_red_sum = 0;
  s_hrm.running_max_ac = 0.0f;
  s_hrm.prev_ac = 0.0f;
  s_hrm.prev_prev_ac = 0.0f;
  return err;
}

/* ════════════════════════════════════════════════════════════════
 *  Temperatura del die
 * ════════════════════════════════════════════════════════════════ */

esp_err_t max30102_read_temperature(float *temp) {
  if (!temp)
    return ESP_ERR_INVALID_ARG;

  esp_err_t err = wr_reg(MAX30102_REG_DIE_TEMP_CONFIG, 0x01); /* TEMP_EN */
  if (err != ESP_OK)
    return err;

  vTaskDelay(pdMS_TO_TICKS(35)); /* ~29 ms de conversión */

  uint8_t data[2];
  err = i2c_read_bytes(MAX30102_I2C_ADDR, MAX30102_REG_DIE_TEMP_INT, data, 2);
  if (err != ESP_OK)
    return err;

  int8_t t_int = (int8_t)data[0];
  uint8_t t_frac = data[1];
  *temp = (float)t_int + ((float)t_frac * 0.0625f);
  return ESP_OK;
}

#include "max30102.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "MAX30102";

/* ════════════════════════════════════════════════════════════════
 *  Parámetros del algoritmo
 * ════════════════════════════════════════════════════════════════ */

#define HRM_FS_HZ                25      /* sample_avg=4, 100 sps */
#define SAMPLE_PERIOD_MS         (1000 / HRM_FS_HZ)

#define FINGER_IR_THRESHOLD      50000UL

/* SpO2: ventana de cómputo R y mediana móvil */
#define R_WIN_SAMPLES            50      /* 2 s @ 25 Hz para una R */
#define R_BUF_SIZE               10      /* mediana sobre 10 valores R = 20 s efectivos */

/* HR: buffer circular de RR para mediana */
#define RR_BUF_SIZE              8
#define RR_MIN_MS                300     /* 200 BPM */
#define RR_MAX_MS                1500    /* 40 BPM  */
#define RR_DELTA_REJECT_PCT      25      /* descarta RR si difiere >25% del último válido */

/* Detección de picos */
#define PEAK_REFRACTORY_SAMPLES  7       /* 280 ms → cap ~214 BPM */
#define PEAK_THRESHOLD_FLOOR     20.0f
#define PEAK_THRESHOLD_FRAC      0.50f

/* Calibración SpO2 (R → %) */
#define SPO2_CALIB_A             110.0f
#define SPO2_CALIB_B             25.0f

/* AGC (Control Automático de Ganancia) */
#define AGC_DC_MIN               50000.0f
#define AGC_DC_MAX               200000.0f

/* Motion gating */
#define MOTION_GATE_THRESHOLD    80      /* 0..255, jerk score */
#define MOTION_INVALIDATE_MS     800     /* invalidar racha por 0.8 s tras movimiento */

/* SPOT */
#define SPOT_SETTLE_MS           5000
#define SPOT_MIN_MEASURE_MS      7000
#define SPOT_MAX_MEASURE_MS      25000
#define SPOT_MIN_RR_FOR_GOOD     8
#define SPOT_MIN_R_FOR_GOOD      5
#define SPOT_RR_CV_GOOD_PCT      8        /* σ/μ ≤ 8% */
#define SPOT_R_CV_GOOD_PCT       5        /* σ/μ ≤ 5% */

/* ════════════════════════════════════════════════════════════════
 *  Bandpass IIR Butterworth: cascada HPF 0.5 Hz + LPF 4 Hz @ Fs=25 Hz
 *  Coeficientes pre-calculados (Direct Form II Transposed).
 *
 *  HPF biquad orden 2 a 0.5 Hz:
 *    b0=0.91497, b1=-1.82994, b2=0.91497
 *    a1=-1.82269, a2=0.83718
 *
 *  LPF biquad orden 2 a 4 Hz:
 *    b0=0.14518, b1=0.29036, b2=0.14518
 *    a1=-0.67214, a2=0.25287
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float z1, z2;
} biquad_t;

static inline float biquad_process(biquad_t *bq, float in) {
    /* Direct Form II Transposed */
    float out = bq->b0 * in + bq->z1;
    bq->z1 = bq->b1 * in - bq->a1 * out + bq->z2;
    bq->z2 = bq->b2 * in - bq->a2 * out;
    return out;
}

static inline void biquad_reset(biquad_t *bq) {
    bq->z1 = 0.0f;
    bq->z2 = 0.0f;
}

/* ════════════════════════════════════════════════════════════════
 *  Estado del algoritmo
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    /* DC tracker (lento) */
    float dc_ir;
    float dc_red;

    /* Bandpass IIR (canal IR) */
    biquad_t bp_hpf;
    biquad_t bp_lpf;
    float prev_filt;
    float prev_prev_filt;

    /* Pico tracking */
    uint32_t samples_since_last_peak;
    float    running_max_ac;

    /* Buffer circular de RR (continuo) */
    uint16_t rr_buf[RR_BUF_SIZE];
    uint8_t  rr_idx;
    uint8_t  rr_count;
    uint16_t last_valid_rr_ms;

    /* Acumulador para una ventana R (SpO2) */
    float    ac_ir_sq_sum;
    float    ac_red_sq_sum;
    float    dc_ir_sum;
    float    dc_red_sum;
    uint16_t r_win_count;

    /* Buffer circular de valores R (SpO2 continuo) */
    float    r_buf[R_BUF_SIZE];
    uint8_t  r_idx;
    uint8_t  r_count;

    /* Salidas continuas */
    uint8_t  hr_bpm;
    uint8_t  spo2_pct;

    /* AGC */
    uint8_t  current_led_pa;
    uint32_t agc_freeze_until_ms; /* 0 = sin freeze */

    /* Estado de hardware */
    bool     awake;
    bool     finger;

    /* Motion gating */
    uint8_t  motion_level;
    uint32_t motion_invalidate_until_ms;

    /* Diagnóstico */
    uint32_t overflow_count;
} hrm_state_t;

static hrm_state_t s_hrm;

/* ════════════════════════════════════════════════════════════════
 *  Estado SPOT
 * ════════════════════════════════════════════════════════════════ */

#define SPOT_RR_BUF_SIZE   32
#define SPOT_R_BUF_SIZE    16

typedef struct {
    max30102_spot_state_t   state;
    uint32_t                start_ms;
    uint32_t                measure_start_ms;
    uint16_t                rr_collected[SPOT_RR_BUF_SIZE];
    uint8_t                 rr_n;
    float                   r_collected[SPOT_R_BUF_SIZE];
    uint8_t                 r_n;

    /* Resultado final */
    uint8_t                 result_bpm;
    uint8_t                 result_spo2;
    uint16_t                result_duration_ms;
    max30102_spot_quality_t result_quality;
} spot_state_t;

static spot_state_t s_spot;

/* ════════════════════════════════════════════════════════════════
 *  Helpers I2C
 * ════════════════════════════════════════════════════════════════ */
static esp_err_t wr_reg(uint8_t reg, uint8_t val) {
    return i2c_write_bytes(MAX30102_I2C_ADDR, reg, &val, 1);
}

static esp_err_t rd_reg(uint8_t reg, uint8_t *val) {
    return i2c_read_bytes(MAX30102_I2C_ADDR, reg, val, 1);
}

/* ════════════════════════════════════════════════════════════════
 *  Helpers de mediana / estadística
 * ════════════════════════════════════════════════════════════════ */

static int cmp_u32(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

static int cmp_float(const void *a, const void *b) {
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

/* Tamaños suficientes para SPOT (hasta 32 RR / 16 R). */
static uint16_t median_u16(const uint16_t *src, uint8_t n) {
    if (n == 0) return 0;
    uint32_t tmp[32];
    if (n > 32) n = 32;
    for (uint8_t i = 0; i < n; i++) tmp[i] = src[i];
    qsort(tmp, n, sizeof(uint32_t), cmp_u32);
    return (uint16_t)tmp[n / 2];
}

static float median_float(const float *src, uint8_t n) {
    if (n == 0) return 0.0f;
    float tmp[16];
    if (n > 16) n = 16;
    for (uint8_t i = 0; i < n; i++) tmp[i] = src[i];
    qsort(tmp, n, sizeof(float), cmp_float);
    return tmp[n / 2];
}

static float cv_pct_u32(const uint32_t *src, uint8_t n) {
    if (n < 2) return 100.0f;
    float sum = 0;
    for (uint8_t i = 0; i < n; i++) sum += src[i];
    float mean = sum / n;
    if (mean < 1.0f) return 100.0f;
    float ssq = 0;
    for (uint8_t i = 0; i < n; i++) {
        float d = src[i] - mean;
        ssq += d * d;
    }
    float sd = sqrtf(ssq / n);
    return 100.0f * sd / mean;
}

static float cv_pct_float(const float *src, uint8_t n) {
    if (n < 2) return 100.0f;
    float sum = 0;
    for (uint8_t i = 0; i < n; i++) sum += src[i];
    float mean = sum / n;
    if (fabsf(mean) < 1e-9f) return 100.0f;
    float ssq = 0;
    for (uint8_t i = 0; i < n; i++) {
        float d = src[i] - mean;
        ssq += d * d;
    }
    float sd = sqrtf(ssq / n);
    return 100.0f * sd / fabsf(mean);
}

static uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* ════════════════════════════════════════════════════════════════
 *  Init
 * ════════════════════════════════════════════════════════════════ */

static void reset_algo_state(void) {
    /* Limpia todo MENOS los coeficientes del bandpass y el current_led_pa */
    biquad_t bp_hpf_save = s_hrm.bp_hpf;
    biquad_t bp_lpf_save = s_hrm.bp_lpf;
    uint8_t led_save = s_hrm.current_led_pa;
    uint32_t ovf_save = s_hrm.overflow_count;
    bool awake_save = s_hrm.awake;

    memset(&s_hrm, 0, sizeof(s_hrm));

    s_hrm.bp_hpf = bp_hpf_save;
    s_hrm.bp_lpf = bp_lpf_save;
    s_hrm.current_led_pa = led_save ? led_save : 0x24;
    s_hrm.overflow_count = ovf_save;
    s_hrm.awake = awake_save;

    biquad_reset(&s_hrm.bp_hpf);
    biquad_reset(&s_hrm.bp_lpf);
}

esp_err_t max30102_init(void) { return max30102_init_hrm(); }

esp_err_t max30102_init_hrm(void) {
    uint8_t part_id = 0;
    esp_err_t err;

    err = rd_reg(MAX30102_REG_PART_ID, &part_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se puede leer PART_ID (err=%s)", esp_err_to_name(err));
        return err;
    }
    if (part_id != MAX30102_PART_ID_VALUE) {
        ESP_LOGE(TAG, "PART_ID inesperado: 0x%02X", part_id);
        return ESP_ERR_NOT_FOUND;
    }

    /* Soft reset */
    wr_reg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_RESET);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Limpiar punteros FIFO */
    wr_reg(MAX30102_REG_FIFO_WR_PTR, 0x00);
    wr_reg(MAX30102_REG_OVF_COUNTER, 0x00);
    wr_reg(MAX30102_REG_FIFO_RD_PTR, 0x00);

    /* SMP_AVE=4, ROLLOVER_EN=1, A_FULL=17 → 0x5F */
    wr_reg(MAX30102_REG_FIFO_CONFIG, 0x5F);

    /* ADC_RGE=4096nA, SR=100sps, LED_PW=411us(18bit) → 0x27 */
    wr_reg(MAX30102_REG_SPO2_CONFIG, 0x27);

    /* LED PA inicial ~7.2 mA */
    wr_reg(MAX30102_REG_LED1_PA, 0x24);
    wr_reg(MAX30102_REG_LED2_PA, 0x24);

    /* Modo SpO2 (Red + IR) */
    wr_reg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Reborrar punteros tras entrar en modo SpO2 */
    wr_reg(MAX30102_REG_FIFO_WR_PTR, 0x00);
    wr_reg(MAX30102_REG_OVF_COUNTER, 0x00);
    wr_reg(MAX30102_REG_FIFO_RD_PTR, 0x00);

    /* Inicializar coeficientes biquad */
    s_hrm.bp_hpf = (biquad_t){
        .b0 =  0.91497f, .b1 = -1.82994f, .b2 =  0.91497f,
        .a1 = -1.82269f, .a2 =  0.83718f,
    };
    s_hrm.bp_lpf = (biquad_t){
        .b0 =  0.14518f, .b1 =  0.29036f, .b2 =  0.14518f,
        .a1 = -0.67214f, .a2 =  0.25287f,
    };
    biquad_reset(&s_hrm.bp_hpf);
    biquad_reset(&s_hrm.bp_lpf);

    s_hrm.current_led_pa = 0x24;
    s_hrm.awake = true;
    s_hrm.overflow_count = 0;

    memset(&s_spot, 0, sizeof(s_spot));

    ESP_LOGI(TAG, "MAX30102 init OK (25 Hz efectivos, BP 0.5-4 Hz, mediana RR/R)");
    return ESP_OK;
}

/* ════════════════════════════════════════════════════════════════
 *  Lectura FIFO (igual que antes, sin cambios sustanciales)
 * ════════════════════════════════════════════════════════════════ */

esp_err_t max30102_read_samples(max30102_sample_t *out, uint8_t max_samples,
                                uint8_t *n_read) {
    if (!out || !n_read || max_samples == 0) return ESP_ERR_INVALID_ARG;
    *n_read = 0;
    if (!s_hrm.awake) return ESP_OK;

    uint8_t wr_ptr = 0, rd_ptr = 0, ovf = 0;
    esp_err_t err;
    if ((err = rd_reg(MAX30102_REG_FIFO_WR_PTR, &wr_ptr)) != ESP_OK) return err;
    if ((err = rd_reg(MAX30102_REG_OVF_COUNTER, &ovf)) != ESP_OK) return err;
    if ((err = rd_reg(MAX30102_REG_FIFO_RD_PTR, &rd_ptr)) != ESP_OK) return err;

    /* En clones, OVF=0x1F suele indicar overflow real. */
    bool overflowed = (ovf == 0x1F);
    if (overflowed) {
        ESP_LOGW(TAG, "FIFO overflow! wr=%d rd=%d ovf=%d", wr_ptr, rd_ptr, ovf);
        s_hrm.overflow_count++;
        s_hrm.rr_count = 0;
        s_hrm.r_win_count = 0;
        s_hrm.r_count = 0;
        s_hrm.samples_since_last_peak = 0;
        s_hrm.ac_ir_sq_sum = s_hrm.ac_red_sq_sum = 0;
        s_hrm.dc_ir_sum = s_hrm.dc_red_sum = 0;
        wr_reg(MAX30102_REG_OVF_COUNTER, 0);
    }

    int pending;
    if (wr_ptr == rd_ptr) {
        pending = overflowed ? 32 : 0;
    } else {
        pending = (int)wr_ptr - (int)rd_ptr;
        if (pending < 0) pending += 32;
    }
    if (pending == 0) return ESP_OK;
    if (pending > max_samples) pending = max_samples;

    uint8_t buf[32 * 6];
    int total_bytes = pending * 6;
    err = i2c_read_bytes(MAX30102_I2C_ADDR, MAX30102_REG_FIFO_DATA, buf, total_bytes);
    if (err != ESP_OK) return err;

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
    if (err != ESP_OK) return err;
    if (n == 0) {
        if (red) *red = 0;
        if (ir)  *ir = 0;
        return ESP_OK;
    }
    if (red) *red = s.red;
    if (ir)  *ir = s.ir;
    return ESP_OK;
}

/* ════════════════════════════════════════════════════════════════
 *  RR buffer + mediana → BPM
 * ════════════════════════════════════════════════════════════════ */

static void push_rr(uint16_t rr_ms) {
    /* Outlier reject: si difiere > 25% del último válido, descarta */
    if (s_hrm.last_valid_rr_ms > 0) {
        int delta = (int)rr_ms - (int)s_hrm.last_valid_rr_ms;
        int lim = (int)s_hrm.last_valid_rr_ms * RR_DELTA_REJECT_PCT / 100;
        if (delta > lim || delta < -lim) {
            return; /* descartado, no actualiza buffer */
        }
    }
    s_hrm.last_valid_rr_ms = rr_ms;

    s_hrm.rr_buf[s_hrm.rr_idx] = rr_ms;
    s_hrm.rr_idx = (s_hrm.rr_idx + 1) % RR_BUF_SIZE;
    if (s_hrm.rr_count < RR_BUF_SIZE) s_hrm.rr_count++;

    uint16_t med = median_u16(s_hrm.rr_buf, s_hrm.rr_count);
    if (med > 0) {
        uint32_t bpm = 60000U / med;
        if (bpm >= 40 && bpm <= 200) {
            s_hrm.hr_bpm = (uint8_t)bpm;
        }
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Motion gating
 * ════════════════════════════════════════════════════════════════ */

void max30102_set_motion_level(uint8_t jerk_score) {
    s_hrm.motion_level = jerk_score;
    if (jerk_score >= MOTION_GATE_THRESHOLD) {
        s_hrm.motion_invalidate_until_ms = now_ms() + MOTION_INVALIDATE_MS;
    }
}

static inline bool motion_active(void) {
    return now_ms() < s_hrm.motion_invalidate_until_ms;
}

/* ════════════════════════════════════════════════════════════════
 *  SPOT — máquina de estados
 * ════════════════════════════════════════════════════════════════ */

esp_err_t max30102_spot_start(void) {
    if (s_spot.state == SPOT_STATE_SETTLING || s_spot.state == SPOT_STATE_MEASURING) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(&s_spot, 0, sizeof(s_spot));
    s_spot.state = SPOT_STATE_SETTLING;
    s_spot.start_ms = now_ms();
    return ESP_OK;
}

esp_err_t max30102_spot_abort(void) {
    if (s_spot.state == SPOT_STATE_SETTLING || s_spot.state == SPOT_STATE_MEASURING) {
        s_spot.state = SPOT_STATE_ABORTED;
    }
    return ESP_OK;
}

void max30102_spot_get_status(max30102_spot_status_t *out) {
    if (!out) return;
    out->state        = s_spot.state;
    out->bpm          = s_spot.result_bpm;
    out->spo2         = s_spot.result_spo2;
    out->duration_ms  = s_spot.result_duration_ms;
    out->quality      = s_spot.result_quality;

    /* Progreso: combina tiempo transcurrido y "completitud" de buffers */
    uint8_t pct_time = 0, pct_data = 0;
    if (s_spot.state == SPOT_STATE_SETTLING) {
        uint32_t el = now_ms() - s_spot.start_ms;
        pct_time = (uint8_t)((el * 30) / SPOT_SETTLE_MS);  /* 0..30 */
    } else if (s_spot.state == SPOT_STATE_MEASURING) {
        uint32_t el = now_ms() - s_spot.measure_start_ms;
        pct_time = 30 + (uint8_t)((el * 70) / SPOT_MAX_MEASURE_MS); /* 30..100 */
        if (pct_time > 100) pct_time = 100;
        pct_data = (s_spot.rr_n * 100) / SPOT_MIN_RR_FOR_GOOD;
        if (pct_data > 100) pct_data = 100;
        /* progreso = max entre tiempo y data */
        if (pct_data > pct_time) pct_time = pct_data;
    } else if (s_spot.state == SPOT_STATE_DONE ||
               s_spot.state == SPOT_STATE_FAILED ||
               s_spot.state == SPOT_STATE_ABORTED) {
        pct_time = 100;
    }
    out->progress_pct = pct_time;
}

/* Llamado desde process_sample para alimentar el SPOT con cada RR/R. */
static void spot_feed_rr(uint16_t rr_ms) {
    if (s_spot.state != SPOT_STATE_MEASURING) return;
    if (s_spot.rr_n < SPOT_RR_BUF_SIZE) {
        s_spot.rr_collected[s_spot.rr_n++] = rr_ms;
    }
}

static void spot_feed_r(float r) {
    if (s_spot.state != SPOT_STATE_MEASURING) return;
    if (s_spot.r_n < SPOT_R_BUF_SIZE) {
        s_spot.r_collected[s_spot.r_n++] = r;
    }
}

/* CV% sobre buffer uint16_t. */
static float cv_pct_u16(const uint16_t *src, uint8_t n) {
    if (n < 2) return 100.0f;
    float sum = 0;
    for (uint8_t i = 0; i < n; i++) sum += src[i];
    float mean = sum / n;
    if (mean < 1.0f) return 100.0f;
    float ssq = 0;
    for (uint8_t i = 0; i < n; i++) {
        float d = (float)src[i] - mean;
        ssq += d * d;
    }
    return 100.0f * sqrtf(ssq / n) / mean;
}

/* Evalúa transiciones del SPOT al final de cada process_sample. */
static void spot_tick(void) {
    if (s_spot.state == SPOT_STATE_SETTLING) {
        if (now_ms() - s_spot.start_ms >= SPOT_SETTLE_MS) {
            s_spot.state = SPOT_STATE_MEASURING;
            s_spot.measure_start_ms = now_ms();
        }
        return;
    }
    if (s_spot.state != SPOT_STATE_MEASURING) return;

    uint32_t elapsed = now_ms() - s_spot.measure_start_ms;

    /* Movimiento sostenido en SPOT → abort (en spot exigimos quietud) */
    if (motion_active() && s_spot.rr_n < SPOT_MIN_RR_FOR_GOOD / 2) {
        /* dejamos pasar; pero si dura todo el período sin acumular,
         * el timeout normal hará FAILED */
    }

    /* Sin dedo durante medición → abort */
    if (!s_hrm.finger) {
        s_spot.state = SPOT_STATE_ABORTED;
        s_spot.result_duration_ms = (uint16_t)elapsed;
        return;
    }

    /* Early-exit por calidad */
    if (elapsed >= SPOT_MIN_MEASURE_MS &&
        s_spot.rr_n >= SPOT_MIN_RR_FOR_GOOD &&
        s_spot.r_n  >= SPOT_MIN_R_FOR_GOOD) {

        float rr_cv = cv_pct_u16(s_spot.rr_collected, s_spot.rr_n);
        float r_cv  = cv_pct_float(s_spot.r_collected, s_spot.r_n);

        if (rr_cv <= SPOT_RR_CV_GOOD_PCT && r_cv <= SPOT_R_CV_GOOD_PCT) {
            uint16_t rr_med = median_u16(s_spot.rr_collected, s_spot.rr_n);
            float r_med     = median_float(s_spot.r_collected, s_spot.r_n);

            uint8_t bpm = rr_med > 0 ? (uint8_t)(60000U / rr_med) : 0;
            float spo2  = SPO2_CALIB_A - SPO2_CALIB_B * r_med;
            if (spo2 > 100.0f) spo2 = 100.0f;
            if (spo2 < 50.0f)  spo2 = 50.0f;

            s_spot.result_bpm         = bpm;
            s_spot.result_spo2        = (uint8_t)(spo2 + 0.5f);
            s_spot.result_duration_ms = (uint16_t)elapsed;
            s_spot.result_quality     = SPOT_QUALITY_GOOD;
            s_spot.state              = SPOT_STATE_DONE;
            return;
        }
    }

    /* Timeout */
    if (elapsed >= SPOT_MAX_MEASURE_MS) {
        if (s_spot.rr_n >= 4 && s_spot.r_n >= 3) {
            uint16_t rr_med = median_u16(s_spot.rr_collected, s_spot.rr_n);
            float r_med     = median_float(s_spot.r_collected, s_spot.r_n);
            uint8_t bpm = rr_med > 0 ? (uint8_t)(60000U / rr_med) : 0;
            float spo2  = SPO2_CALIB_A - SPO2_CALIB_B * r_med;
            if (spo2 > 100.0f) spo2 = 100.0f;
            if (spo2 < 50.0f)  spo2 = 50.0f;

            s_spot.result_bpm     = bpm;
            s_spot.result_spo2    = (uint8_t)(spo2 + 0.5f);
            s_spot.result_quality = SPOT_QUALITY_FAIR;
            s_spot.state          = SPOT_STATE_DONE;
        } else {
            s_spot.result_quality = SPOT_QUALITY_POOR;
            s_spot.state          = SPOT_STATE_FAILED;
        }
        s_spot.result_duration_ms = (uint16_t)elapsed;
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Pipeline principal — process_sample
 * ════════════════════════════════════════════════════════════════ */

void max30102_process_sample(uint32_t red, uint32_t ir) {
    /* Detección de dedo */
    s_hrm.finger = (ir > FINGER_IR_THRESHOLD);
    if (!s_hrm.finger) {
        s_hrm.hr_bpm = 0;
        s_hrm.spo2_pct = 0;
        s_hrm.rr_count = 0;
        s_hrm.last_valid_rr_ms = 0;
        s_hrm.r_count = 0;
        s_hrm.r_win_count = 0;
        s_hrm.samples_since_last_peak = 0;
        s_hrm.ac_ir_sq_sum = s_hrm.ac_red_sq_sum = 0;
        s_hrm.dc_ir_sum = s_hrm.dc_red_sum = 0;
        s_hrm.running_max_ac = 0;
        s_hrm.dc_ir = 0;
        s_hrm.dc_red = 0;
        biquad_reset(&s_hrm.bp_hpf);
        biquad_reset(&s_hrm.bp_lpf);
        spot_tick();
        return;
    }

    /* DC tracker EMA lento (α = 0.05) */
    if (s_hrm.dc_ir == 0) {
        s_hrm.dc_ir = ir;
        s_hrm.dc_red = red;
    } else {
        s_hrm.dc_ir  += 0.05f * ((float)ir  - s_hrm.dc_ir);
        s_hrm.dc_red += 0.05f * ((float)red - s_hrm.dc_red);
    }

    float ac_ir_raw  = (float)ir  - s_hrm.dc_ir;
    float ac_red_raw = (float)red - s_hrm.dc_red;

    /* Bandpass IIR (HPF → LPF) sobre canal IR para detección de picos */
    float filt = biquad_process(&s_hrm.bp_hpf, ac_ir_raw);
    filt = biquad_process(&s_hrm.bp_lpf, filt);

    /* Umbral adaptativo de pico */
    s_hrm.running_max_ac *= 0.98f;
    if (filt > s_hrm.running_max_ac) {
        float diff = filt - s_hrm.running_max_ac;
        if (diff > 500.0f) {
            s_hrm.running_max_ac += 100.0f;  /* slew rate limit anti-spike */
        } else {
            s_hrm.running_max_ac = filt;
        }
    }
    float peak_threshold = s_hrm.running_max_ac * PEAK_THRESHOLD_FRAC;
    if (peak_threshold < PEAK_THRESHOLD_FLOOR) peak_threshold = PEAK_THRESHOLD_FLOOR;

    s_hrm.samples_since_last_peak++;

    /* Pico = máximo local (cruce por cero de la derivada) */
    bool peak = (s_hrm.prev_filt > s_hrm.prev_prev_filt) &&
                (s_hrm.prev_filt > filt) &&
                (s_hrm.prev_filt > peak_threshold);

    if (peak && s_hrm.samples_since_last_peak >= PEAK_REFRACTORY_SAMPLES &&
        !motion_active()) {
        uint16_t rr_ms = (uint16_t)(s_hrm.samples_since_last_peak * SAMPLE_PERIOD_MS);
        if (rr_ms >= RR_MIN_MS && rr_ms <= RR_MAX_MS) {
            push_rr(rr_ms);
            spot_feed_rr(rr_ms);
        }
        s_hrm.samples_since_last_peak = 0;
    }

    s_hrm.prev_prev_filt = s_hrm.prev_filt;
    s_hrm.prev_filt = filt;

    /* Ventana R para SpO2 (usa AC raw, no la del bandpass) */
    s_hrm.ac_ir_sq_sum  += ac_ir_raw  * ac_ir_raw;
    s_hrm.ac_red_sq_sum += ac_red_raw * ac_red_raw;
    s_hrm.dc_ir_sum     += s_hrm.dc_ir;
    s_hrm.dc_red_sum    += s_hrm.dc_red;
    s_hrm.r_win_count++;

    if (s_hrm.r_win_count >= R_WIN_SAMPLES) {
        float ac_ir_rms  = sqrtf(s_hrm.ac_ir_sq_sum  / s_hrm.r_win_count);
        float ac_red_rms = sqrtf(s_hrm.ac_red_sq_sum / s_hrm.r_win_count);
        float dc_ir_avg  = s_hrm.dc_ir_sum  / s_hrm.r_win_count;
        float dc_red_avg = s_hrm.dc_red_sum / s_hrm.r_win_count;

        bool agc_was_frozen = (s_hrm.agc_freeze_until_ms > now_ms());

        if (!motion_active() && !agc_was_frozen &&
            dc_ir_avg > 1000.0f && dc_red_avg > 1000.0f && ac_ir_rms > 1.0f) {

            float r = (ac_red_rms / dc_red_avg) / (ac_ir_rms / dc_ir_avg);

            /* Push al buffer circular */
            s_hrm.r_buf[s_hrm.r_idx] = r;
            s_hrm.r_idx = (s_hrm.r_idx + 1) % R_BUF_SIZE;
            if (s_hrm.r_count < R_BUF_SIZE) s_hrm.r_count++;

            /* Mediana móvil */
            float r_med = median_float(s_hrm.r_buf, s_hrm.r_count);
            float spo2 = SPO2_CALIB_A - SPO2_CALIB_B * r_med;
            if (spo2 > 100.0f) spo2 = 100.0f;
            if (spo2 < 50.0f)  spo2 = 50.0f;
            s_hrm.spo2_pct = (uint8_t)(spo2 + 0.5f);

            spot_feed_r(r);
        }

        /* AGC: ajusta LED PA si DC fuera de banda; congela 2 s tras cambio */
        if (!agc_was_frozen) {
            bool changed = false;
            if (dc_ir_avg > AGC_DC_MAX || dc_red_avg > AGC_DC_MAX) {
                if (s_hrm.current_led_pa > 0x05) {
                    s_hrm.current_led_pa -= 0x05;
                    wr_reg(MAX30102_REG_LED1_PA, s_hrm.current_led_pa);
                    wr_reg(MAX30102_REG_LED2_PA, s_hrm.current_led_pa);
                    changed = true;
                }
            } else if (dc_ir_avg < AGC_DC_MIN || dc_red_avg < AGC_DC_MIN) {
                if (s_hrm.current_led_pa < 0x50) {
                    s_hrm.current_led_pa += 0x05;
                    wr_reg(MAX30102_REG_LED1_PA, s_hrm.current_led_pa);
                    wr_reg(MAX30102_REG_LED2_PA, s_hrm.current_led_pa);
                    changed = true;
                }
            }
            if (changed) {
                /* Congelar SpO2 y AGC por 2 s para que se estabilice */
                s_hrm.agc_freeze_until_ms = now_ms() + 2000;
                s_hrm.dc_ir = 0;  /* fuerza re-tracking */
                s_hrm.dc_red = 0;
            }
        }

        s_hrm.ac_ir_sq_sum = s_hrm.ac_red_sq_sum = 0;
        s_hrm.dc_ir_sum = s_hrm.dc_red_sum = 0;
        s_hrm.r_win_count = 0;
    }

    spot_tick();
}

/* ════════════════════════════════════════════════════════════════
 *  Getters
 * ════════════════════════════════════════════════════════════════ */

esp_err_t max30102_get_hr(uint8_t *bpm) {
    if (!bpm) return ESP_ERR_INVALID_ARG;
    *bpm = s_hrm.hr_bpm;
    return (s_hrm.hr_bpm != 0 && s_hrm.finger) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t max30102_get_spo2(uint8_t *spo2) {
    if (!spo2) return ESP_ERR_INVALID_ARG;
    *spo2 = s_hrm.spo2_pct;
    return (s_hrm.spo2_pct != 0 && s_hrm.finger) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

bool max30102_finger_present(void) { return s_hrm.finger; }

uint32_t max30102_get_overflow_count(void) { return s_hrm.overflow_count; }

esp_err_t max30102_flush_fifo(void) {
    esp_err_t err = ESP_OK;
    err |= wr_reg(MAX30102_REG_FIFO_WR_PTR, 0x00);
    err |= wr_reg(MAX30102_REG_OVF_COUNTER, 0x00);
    err |= wr_reg(MAX30102_REG_FIFO_RD_PTR, 0x00);
    reset_algo_state();
    return err;
}

/* ════════════════════════════════════════════════════════════════
 *  SHDN / Wake
 * ════════════════════════════════════════════════════════════════ */

esp_err_t max30102_shutdown(void) {
    if (!s_hrm.awake) return ESP_OK;
    esp_err_t err = wr_reg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SHDN | MAX30102_MODE_SPO2);
    if (err == ESP_OK) {
        s_hrm.awake = false;
    }
    return err;
}

esp_err_t max30102_wake(void) {
    if (s_hrm.awake) return ESP_OK;
    esp_err_t err = wr_reg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2);
    if (err != ESP_OK) return err;
    s_hrm.awake = true;
    vTaskDelay(pdMS_TO_TICKS(10));
    /* Reset FIFO + estado para empezar limpio */
    return max30102_flush_fifo();
}

bool max30102_is_awake(void) { return s_hrm.awake; }

/* ════════════════════════════════════════════════════════════════
 *  Temperatura del die
 * ════════════════════════════════════════════════════════════════ */

esp_err_t max30102_read_temperature(float *temp) {
    if (!temp) return ESP_ERR_INVALID_ARG;
    esp_err_t err = wr_reg(MAX30102_REG_DIE_TEMP_CONFIG, 0x01);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(35));
    uint8_t data[2];
    err = i2c_read_bytes(MAX30102_I2C_ADDR, MAX30102_REG_DIE_TEMP_INT, data, 2);
    if (err != ESP_OK) return err;
    int8_t t_int = (int8_t)data[0];
    uint8_t t_frac = data[1];
    *temp = (float)t_int + ((float)t_frac * 0.0625f);
    return ESP_OK;
}

#include "spo2_algo.h"
#include <math.h>
#include <string.h>

// ==== ОПТИМІЗОВАНІ КОНСТАНТИ ====
#define MIN_AC_AMPLITUDE   20.0f      // Реальніша мінімальна амплітуда
#define MIN_DC_VALUE       5000.0f    // Реальніше мінімальне DC
#define R_RATIO_MIN        0.4f       // Реальніший діапазон R
#define R_RATIO_MAX        2.0f
#define SPO2_WINDOW        12         // Оптимальне вікно для швидкості/стабільності

#define SPO2_MIN           70.0f      // Реальніший діапазон SpO2
#define SPO2_MAX           100.0f

// Адаптивні калібрувальні коефіцієнти
#define CALIB_A            110.0f
#define CALIB_B            25.0f

// Коефіцієнти якості сигналу
#define MIN_SNR_RATIO      2.0f       // Мінімальне співвідношення сигнал/шум
#define MAX_AC_DC_RATIO    0.03f      // Максимальне AC/DC співвідношення

void spo2_init(spo2_algo_t *s)
{
    memset(s, 0, sizeof(spo2_algo_t));
    s->idx = 0;
    s->count = 0;
    s->last_valid_spo2 = 0.0f;
    s->quality = 0.0f;
    s->consecutive_valid = 0;
    s->calib_a = CALIB_A;
    s->calib_b = CALIB_B;

    for (int i = 0; i < SPO2_WINDOW; i++) {
        s->ratio_window[i] = 0.0f;
        s->ac_amplitude_window[i] = 0.0f;
    }
}

void spo2_reset(spo2_algo_t *s)
{
    spo2_init(s);
}

// ===== ОПТИМІЗОВАНИЙ АЛГОРИТМ SpO2 =====
bool spo2_update(
    spo2_algo_t *s,
    float red_ac, float red_dc,
    float ir_ac,  float ir_dc,
    float *out_spo2
)
{
    if (s == NULL || out_spo2 == NULL) {
        return false;
    }

    // === БАЗОВІ ПЕРЕВІРКИ ЯКОСТІ ===
    if (!isfinite(red_ac) || !isfinite(red_dc) || !isfinite(ir_ac) || !isfinite(ir_dc)) {
        s->consecutive_valid = 0;
        s->quality *= 0.8f; // Поступове зниження якості
        return false;
    }

    // Перевірка реалістичних значень DC
    if (ir_dc < MIN_DC_VALUE || red_dc < MIN_DC_VALUE ||
        ir_dc > 1000000.0f || red_dc > 1000000.0f) {
        s->consecutive_valid = 0;
        s->quality *= 0.8f;
        return false;
    }

    // Перевірка амплітуди AC сигналу
    float ir_ac_abs = fabsf(ir_ac);
    float red_ac_abs = fabsf(red_ac);

    if (ir_ac_abs < MIN_AC_AMPLITUDE || red_ac_abs < MIN_AC_AMPLITUDE) {
        s->consecutive_valid = 0;
        s->quality *= 0.8f;
        return false;
    }

    // Перевірка співвідношення AC/DC (захист від артефактів)
    float ir_ac_dc_ratio = ir_ac_abs / ir_dc;
    float red_ac_dc_ratio = red_ac_abs / red_dc;

    if (ir_ac_dc_ratio > MAX_AC_DC_RATIO || red_ac_dc_ratio > MAX_AC_DC_RATIO) {
        s->consecutive_valid = 0;
        s->quality *= 0.7f;
        return false;
    }

    // === РОЗРАХУНОК КОЕФІЦІЄНТА R ===
    float red_ratio = red_ac_abs / red_dc;
    float ir_ratio = ir_ac_abs / ir_dc;

    // Перевірка реалістичних співвідношень
    if (red_ratio < 0.001f || ir_ratio < 0.001f ||
        red_ratio > 0.02f || ir_ratio > 0.02f) {
        s->consecutive_valid = 0;
        s->quality *= 0.8f;
        return false;
    }

    float R = red_ratio / ir_ratio;

    // Перевірка діапазону R
    if (R < R_RATIO_MIN || R > R_RATIO_MAX) {
        s->consecutive_valid = 0;
        s->quality *= 0.7f;
        return false;
    }

    // === РОЗРАХУНОК ЯКОСТІ СИГНАЛУ ===
    float snr_ratio = (ir_ac_abs > 0.1f) ? (ir_ac_abs / fmaxf(red_ac_abs, 1.0f)) : 0.0f;
    float current_quality = 0.0f;

    // Критерії якості
    if (snr_ratio >= MIN_SNR_RATIO &&
        ir_ac_abs >= MIN_AC_AMPLITUDE * 2.0f &&
        ir_ac_dc_ratio >= 0.005f) {
        current_quality = 0.8f;
    } else if (ir_ac_abs >= MIN_AC_AMPLITUDE) {
        current_quality = 0.5f;
    } else {
        current_quality = 0.2f;
    }

    // === ЗГЛАДЖУВАННЯ ТА ФІЛЬТРАЦІЯ ===
    s->ratio_window[s->idx] = R;
    s->ac_amplitude_window[s->idx] = ir_ac_abs;
    s->idx = (s->idx + 1) % SPO2_WINDOW;

    if (s->count < SPO2_WINDOW) {
        s->count++;
    }

    // Можна працювати з меншою кількістю семплів, але з нижчою якістю
    int min_samples = (s->count >= 8) ? 6 : 4;
    if (s->count < min_samples) {
        s->consecutive_valid = 0;
        s->quality = fmaxf(s->quality * 0.9f, current_quality);
        return false;
    }

    // Адаптивне середнє з вагами за якістю
    float sum_ratio = 0.0f;
    float sum_weights = 0.0f;
    int valid_samples = 0;

    for (int i = 0; i < s->count; i++) {
        float sample_ratio = s->ratio_window[i];
        float sample_amplitude = s->ac_amplitude_window[i];

        if (sample_ratio >= R_RATIO_MIN && sample_ratio <= R_RATIO_MAX &&
            sample_amplitude >= MIN_AC_AMPLITUDE) {

            // Вага залежить від амплітуди (сильніший сигнал = вища якість)
            float weight = fminf(sample_amplitude / (MIN_AC_AMPLITUDE * 2.0f), 2.0f);
            sum_ratio += sample_ratio * weight;
            sum_weights += weight;
            valid_samples++;
        }
    }

    // Вимагаємо достатньої кількості валідних семплів
    if (valid_samples < (s->count / 2) || sum_weights < 0.1f) {
        s->consecutive_valid = 0;
        s->quality *= 0.8f;
        return false;
    }

    float R_smoothed = sum_ratio / sum_weights;

    // === РОЗРАХУНОК SpO2 ===
    float spo2 = s->calib_a - s->calib_b * R_smoothed;

    // Реальніший діапазон SpO2
    if (spo2 < SPO2_MIN || spo2 > SPO2_MAX) {
        s->consecutive_valid = 0;
        s->quality *= 0.7f;
        return false;
    }

    // === ПЕРЕВІРКА СТАБІЛЬНОСТІ ===
    if (s->last_valid_spo2 > SPO2_MIN) {
        float spo2_change = fabsf(spo2 - s->last_valid_spo2);

        // Адаптивна перевірка зміни
        float max_change = (s->quality > 0.7f) ? 3.0f : 5.0f;
        if (spo2_change > max_change) {
            s->consecutive_valid = 0;
            s->quality *= 0.6f;
            return false;
        }

        // Плавне оновлення при низькій якості
        if (s->quality < 0.5f) {
            spo2 = 0.7f * spo2 + 0.3f * s->last_valid_spo2;
        }
    }

    // === ОНОВЛЕННЯ СТАНУ ===
    s->consecutive_valid++;
    s->last_valid_spo2 = spo2;

    // Оновлення якості з гістерезисом
    s->quality = 0.8f * s->quality + 0.2f * current_quality;
    s->quality = fmaxf(0.1f, fminf(1.0f, s->quality));

    // Вимагаємо менше послідовних вимірів при високій якості
    int required_consecutive = (s->quality > 0.7f) ? 2 : 3;

    if (s->consecutive_valid >= required_consecutive) {
        *out_spo2 = spo2;
        return true;
    }

    return false;
}

// ===== ДОДАТКОВІ ОПТИМІЗОВАНІ ФУНКЦІЇ =====
float spo2_get_signal_quality(const spo2_algo_t *s)
{
    return (s != NULL) ? s->quality : 0.0f;
}

float spo2_get_last_valid(const spo2_algo_t *s)
{
    return (s != NULL) ? s->last_valid_spo2 : 0.0f;
}

int spo2_get_consecutive_valid_count(const spo2_algo_t *s)
{
    return (s != NULL) ? s->consecutive_valid : 0;
}

void spo2_set_calibration(spo2_algo_t *s, float calib_a, float calib_b)
{
    if (s != NULL && calib_a > 0 && calib_b > 0) {
        s->calib_a = calib_a;
        s->calib_b = calib_b;
    }
}

void spo2_get_calibration(const spo2_algo_t *s, float *calib_a, float *calib_b)
{
    if (s != NULL && calib_a != NULL && calib_b != NULL) {
        *calib_a = s->calib_a;
        *calib_b = s->calib_b;
    }
}

bool spo2_is_signal_adequate(const spo2_algo_t *s, float min_quality)
{
    if (s == NULL) return false;
    return (s->quality >= min_quality && s->consecutive_valid >= 2);
}
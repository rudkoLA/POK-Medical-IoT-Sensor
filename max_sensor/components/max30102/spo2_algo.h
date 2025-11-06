#ifndef SPO2_ALGO_H
#define SPO2_ALGO_H

#include <stdbool.h>
#include <stdint.h>

#define SPO2_WINDOW 12

typedef struct {
    // Буфери для згладжування
    float ratio_window[SPO2_WINDOW];
    float ac_amplitude_window[SPO2_WINDOW];

    // Стан алгоритму
    uint32_t idx;
    uint32_t count;
    float last_valid_spo2;
    float quality;
    int consecutive_valid;

    // Калібрування
    float calib_a;
    float calib_b;

} spo2_algo_t;

// Основні функції
void spo2_init(spo2_algo_t *s);
void spo2_reset(spo2_algo_t *s);
bool spo2_update(spo2_algo_t *s, float red_ac, float red_dc, float ir_ac, float ir_dc, float *out_spo2);

// Допоміжні функції
float spo2_get_signal_quality(const spo2_algo_t *s);
float spo2_get_last_valid(const spo2_algo_t *s);
int spo2_get_consecutive_valid_count(const spo2_algo_t *s);
bool spo2_is_signal_adequate(const spo2_algo_t *s, float min_quality);

// Калібрування
void spo2_set_calibration(spo2_algo_t *s, float calib_a, float calib_b);
void spo2_get_calibration(const spo2_algo_t *s, float *calib_a, float *calib_b);

#endif // SPO2_ALGO_H
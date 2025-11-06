#ifndef HR_ALGO_H
#define HR_ALGO_H

#include <stdint.h>
#include <stdbool.h>

#define RR_BUFFER_SIZE 8

typedef struct {
    // Стан детектора
    float thresh;
    float ema_peak;
    float ema_noise;
    bool rising;
    float prev;

    // Таймінги
    int64_t last_peak_us;
    int64_t prev_peak_us;
    int refractory_ms;

    // RR-інтервали
    float rr_ms[RR_BUFFER_SIZE];
    int rr_idx;
    int rr_cnt;

    // Діагностика
    float last_peak_val;

} hr_algo_t;

// Основні функції
void hr_init(hr_algo_t *s);
void hr_reset(hr_algo_t *s);
bool hr_update(hr_algo_t *s, float x, int64_t now_us, float *out_bpm);

// Допоміжні функції
float hr_get_current_threshold(const hr_algo_t *s);
float hr_get_signal_quality(const hr_algo_t *s);
float hr_get_last_peak_value(const hr_algo_t *s);
int hr_get_rr_count(const hr_algo_t *s);
float hr_get_current_rr_interval(const hr_algo_t *s);
void hr_set_refractory_period(hr_algo_t *s, int refractory_ms);

#endif // HR_ALGO_H
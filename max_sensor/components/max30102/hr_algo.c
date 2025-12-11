#include "hr_algo.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>


#define MIN_BPM 40.0f
#define MAX_BPM 180.0f
#define MIN_RR_MS (60000.0f / MAX_BPM)
#define MAX_RR_MS (60000.0f / MIN_BPM)

#define PEAK_ALPHA 0.15f
#define NOISE_ALPHA 0.08f
#define THRESHOLD_RATIO 0.5f

#define REFRACTORY_MS 250
#define MIN_RR_COUNT 2
#define RR_BUFFER_SIZE 8

#define MIN_THRESHOLD 5.0f
#define INITIAL_THRESHOLD 30.0f

static inline bool is_valid_bpm(float bpm) {
    return (bpm >= MIN_BPM && bpm <= MAX_BPM);
}

static inline bool is_valid_rr_interval(float rr_ms) {
    return (rr_ms >= MIN_RR_MS && rr_ms <= MAX_RR_MS);
}

static inline int64_t us_to_ms(int64_t us) {
    return us / 1000;
}

void hr_init(hr_algo_t *s)
{
    if (s == NULL) return;

    memset(s, 0, sizeof(hr_algo_t));

    s->thresh = INITIAL_THRESHOLD;
    s->ema_peak = INITIAL_THRESHOLD;
    s->ema_noise = 5.0f;
    s->refractory_ms = REFRACTORY_MS;
    s->prev = 0.0f;

    for (int i = 0; i < RR_BUFFER_SIZE; i++) {
        s->rr_ms[i] = 0.0f;
    }
}

void hr_reset(hr_algo_t *s)
{
    hr_init(s);
}

static float rr_avg_bpm(const hr_algo_t *s)
{
    if (s->rr_cnt < MIN_RR_COUNT) {
        return 0.0f;
    }

    float sum = 0.0f;
    int valid_count = 0;

    for (int i = 0; i < s->rr_cnt; ++i) {
        float rr = s->rr_ms[i];
        if (rr >= MIN_RR_MS && rr <= MAX_RR_MS) {
            sum += rr;
            valid_count++;
        }
    }

    if (valid_count < MIN_RR_COUNT) {
        return 0.0f;
    }

    float mean_ms = sum / valid_count;

    if (mean_ms < 1.0f) {
        return 0.0f;
    }

    float bpm = 60000.0f / mean_ms;

    return is_valid_bpm(bpm) ? bpm : 0.0f;
}

bool hr_update(hr_algo_t *s, float x, int64_t now_us, float *out_bpm)
{
    if (s == NULL || !isfinite(x)) {
        return false;
    }

    float y = fabsf(x);
    bool new_peak = false;

    if (y > s->prev) {
        s->rising = true;
    }
    else if (s->rising && y < s->prev) {
        float peak_val = s->prev;
        int64_t dt_ms = us_to_ms(now_us - s->last_peak_us);

        if (peak_val > s->thresh * 0.7f && dt_ms > s->refractory_ms) {
            s->prev_peak_us = s->last_peak_us;
            s->last_peak_us = now_us;
            s->last_peak_val = peak_val;

            s->ema_peak = (1.0f - PEAK_ALPHA) * s->ema_peak + PEAK_ALPHA * peak_val;
            new_peak = true;

            if (s->prev_peak_us != 0) {
                float rr_ms = (float)us_to_ms(now_us - s->prev_peak_us);

                if (is_valid_rr_interval(rr_ms)) {
                    s->rr_ms[s->rr_idx] = rr_ms;
                    s->rr_idx = (s->rr_idx + 1) % RR_BUFFER_SIZE;
                    if (s->rr_cnt < RR_BUFFER_SIZE) {
                        s->rr_cnt++;
                    }
                }
            }
        }
        else {
            s->ema_noise = (1.0f - NOISE_ALPHA) * s->ema_noise + NOISE_ALPHA * peak_val;
        }

        s->rising = false;
    }

    s->thresh = THRESHOLD_RATIO * s->ema_peak + (1.0f - THRESHOLD_RATIO) * s->ema_noise;

    if (s->thresh < MIN_THRESHOLD) {
        s->thresh = MIN_THRESHOLD;
    }

    s->prev = y;

    if (new_peak && s->rr_cnt >= MIN_RR_COUNT) {
        float bpm = rr_avg_bpm(s);
        if (bpm > 0.0f) {
            if (out_bpm != NULL) {
                *out_bpm = bpm;
            }
            return true;
        }
    }

    return false;
}

float hr_get_current_threshold(const hr_algo_t *s)
{
    return (s != NULL) ? s->thresh : 0.0f;
}

float hr_get_signal_quality(const hr_algo_t *s)
{
    if (s == NULL) return 0.0f;

    float denominator = s->ema_peak + s->ema_noise;
    if (denominator < 0.1f) return 0.0f;

    return (s->ema_peak - s->ema_noise) / denominator;
}

float hr_get_last_peak_value(const hr_algo_t *s)
{
    return (s != NULL) ? s->last_peak_val : 0.0f;
}

int hr_get_rr_count(const hr_algo_t *s)
{
    return (s != NULL) ? s->rr_cnt : 0;
}

float hr_get_current_rr_interval(const hr_algo_t *s)
{
    if (s == NULL || s->rr_cnt == 0) return 0.0f;

    int last_idx = (s->rr_idx == 0) ? RR_BUFFER_SIZE - 1 : s->rr_idx - 1;
    return s->rr_ms[last_idx];
}

void hr_set_refractory_period(hr_algo_t *s, int refractory_ms)
{
    if (s != NULL && refractory_ms > 0) {
        s->refractory_ms = refractory_ms;
    }
}
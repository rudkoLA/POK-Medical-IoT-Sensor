#ifndef FILTERS_H
#define FILTERS_H

#include <stdint.h>
#include <stdbool.h>

// Константи для фільтрів
#define MA_WINDOW_AC 10
#define MA_WINDOW_DC 100
#define MA_AC_INV (1.0f / MA_WINDOW_AC)
#define MA_DC_INV (1.0f / MA_WINDOW_DC)

// Структури фільтрів
typedef struct {
    float buf[MA_WINDOW_AC];
    uint16_t idx;
    float sum;
} ma_filter_ac_t;

typedef struct {
    float buf[MA_WINDOW_DC];
    uint16_t idx;
    float sum;
} ma_filter_dc_t;

typedef struct {
    float prev_input;
    float prev_output;
    bool first_sample;
    float alpha;
    float cutoff_hz;
    float sample_rate;
} highpass_t;

typedef struct {
    highpass_t hp;
    ma_filter_ac_t ma;
    float hp_cutoff_hz;
    float sample_rate;
} bandpass_t;

// Moving Average AC фільтр
void ma_ac_init(ma_filter_ac_t *f);
float ma_ac_update(ma_filter_ac_t *f, float value);
void ma_ac_reset(ma_filter_ac_t *f);

// Moving Average DC фільтр
void ma_dc_init(ma_filter_dc_t *f);
float ma_dc_update(ma_filter_dc_t *f, float value);
void ma_dc_reset(ma_filter_dc_t *f);

// High-pass фільтр
void highpass_init(highpass_t *hp, float cutoff_hz, float sample_rate);
float highpass_update(highpass_t *hp, float input);
void highpass_reset(highpass_t *hp);

// Band-pass фільтр
void bandpass_init(bandpass_t *bp, float hp_cutoff_hz, float sample_rate);
float bandpass_update(bandpass_t *bp, float input);
void bandpass_reset(bandpass_t *bp);

// Допоміжні функції для зворотної сумісності
float highpass_filter_ir(float x);
float highpass_filter_red(float x);
void highpass_reset_ir(void);
void highpass_reset_red(void);
void filters_global_init(void);

#endif // FILTERS_H
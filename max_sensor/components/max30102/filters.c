#include "filters.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>

// ===== Moving Average Filter для AC (швидкий) =====
void ma_ac_init(ma_filter_ac_t *f)
{
    if (f == NULL) return;

    memset(f->buf, 0, sizeof(f->buf));
    f->idx = 0;
    f->sum = 0.0f;
}

float ma_ac_update(ma_filter_ac_t *f, float value)
{
    if (f == NULL) return value;

    f->sum -= f->buf[f->idx];
    f->buf[f->idx] = value;
    f->sum += value;
    f->idx = (f->idx + 1) % MA_WINDOW_AC;
    return f->sum * MA_AC_INV;
}

void ma_ac_reset(ma_filter_ac_t *f)
{
    if (f == NULL) return;
    ma_ac_init(f);
}

// ===== Moving Average Filter для DC (повільний) =====
void ma_dc_init(ma_filter_dc_t *f)
{
    if (f == NULL) return;

    memset(f->buf, 0, sizeof(f->buf));
    f->idx = 0;
    f->sum = 0.0f;
}

float ma_dc_update(ma_filter_dc_t *f, float value)
{
    if (f == NULL) return value;

    f->sum -= f->buf[f->idx];
    f->buf[f->idx] = value;
    f->sum += value;
    f->idx = (f->idx + 1) % MA_WINDOW_DC;
    return f->sum * MA_DC_INV;
}

void ma_dc_reset(ma_filter_dc_t *f)
{
    if (f == NULL) return;
    ma_dc_init(f);
}

// ===== ВДОСКОНАЛЕНИЙ High-pass фільтр =====
void highpass_init(highpass_t *hp, float cutoff_hz, float sample_rate)
{
    if (hp == NULL) return;

    hp->prev_input = 0.0f;
    hp->prev_output = 0.0f;
    hp->first_sample = true;

    // Розрахунок коефіцієнтів
    if (cutoff_hz <= 0.0f || sample_rate <= 0.0f) {
        // Значення за замовчуванням
        cutoff_hz = 0.5f;
        sample_rate = 100.0f;
    }

    float rc = 1.0f / (2.0f * (float)M_PI * cutoff_hz);
    float dt = 1.0f / sample_rate;
    hp->alpha = rc / (rc + dt);
    hp->cutoff_hz = cutoff_hz;
    hp->sample_rate = sample_rate;
}

float highpass_update(highpass_t *hp, float input)
{
    if (hp == NULL) return input;

    if (hp->first_sample) {
        hp->first_sample = false;
        hp->prev_input = input;
        hp->prev_output = 0.0f;
        return 0.0f;
    }

    // Виправлена формула high-pass фільтра
    float output = hp->alpha * (hp->prev_output + input - hp->prev_input);

    hp->prev_input = input;
    hp->prev_output = output;

    return output;
}

void highpass_reset(highpass_t *hp)
{
    if (hp == NULL) return;
    hp->first_sample = true;
    hp->prev_input = 0.0f;
    hp->prev_output = 0.0f;
}

// ===== Band-pass фільтр для пульсу =====
void bandpass_init(bandpass_t *bp, float hp_cutoff_hz, float sample_rate)
{
    if (bp == NULL) return;

    highpass_init(&bp->hp, hp_cutoff_hz, sample_rate);
    ma_ac_init(&bp->ma);
    bp->hp_cutoff_hz = hp_cutoff_hz;
    bp->sample_rate = sample_rate;
}

float bandpass_update(bandpass_t *bp, float input)
{
    if (bp == NULL) return input;

    // High-pass -> Moving Average (low-pass)
    float hp_out = highpass_update(&bp->hp, input);
    return ma_ac_update(&bp->ma, hp_out);
}

void bandpass_reset(bandpass_t *bp)
{
    if (bp == NULL) return;
    highpass_reset(&bp->hp);
    ma_ac_reset(&bp->ma);
}

// ===== Допоміжні функції (залишаються для зворотної сумісності) =====
static highpass_t hp_ir = {0};
static highpass_t hp_red = {0};

float highpass_filter_ir(float x)
{
    return highpass_update(&hp_ir, x);
}

float highpass_filter_red(float x)
{
    return highpass_update(&hp_red, x);
}

void highpass_reset_ir(void)
{
    highpass_reset(&hp_ir);
}

void highpass_reset_red(void)
{
    highpass_reset(&hp_red);
}

// Ініціалізація глобальних фільтрів (викликати один раз на початку)
void filters_global_init(void)
{
    highpass_init(&hp_ir, 0.5f, 100.0f);
    highpass_init(&hp_red, 0.5f, 100.0f);
}
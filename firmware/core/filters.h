#ifndef BATTERY_FILTERS_H
#define BATTERY_FILTERS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_MAX_SIZE 16

typedef struct {
    float buf[FW_MAX_SIZE];
    uint8_t size;
    uint8_t index;
    uint8_t count;
} float_window_t;

bool bat_is_finite(float value);
float bat_clampf(float value, float low, float high);
float bat_absf(float value);

void fw_init(float_window_t *window, uint8_t size);
void fw_reset(float_window_t *window);
void fw_push(float_window_t *window, float value);
uint8_t fw_count(const float_window_t *window);
float fw_mean(const float_window_t *window);
float fw_min(const float_window_t *window);
float fw_max(const float_window_t *window);
float fw_range(const float_window_t *window);
float fw_stddev(const float_window_t *window);
float fw_median(const float_window_t *window);
float fw_last(const float_window_t *window);

float ema_update(float *state, float sample, float alpha);
float rate_limit(float previous, float target, float max_delta);

#ifdef __cplusplus
}
#endif

#endif

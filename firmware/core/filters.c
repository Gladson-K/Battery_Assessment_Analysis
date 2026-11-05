#include "filters.h"

#include <math.h>
#include <string.h>

bool bat_is_finite(float value)
{
    return isfinite(value) != 0;
}

float bat_clampf(float value, float low, float high)
{
    if (!bat_is_finite(value)) {
        return low;
    }
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

float bat_absf(float value)
{
    return value < 0.0f ? -value : value;
}

void fw_init(float_window_t *window, uint8_t size)
{
    if (window == 0) {
        return;
    }
    if (size == 0 || size > FW_MAX_SIZE) {
        size = FW_MAX_SIZE;
    }
    memset(window, 0, sizeof(*window));
    window->size = size;
}

void fw_reset(float_window_t *window)
{
    if (window == 0) {
        return;
    }
    uint8_t size = window->size;
    memset(window, 0, sizeof(*window));
    window->size = size;
}

void fw_push(float_window_t *window, float value)
{
    if (window == 0 || window->size == 0) {
        return;
    }
    if (!bat_is_finite(value)) {
        return;
    }
    window->buf[window->index] = value;
    window->index = (uint8_t)((window->index + 1u) % window->size);
    if (window->count < window->size) {
        window->count++;
    }
}

uint8_t fw_count(const float_window_t *window)
{
    return window == 0 ? 0 : window->count;
}

float fw_mean(const float_window_t *window)
{
    if (window == 0 || window->count == 0) {
        return NAN;
    }
    float sum = 0.0f;
    for (uint8_t i = 0; i < window->count; i++) {
        sum += window->buf[i];
    }
    return sum / (float)window->count;
}

float fw_min(const float_window_t *window)
{
    if (window == 0 || window->count == 0) {
        return NAN;
    }
    float value = window->buf[0];
    for (uint8_t i = 1; i < window->count; i++) {
        if (window->buf[i] < value) {
            value = window->buf[i];
        }
    }
    return value;
}

float fw_max(const float_window_t *window)
{
    if (window == 0 || window->count == 0) {
        return NAN;
    }
    float value = window->buf[0];
    for (uint8_t i = 1; i < window->count; i++) {
        if (window->buf[i] > value) {
            value = window->buf[i];
        }
    }
    return value;
}

float fw_range(const float_window_t *window)
{
    if (window == 0 || window->count == 0) {
        return NAN;
    }
    return fw_max(window) - fw_min(window);
}

float fw_stddev(const float_window_t *window)
{
    if (window == 0 || window->count < 2) {
        return 0.0f;
    }
    float mean = fw_mean(window);
    float sum = 0.0f;
    for (uint8_t i = 0; i < window->count; i++) {
        float delta = window->buf[i] - mean;
        sum += delta * delta;
    }
    return sqrtf(sum / (float)(window->count - 1u));
}

float fw_median(const float_window_t *window)
{
    if (window == 0 || window->count == 0) {
        return NAN;
    }
    float copy[FW_MAX_SIZE];
    uint8_t count = window->count;
    memcpy(copy, window->buf, sizeof(float) * count);
    for (uint8_t i = 1; i < count; i++) {
        float key = copy[i];
        int16_t j = (int16_t)i - 1;
        while (j >= 0 && copy[j] > key) {
            copy[j + 1] = copy[j];
            j--;
        }
        copy[j + 1] = key;
    }
    if ((count & 1u) != 0u) {
        return copy[count / 2u];
    }
    return 0.5f * (copy[count / 2u - 1u] + copy[count / 2u]);
}

float fw_last(const float_window_t *window)
{
    if (window == 0 || window->count == 0) {
        return NAN;
    }
    uint8_t last = (uint8_t)((window->index + window->size - 1u) % window->size);
    return window->buf[last];
}

float ema_update(float *state, float sample, float alpha)
{
    if (state == 0) {
        return sample;
    }
    alpha = bat_clampf(alpha, 0.0f, 1.0f);
    if (!bat_is_finite(*state) || !bat_is_finite(sample)) {
        *state = sample;
        return sample;
    }
    *state = (*state) + alpha * (sample - (*state));
    return *state;
}

float rate_limit(float previous, float target, float max_delta)
{
    if (!bat_is_finite(previous) || !bat_is_finite(target)) {
        return target;
    }
    if (max_delta < 0.0f) {
        max_delta = -max_delta;
    }
    float delta = target - previous;
    if (delta > max_delta) {
        return previous + max_delta;
    }
    if (delta < -max_delta) {
        return previous - max_delta;
    }
    return target;
}

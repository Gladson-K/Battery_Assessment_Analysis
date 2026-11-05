#include "feature_builder.h"

#include <math.h>
#include <string.h>

static bool temp_status_is_valid(uint8_t status)
{
    return status == SENSOR_STATUS_OK || status == SENSOR_STATUS_STUCK ||
           status == SENSOR_STATUS_NOISY;
}

void fb_default_config(fb_config_t *config)
{
    if (config == 0) {
        return;
    }
    config->window_size = 8;
    config->temp_alpha = 0.35f;
    config->voltage_alpha = 0.30f;
    config->current_alpha = 0.30f;
    config->rate_alpha = 0.25f;
    config->stuck_range_c = 0.02f;
    config->stuck_global_spread_c = 0.50f;
}

bool fb_init(fb_t *builder, uint8_t num_temp, const fb_config_t *config, uint32_t now_ms)
{
    if (builder == 0) {
        return false;
    }
    if (num_temp == 0 || num_temp > BAT_MAX_TEMP_SENSORS) {
        return false;
    }
    memset(builder, 0, sizeof(*builder));
    if (config != 0) {
        builder->config = *config;
    } else {
        fb_default_config(&builder->config);
    }
    if (builder->config.window_size == 0 || builder->config.window_size > FW_MAX_SIZE) {
        builder->config.window_size = FW_MAX_SIZE;
    }
    builder->num_temp = num_temp;
    builder->start_ms = now_ms;
    builder->prev_ms = now_ms;
    builder->voltage_filtered = NAN;
    builder->current_filtered = NAN;
    builder->voltage_previous = NAN;
    builder->current_previous = NAN;
    builder->resistance_filtered = NAN;
    builder->temp_rate_filtered = 0.0f;
    builder->voltage_rate_filtered = 0.0f;
    builder->current_rate_filtered = 0.0f;
    for (uint8_t i = 0; i < BAT_MAX_TEMP_SENSORS; i++) {
        fw_init(&builder->temp_window[i], builder->config.window_size);
        builder->temp_filtered[i] = NAN;
        builder->temp_previous[i] = NAN;
    }
    builder->initialized = 1;
    return true;
}

void fb_reset(fb_t *builder, uint32_t now_ms)
{
    if (builder == 0 || !builder->initialized) {
        return;
    }
    uint8_t num_temp = builder->num_temp;
    fb_config_t config = builder->config;
    fb_init(builder, num_temp, &config, now_ms);
}

static float compute_rate(float current, float previous, float dt_s, float *state, float alpha)
{
    if (!bat_is_finite(current) || !bat_is_finite(previous) || dt_s <= 0.0f || state == 0) {
        return state == 0 ? 0.0f : *state;
    }
    float raw_rate = (current - previous) / dt_s;
    return ema_update(state, raw_rate, alpha);
}

void fb_update(fb_t *builder, const bat_raw_sample_t *raw, bat_features_t *features, uint32_t now_ms)
{
    if (builder == 0 || raw == 0 || features == 0 || !builder->initialized) {
        return;
    }

    memset(features, 0, sizeof(*features));
    features->num_temp = builder->num_temp;

    uint32_t dt_ms = now_ms >= builder->prev_ms ? (now_ms - builder->prev_ms) : 0u;
    float dt_s = (float)dt_ms / 1000.0f;
    if (dt_s < 0.001f) {
        dt_s = 0.001f;
    } else if (dt_s > 10.0f) {
        dt_s = 10.0f;
    }

    uint16_t valid_mask = 0u;
    uint16_t stuck_mask = 0u;
    uint8_t valid_count = 0;
    float t_min = NAN;
    float t_max = NAN;
    float t_sum = 0.0f;
    float max_positive_rate = 0.0f;

    for (uint8_t i = 0; i < builder->num_temp; i++) {
        bool valid = (raw->temp_valid_mask & (uint16_t)(1u << i)) != 0u;
        valid = valid && temp_status_is_valid(raw->temp_status[i]);
        float sample = raw->temp_c[i];
        valid = valid && bat_is_finite(sample);
        valid = valid && sample >= -55.0f && sample <= 150.0f;

        if (!valid) {
            features->temp_c[i] = NAN;
            builder->temp_filtered[i] = NAN;
            builder->temp_previous[i] = NAN;
            fw_reset(&builder->temp_window[i]);
            continue;
        }

        fw_push(&builder->temp_window[i], sample);
        float median = fw_median(&builder->temp_window[i]);
        float filtered = ema_update(&builder->temp_filtered[i], median, builder->config.temp_alpha);
        features->temp_c[i] = filtered;
        valid_mask |= (uint16_t)(1u << i);
        valid_count++;

        float rate = 0.0f;
        if (bat_is_finite(builder->temp_previous[i])) {
            rate = (filtered - builder->temp_previous[i]) / dt_s;
        }
        builder->temp_previous[i] = filtered;
        if (rate > max_positive_rate) {
            max_positive_rate = rate;
        }

        if (fw_count(&builder->temp_window[i]) >= builder->config.window_size) {
            float window_range = fw_range(&builder->temp_window[i]);
            if (bat_is_finite(window_range) && window_range < builder->config.stuck_range_c) {
                stuck_mask |= (uint16_t)(1u << i);
            }
        }
        if (raw->temp_status[i] == SENSOR_STATUS_STUCK) {
            stuck_mask |= (uint16_t)(1u << i);
        }

        if (!bat_is_finite(t_min) || filtered < t_min) {
            t_min = filtered;
        }
        if (!bat_is_finite(t_max) || filtered > t_max) {
            t_max = filtered;
        }
        t_sum += filtered;
    }

    features->temp_valid_mask = valid_mask;
    features->temp_stuck_mask = stuck_mask;
    features->t_min = t_min;
    features->t_max = t_max;
    features->t_avg = valid_count > 0 ? (t_sum / (float)valid_count) : NAN;
    features->t_spread = valid_count > 1 ? (t_max - t_min) : 0.0f;

    if (valid_count > 1 && bat_is_finite(features->t_spread) &&
        features->t_spread > builder->config.stuck_global_spread_c) {
        for (uint8_t i = 0; i < builder->num_temp; i++) {
            if ((stuck_mask & (uint16_t)(1u << i)) != 0u) {
                features->temp_stuck_mask |= (uint16_t)(1u << i);
            }
        }
    }

    builder->temp_rate_filtered = ema_update(&builder->temp_rate_filtered, max_positive_rate,
                                             builder->config.rate_alpha);
    features->dt_dt_c_per_s = builder->temp_rate_filtered;

    float voltage = raw->voltage_v;
    float current = raw->current_a;
    if (!bat_is_finite(voltage)) {
        voltage = NAN;
    }
    if (!bat_is_finite(current)) {
        current = NAN;
    }

    float v_previous = builder->voltage_previous;
    float i_previous = builder->current_previous;
    float v_filt = ema_update(&builder->voltage_filtered, voltage, builder->config.voltage_alpha);
    float i_filt = ema_update(&builder->current_filtered, current, builder->config.current_alpha);
    features->voltage_v = v_filt;
    features->current_a = i_filt;

    float dv_rate = compute_rate(v_filt, v_previous, dt_s, &builder->voltage_rate_filtered,
                                 builder->config.rate_alpha);
    float di_rate = compute_rate(i_filt, i_previous, dt_s, &builder->current_rate_filtered,
                                 builder->config.rate_alpha);
    features->dv_dt_v_per_s = dv_rate;
    features->di_dt_a_per_s = di_rate;

    features->power_w = (bat_is_finite(v_filt) && bat_is_finite(i_filt)) ? v_filt * i_filt : NAN;

    if (bat_is_finite(v_filt) && bat_is_finite(i_filt) && bat_is_finite(v_previous) &&
        bat_is_finite(i_previous) && bat_absf(i_filt) >= 0.10f) {
        float v_delta = v_filt - v_previous;
        float i_delta = i_filt - i_previous;
        if (bat_absf(i_delta) >= 0.05f) {
            float resistance = bat_absf(v_delta / i_delta);
            resistance = bat_clampf(resistance, 0.0f, 1000.0f);
            builder->resistance_filtered = ema_update(&builder->resistance_filtered, resistance, 0.20f);
        }
    }
    features->resistance_ohm = builder->resistance_filtered;

    builder->voltage_previous = v_filt;
    builder->current_previous = i_filt;

    float rain = raw->rain_analog;
    if (!bat_is_finite(rain)) {
        rain = 0.0f;
    }
    features->rain_pct = bat_clampf((rain / 1023.0f) * 100.0f, 0.0f, 100.0f);
    features->rain_wet = (raw->rain_digital_dry == 0u ||
                          features->rain_pct >= 60.0f) ? 1u : 0u;

    features->humidity_valid = raw->humidity_valid;
    features->humidity_pct = raw->humidity_valid ? raw->humidity_pct : NAN;

    builder->sample_count++;
    features->sample_count = (uint16_t)builder->sample_count;
    features->age_ms = now_ms >= builder->start_ms ? (now_ms - builder->start_ms) : 0u;
    builder->prev_ms = now_ms;
}

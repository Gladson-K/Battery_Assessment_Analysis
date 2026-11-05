#ifndef BATTERY_FEATURE_BUILDER_H
#define BATTERY_FEATURE_BUILDER_H

#include "battery_types.h"
#include "filters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t window_size;
    float temp_alpha;
    float voltage_alpha;
    float current_alpha;
    float rate_alpha;
    float stuck_range_c;
    float stuck_global_spread_c;
} fb_config_t;

typedef struct {
    fb_config_t config;
    uint8_t num_temp;
    uint8_t initialized;
    uint32_t start_ms;
    uint32_t prev_ms;
    uint32_t sample_count;
    float_window_t temp_window[BAT_MAX_TEMP_SENSORS];
    float temp_filtered[BAT_MAX_TEMP_SENSORS];
    float temp_previous[BAT_MAX_TEMP_SENSORS];
    float voltage_filtered;
    float current_filtered;
    float voltage_previous;
    float current_previous;
    float temp_rate_filtered;
    float voltage_rate_filtered;
    float current_rate_filtered;
    float resistance_filtered;
} fb_t;

void fb_default_config(fb_config_t *config);
bool fb_init(fb_t *builder, uint8_t num_temp, const fb_config_t *config, uint32_t now_ms);
void fb_reset(fb_t *builder, uint32_t now_ms);
void fb_update(fb_t *builder, const bat_raw_sample_t *raw, bat_features_t *features, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif

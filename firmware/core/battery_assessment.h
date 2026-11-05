#ifndef BATTERY_ASSESSMENT_H
#define BATTERY_ASSESSMENT_H

#include "battery_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t active;
    uint32_t since_ms;
} ba_debounce_t;

typedef struct {
    ba_config_t config;
    bat_state_t state;
    uint32_t active_faults;
    uint32_t latched_faults;
    float rule_score;
    float ml_score;
    float risk_score;
    float ml_p_unsafe;
    uint8_t ml_label;
    uint8_t ml_valid;
    uint8_t ml_low_confidence;
    uint8_t initialized;
    uint8_t config_valid;
    uint8_t reset_requested;
    uint8_t trip_latched;
    uint32_t init_ms;
    uint32_t last_ms;
    ba_debounce_t warn_db;
    ba_debounce_t crit_db;
    ba_debounce_t trip_db;
    ba_debounce_t recover_warn_db;
    ba_debounce_t recover_crit_db;
} ba_t;

bool ba_validate_config(const ba_config_t *config);
bool ba_init(ba_t *assessment, const ba_config_t *config, uint32_t now_ms);
void ba_manual_reset(ba_t *assessment);
const char *ba_fault_name(uint32_t fault);
const char *ba_state_name(bat_state_t state);
void ba_update(ba_t *assessment, const bat_raw_sample_t *raw, const bat_features_t *features,
               const rf_result_t *ml, uint32_t now_ms, bat_assessment_t *out);

#ifdef __cplusplus
}
#endif

#endif

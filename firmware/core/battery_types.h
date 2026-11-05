#ifndef BATTERY_TYPES_H
#define BATTERY_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BAT_MAX_TEMP_SENSORS 16
#define BAT_ML_FEATURE_COUNT 12
#define BAT_REASON_LEN 64
#define BAT_FEATURE_INDEX_T1 0
#define BAT_FEATURE_INDEX_VOLTAGE 8
#define BAT_FEATURE_INDEX_CURRENT 9
#define BAT_FEATURE_INDEX_RAIN_ANALOG 10
#define BAT_FEATURE_INDEX_RAIN_DIGITAL 11

typedef enum {
    BAT_STATE_INIT = 0,
    BAT_STATE_NORMAL = 1,
    BAT_STATE_WARNING = 2,
    BAT_STATE_CRITICAL = 3,
    BAT_STATE_TRIPPED = 4
} bat_state_t;

typedef enum {
    BAT_LABEL_SAFE = 0,
    BAT_LABEL_UNSAFE = 1
} bat_label_t;

typedef enum {
    SENSOR_STATUS_OK = 0,
    SENSOR_STATUS_OPEN = 1,
    SENSOR_STATUS_SHORT = 2,
    SENSOR_STATUS_RAIL = 3,
    SENSOR_STATUS_OUT_OF_RANGE = 4,
    SENSOR_STATUS_STUCK = 5,
    SENSOR_STATUS_NOISY = 6,
    SENSOR_STATUS_ABSENT = 7,
    SENSOR_STATUS_CONFLICT = 8
} sensor_status_t;

typedef enum {
    BAT_PROFILE_BENCH_6V = 0,
    BAT_PROFILE_LEAD_12V = 1,
    BAT_PROFILE_LFP_24V = 2,
    BAT_PROFILE_LFP_48V = 3,
    BAT_PROFILE_COLD_CHAIN = 4,
    BAT_PROFILE_CUSTOM = 5
} bat_profile_t;

#define BAT_FAULT_TEMP_WARN           (1u << 0)
#define BAT_FAULT_TEMP_CRITICAL       (1u << 1)
#define BAT_FAULT_TEMP_TRIP           (1u << 2)
#define BAT_FAULT_TEMP_SPREAD         (1u << 3)
#define BAT_FAULT_TEMP_RATE           (1u << 4)
#define BAT_FAULT_TEMP_SENSOR_OPEN    (1u << 5)
#define BAT_FAULT_TEMP_SENSOR_SHORT   (1u << 6)
#define BAT_FAULT_TEMP_SENSOR_STUCK   (1u << 7)
#define BAT_FAULT_VOLT_HIGH           (1u << 8)
#define BAT_FAULT_VOLT_LOW            (1u << 9)
#define BAT_FAULT_VOLT_REVERSE        (1u << 10)
#define BAT_FAULT_VOLT_SENSOR         (1u << 11)
#define BAT_FAULT_OVERCURRENT         (1u << 12)
#define BAT_FAULT_CURRENT_SENSOR      (1u << 13)
#define BAT_FAULT_CURRENT_DRIFT       (1u << 14)
#define BAT_FAULT_RAIN_WET            (1u << 15)
#define BAT_FAULT_RAIN_CONFLICT       (1u << 16)
#define BAT_FAULT_ML_UNSAFE           (1u << 17)
#define BAT_FAULT_ML_LOW_CONFIDENCE   (1u << 18)
#define BAT_FAULT_COMMS_TIMEOUT       (1u << 19)
#define BAT_FAULT_CONFIG_INVALID      (1u << 20)
#define BAT_FAULT_WATCHDOG_RESET      (1u << 21)
#define BAT_FAULT_ESTOP               (1u << 22)
#define BAT_FAULT_FAN_FAULT           (1u << 23)
#define BAT_FAULT_HUMIDITY_HIGH       (1u << 24)
#define BAT_FAULT_RESISTANCE_HIGH     (1u << 25)
#define BAT_FAULT_SELF_TEST           (1u << 26)
#define BAT_FAULT_SENSOR_PLAUSIBILITY (1u << 27)
#define BAT_FAULT_TRIP_LATCHED        (1u << 28)
#define BAT_FAULT_PROFILE_MISMATCH    (1u << 29)
#define BAT_FAULT_UNKNOWN             (1u << 31)

typedef struct {
    uint8_t num_temp_sensors;
    float temp_warn_c;
    float temp_crit_c;
    float temp_trip_c;
    float temp_low_warn_c;
    float temp_low_crit_c;
    float temp_min_plausible_c;
    float temp_max_plausible_c;
    float temp_spread_warn_c;
    float temp_spread_crit_c;
    float temp_rate_warn_c_s;
    float temp_rate_crit_c_s;
    float volt_warn_high_v;
    float volt_crit_high_v;
    float volt_warn_low_v;
    float volt_crit_low_v;
    float volt_abs_max_v;
    float volt_reverse_v;
    float current_warn_a;
    float current_crit_a;
    float current_abs_max_a;
    float current_zero_tolerance_a;
    float resistance_warn_ohm;
    float resistance_crit_ohm;
    float rain_wet_warn_pct;
    float rain_wet_crit_pct;
    float humidity_warn_pct;
    float humidity_crit_pct;
    float ml_unsafe_threshold;
    float ml_min_confidence;
    float score_warn;
    float score_crit;
    float score_trip;
    float score_warn_exit;
    float score_crit_exit;
    uint32_t warn_debounce_ms;
    uint32_t crit_debounce_ms;
    uint32_t trip_debounce_ms;
    uint32_t recovery_ms;
    uint32_t warmup_ms;
    uint32_t comms_timeout_ms;
    uint32_t stuck_window_ms;
    uint8_t ml_enabled;
    uint8_t domain_gate_enabled;
    float domain_voltage_min_v;
    float domain_voltage_max_v;
    float domain_current_min_a;
    float domain_current_max_a;
} ba_config_t;

typedef struct {
    float temp_c[BAT_MAX_TEMP_SENSORS];
    uint16_t temp_valid_mask;
    uint8_t temp_status[BAT_MAX_TEMP_SENSORS];
    float voltage_v;
    float current_a;
    float rain_analog;
    uint8_t rain_digital_dry;
    float humidity_pct;
    uint8_t humidity_valid;
    uint8_t estop_active;
    uint8_t comms_ok;
    uint8_t self_test_ok;
    uint8_t channel_count;
    uint32_t external_faults;
    uint32_t timestamp_ms;
} bat_raw_sample_t;

typedef struct {
    uint8_t num_temp;
    float temp_c[BAT_MAX_TEMP_SENSORS];
    uint16_t temp_valid_mask;
    uint16_t temp_stuck_mask;
    float t_min;
    float t_max;
    float t_avg;
    float t_spread;
    float dt_dt_c_per_s;
    float voltage_v;
    float current_a;
    float power_w;
    float resistance_ohm;
    float dv_dt_v_per_s;
    float di_dt_a_per_s;
    float rain_pct;
    uint8_t rain_wet;
    float humidity_pct;
    uint8_t humidity_valid;
    uint16_t sample_count;
    uint32_t age_ms;
} bat_features_t;

typedef struct {
    float p_unsafe;
    uint8_t label;
    uint8_t valid;
    uint8_t low_confidence;
} rf_result_t;

typedef struct {
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
    uint8_t relay_allowed;
    uint8_t buzzer_on;
    uint8_t trip_latched;
    const char *reason;
} bat_assessment_t;

#ifdef __cplusplus
}
#endif

#endif

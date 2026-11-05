#include "battery_assessment.h"

#include "filters.h"

#include <math.h>
#include <string.h>

#define HARD_TRIP_MASK (BAT_FAULT_TEMP_TRIP | BAT_FAULT_VOLT_REVERSE | BAT_FAULT_OVERCURRENT | \
                        BAT_FAULT_ESTOP | BAT_FAULT_CONFIG_INVALID)

static bool db_update(ba_debounce_t *db, bool condition, uint32_t now_ms, uint32_t debounce_ms)
{
    if (db == 0) {
        return false;
    }
    if (!condition) {
        db->active = 0;
        db->since_ms = now_ms;
        return false;
    }
    if (!db->active) {
        db->active = 1;
        db->since_ms = now_ms;
    }
    return (uint32_t)(now_ms - db->since_ms) >= debounce_ms;
}

static float risk_high(float value, float warn, float crit)
{
    if (!bat_is_finite(value) || !bat_is_finite(warn) || !bat_is_finite(crit) || crit <= warn) {
        return 0.0f;
    }
    if (value <= 0.0f) {
        return 0.0f;
    }
    if (value < warn) {
        return bat_clampf(50.0f * (value / warn), 0.0f, 50.0f);
    }
    if (value < crit) {
        return bat_clampf(50.0f + 50.0f * ((value - warn) / (crit - warn)), 50.0f, 100.0f);
    }
    return 100.0f;
}

static float risk_low(float value, float warn, float crit)
{
    if (!bat_is_finite(value) || !bat_is_finite(warn) || !bat_is_finite(crit) || warn <= crit) {
        return 0.0f;
    }
    if (value >= warn) {
        return 0.0f;
    }
    if (value > crit) {
        return bat_clampf(50.0f * ((warn - value) / (warn - crit)), 0.0f, 50.0f);
    }
    return 100.0f;
}

static float maxf_local(float a, float b)
{
    return a > b ? a : b;
}

bool ba_validate_config(const ba_config_t *c)
{
    if (c == 0) {
        return false;
    }
    if (c->num_temp_sensors == 0 || c->num_temp_sensors > BAT_MAX_TEMP_SENSORS) {
        return false;
    }
    if (!(c->temp_trip_c > c->temp_crit_c && c->temp_crit_c > c->temp_warn_c)) {
        return false;
    }
    if (!(c->temp_low_warn_c > c->temp_low_crit_c)) {
        return false;
    }
    if (!(c->temp_spread_crit_c > c->temp_spread_warn_c)) {
        return false;
    }
    if (!(c->temp_rate_crit_c_s > c->temp_rate_warn_c_s)) {
        return false;
    }
    if (!(c->volt_crit_high_v > c->volt_warn_high_v && c->volt_warn_high_v > c->volt_warn_low_v &&
          c->volt_warn_low_v > c->volt_crit_low_v)) {
        return false;
    }
    if (!(c->volt_abs_max_v > c->volt_crit_high_v) || !(c->volt_reverse_v < 0.0f)) {
        return false;
    }
    if (!(c->current_crit_a > c->current_warn_a && c->current_warn_a > 0.0f)) {
        return false;
    }
    if (!(c->current_abs_max_a > c->current_crit_a)) {
        return false;
    }
    if (!(c->rain_wet_crit_pct > c->rain_wet_warn_pct)) {
        return false;
    }
    if (!(c->humidity_crit_pct > c->humidity_warn_pct)) {
        return false;
    }
    if (!(c->score_trip > c->score_crit && c->score_crit > c->score_warn &&
          c->score_warn > c->score_warn_exit && c->score_warn_exit > 0.0f)) {
        return false;
    }
    if (!(c->score_crit_exit < c->score_crit && c->score_crit_exit > c->score_warn_exit)) {
        return false;
    }
    if (c->trip_debounce_ms == 0u || c->warn_debounce_ms == 0u || c->crit_debounce_ms == 0u) {
        return false;
    }
    if (c->ml_enabled != 0u) {
        if (!(c->domain_voltage_max_v > c->domain_voltage_min_v)) {
            return false;
        }
        if (!(c->domain_current_max_a > c->domain_current_min_a)) {
            return false;
        }
        if (c->ml_unsafe_threshold <= 0.0f || c->ml_unsafe_threshold >= 1.0f) {
            return false;
        }
        if (c->ml_min_confidence <= 0.0f || c->ml_min_confidence >= 1.0f) {
            return false;
        }
    }
    return true;
}

static void reset_output(ba_t *a, bat_assessment_t *out, const char *reason)
{
    if (out == 0) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->state = a != 0 ? a->state : BAT_STATE_TRIPPED;
    out->active_faults = a != 0 ? a->active_faults : BAT_FAULT_CONFIG_INVALID;
    out->latched_faults = a != 0 ? a->latched_faults : BAT_FAULT_CONFIG_INVALID;
    out->risk_score = 100.0f;
    out->rule_score = 100.0f;
    out->ml_score = -1.0f;
    out->reason = reason;
    out->relay_allowed = 0;
    out->buzzer_on = 1;
    out->trip_latched = a != 0 ? a->trip_latched : 1u;
}

bool ba_init(ba_t *assessment, const ba_config_t *config, uint32_t now_ms)
{
    if (assessment == 0) {
        return false;
    }
    memset(assessment, 0, sizeof(*assessment));
    assessment->init_ms = now_ms;
    assessment->last_ms = now_ms;
    assessment->initialized = 1;
    if (!ba_validate_config(config)) {
        assessment->config_valid = 0;
        assessment->state = BAT_STATE_TRIPPED;
        assessment->active_faults = BAT_FAULT_CONFIG_INVALID;
        assessment->latched_faults = BAT_FAULT_CONFIG_INVALID | BAT_FAULT_TRIP_LATCHED;
        assessment->trip_latched = 1;
        assessment->risk_score = 100.0f;
        assessment->rule_score = 100.0f;
        assessment->ml_score = -1.0f;
        return false;
    }
    assessment->config = *config;
    assessment->config_valid = 1;
    assessment->state = BAT_STATE_INIT;
    assessment->ml_score = -1.0f;
    return true;
}

void ba_manual_reset(ba_t *assessment)
{
    if (assessment != 0) {
        assessment->reset_requested = 1;
    }
}

const char *ba_state_name(bat_state_t state)
{
    switch (state) {
    case BAT_STATE_INIT:
        return "INIT";
    case BAT_STATE_NORMAL:
        return "NORMAL";
    case BAT_STATE_WARNING:
        return "WARNING";
    case BAT_STATE_CRITICAL:
        return "CRITICAL";
    case BAT_STATE_TRIPPED:
        return "TRIPPED";
    default:
        return "UNKNOWN";
    }
}

const char *ba_fault_name(uint32_t fault)
{
    switch (fault) {
    case BAT_FAULT_TEMP_WARN:
        return "temperature_warning";
    case BAT_FAULT_TEMP_CRITICAL:
        return "temperature_critical";
    case BAT_FAULT_TEMP_TRIP:
        return "temperature_trip";
    case BAT_FAULT_TEMP_SPREAD:
        return "thermal_imbalance";
    case BAT_FAULT_TEMP_RATE:
        return "thermal_rate";
    case BAT_FAULT_TEMP_SENSOR_OPEN:
        return "temperature_sensor_open";
    case BAT_FAULT_TEMP_SENSOR_SHORT:
        return "temperature_sensor_short";
    case BAT_FAULT_TEMP_SENSOR_STUCK:
        return "temperature_sensor_stuck";
    case BAT_FAULT_VOLT_HIGH:
        return "voltage_high";
    case BAT_FAULT_VOLT_LOW:
        return "voltage_low";
    case BAT_FAULT_VOLT_REVERSE:
        return "reverse_polarity";
    case BAT_FAULT_VOLT_SENSOR:
        return "voltage_sensor_fault";
    case BAT_FAULT_OVERCURRENT:
        return "overcurrent";
    case BAT_FAULT_CURRENT_SENSOR:
        return "current_sensor_fault";
    case BAT_FAULT_CURRENT_DRIFT:
        return "current_offset_drift";
    case BAT_FAULT_RAIN_WET:
        return "moisture_detected";
    case BAT_FAULT_RAIN_CONFLICT:
        return "rain_sensor_conflict";
    case BAT_FAULT_ML_UNSAFE:
        return "ml_unsafe";
    case BAT_FAULT_ML_LOW_CONFIDENCE:
        return "ml_low_confidence";
    case BAT_FAULT_COMMS_TIMEOUT:
        return "comms_timeout";
    case BAT_FAULT_CONFIG_INVALID:
        return "invalid_configuration";
    case BAT_FAULT_WATCHDOG_RESET:
        return "watchdog_reset";
    case BAT_FAULT_ESTOP:
        return "emergency_stop";
    case BAT_FAULT_FAN_FAULT:
        return "fan_fault";
    case BAT_FAULT_HUMIDITY_HIGH:
        return "humidity_high";
    case BAT_FAULT_RESISTANCE_HIGH:
        return "resistance_high";
    case BAT_FAULT_SELF_TEST:
        return "self_test_failed";
    case BAT_FAULT_SENSOR_PLAUSIBILITY:
        return "sensor_plausibility";
    case BAT_FAULT_TRIP_LATCHED:
        return "trip_latched";
    case BAT_FAULT_PROFILE_MISMATCH:
        return "profile_mismatch";
    default:
        return "none";
    }
}

static const uint32_t FAULT_PRIORITY[] = {
    BAT_FAULT_CONFIG_INVALID,
    BAT_FAULT_ESTOP,
    BAT_FAULT_TEMP_TRIP,
    BAT_FAULT_OVERCURRENT,
    BAT_FAULT_VOLT_REVERSE,
    BAT_FAULT_TEMP_SENSOR_OPEN,
    BAT_FAULT_TEMP_SENSOR_SHORT,
    BAT_FAULT_SENSOR_PLAUSIBILITY,
    BAT_FAULT_TEMP_CRITICAL,
    BAT_FAULT_VOLT_HIGH,
    BAT_FAULT_VOLT_LOW,
    BAT_FAULT_HUMIDITY_HIGH,
    BAT_FAULT_TEMP_SPREAD,
    BAT_FAULT_TEMP_RATE,
    BAT_FAULT_TEMP_WARN,
    BAT_FAULT_RAIN_WET,
    BAT_FAULT_ML_UNSAFE,
    BAT_FAULT_RESISTANCE_HIGH,
    BAT_FAULT_TEMP_SENSOR_STUCK,
    BAT_FAULT_RAIN_CONFLICT,
    BAT_FAULT_COMMS_TIMEOUT,
    BAT_FAULT_VOLT_SENSOR,
    BAT_FAULT_CURRENT_SENSOR,
    BAT_FAULT_SELF_TEST,
    BAT_FAULT_TRIP_LATCHED,
    BAT_FAULT_UNKNOWN
};

static const char *dominant_fault_name(uint32_t faults)
{
    if (faults == 0u) {
        return "none";
    }
    for (size_t i = 0; i < sizeof(FAULT_PRIORITY) / sizeof(FAULT_PRIORITY[0]); i++) {
        if ((faults & FAULT_PRIORITY[i]) != 0u) {
            return ba_fault_name(FAULT_PRIORITY[i]);
        }
    }
    return "unknown";
}

void ba_update(ba_t *a, const bat_raw_sample_t *raw, const bat_features_t *f,
               const rf_result_t *ml, uint32_t now_ms, bat_assessment_t *out)
{
    if (out == 0) {
        return;
    }
    if (a == 0 || !a->initialized) {
        reset_output(a, out, "assessment_not_initialized");
        return;
    }
    if (!a->config_valid) {
        reset_output(a, out, "invalid_configuration");
        return;
    }
    if (raw == 0 || f == 0) {
        reset_output(a, out, "missing_sensor_data");
        return;
    }

    const ba_config_t *c = &a->config;
    uint32_t faults = raw->external_faults;

    bool no_valid_temp = false;
    uint8_t valid_temp_count = 0;
    for (uint8_t i = 0; i < c->num_temp_sensors; i++) {
        if ((f->temp_valid_mask & (uint16_t)(1u << i)) != 0u) {
            valid_temp_count++;
        }
        switch (raw->temp_status[i]) {
        case SENSOR_STATUS_OPEN:
            faults |= BAT_FAULT_TEMP_SENSOR_OPEN;
            break;
        case SENSOR_STATUS_SHORT:
        case SENSOR_STATUS_RAIL:
            faults |= BAT_FAULT_TEMP_SENSOR_SHORT;
            break;
        case SENSOR_STATUS_OUT_OF_RANGE:
        case SENSOR_STATUS_ABSENT:
        case SENSOR_STATUS_CONFLICT:
            faults |= BAT_FAULT_SENSOR_PLAUSIBILITY;
            break;
        default:
            break;
        }
        if ((f->temp_stuck_mask & (uint16_t)(1u << i)) != 0u) {
            faults |= BAT_FAULT_TEMP_SENSOR_STUCK;
        }
    }
    if (valid_temp_count == 0u) {
        no_valid_temp = true;
        faults |= BAT_FAULT_TEMP_SENSOR_OPEN | BAT_FAULT_SENSOR_PLAUSIBILITY;
    }

    float t_max = f->t_max;
    float t_min = f->t_min;
    float spread = f->t_spread;
    float rate = f->dt_dt_c_per_s;
    float voltage = f->voltage_v;
    float current = f->current_a;
    float abs_current = bat_absf(current);

    bool temp_warn = bat_is_finite(t_max) && t_max >= c->temp_warn_c;
    bool temp_crit = bat_is_finite(t_max) && t_max >= c->temp_crit_c;
    bool temp_trip = bat_is_finite(t_max) && t_max >= c->temp_trip_c;
    bool temp_low_warn = bat_is_finite(t_min) && t_min <= c->temp_low_warn_c;
    bool temp_low_crit = bat_is_finite(t_min) && t_min <= c->temp_low_crit_c;
    bool spread_warn = bat_is_finite(spread) && spread >= c->temp_spread_warn_c;
    bool spread_crit = bat_is_finite(spread) && spread >= c->temp_spread_crit_c;
    bool rate_warn = bat_is_finite(rate) && rate >= c->temp_rate_warn_c_s;
    bool rate_crit = bat_is_finite(rate) && rate >= c->temp_rate_crit_c_s;

    if (temp_warn || temp_low_warn) {
        faults |= BAT_FAULT_TEMP_WARN;
    }
    if (temp_crit || temp_low_crit) {
        faults |= BAT_FAULT_TEMP_CRITICAL;
    }
    if (temp_trip) {
        faults |= BAT_FAULT_TEMP_TRIP;
    }
    if (spread_warn) {
        faults |= BAT_FAULT_TEMP_SPREAD;
    }
    if (rate_warn) {
        faults |= BAT_FAULT_TEMP_RATE;
    }

    bool volt_sensor_fault = !bat_is_finite(voltage);
    bool volt_reverse = bat_is_finite(voltage) && voltage <= c->volt_reverse_v;
    bool volt_high_warn = bat_is_finite(voltage) && voltage >= c->volt_warn_high_v;
    bool volt_high_crit = bat_is_finite(voltage) && voltage >= c->volt_crit_high_v;
    bool volt_low_warn = bat_is_finite(voltage) && voltage <= c->volt_warn_low_v;
    bool volt_low_crit = bat_is_finite(voltage) && voltage <= c->volt_crit_low_v;
    bool volt_abs_trip = bat_is_finite(voltage) && bat_absf(voltage) >= c->volt_abs_max_v;

    if (volt_sensor_fault) {
        faults |= BAT_FAULT_VOLT_SENSOR;
    }
    if (volt_reverse) {
        faults |= BAT_FAULT_VOLT_REVERSE;
    }
    if (volt_high_warn || volt_low_warn) {
        faults |= BAT_FAULT_VOLT_HIGH;
    }
    if (volt_low_warn) {
        faults |= BAT_FAULT_VOLT_LOW;
    }
    if (volt_abs_trip) {
        faults |= BAT_FAULT_VOLT_SENSOR;
    }

    bool current_sensor_fault = !bat_is_finite(current);
    bool overcurrent_warn = bat_is_finite(abs_current) && abs_current >= c->current_warn_a;
    bool overcurrent_crit = bat_is_finite(abs_current) && abs_current >= c->current_crit_a;
    bool overcurrent_trip = bat_is_finite(abs_current) && abs_current >= c->current_abs_max_a;
    if (current_sensor_fault) {
        faults |= BAT_FAULT_CURRENT_SENSOR;
    }
    if (overcurrent_warn || overcurrent_crit || overcurrent_trip) {
        faults |= BAT_FAULT_OVERCURRENT;
    }

    bool resistance_warn = bat_is_finite(f->resistance_ohm) &&
                           f->resistance_ohm >= c->resistance_warn_ohm;
    bool resistance_crit = bat_is_finite(f->resistance_ohm) &&
                           f->resistance_ohm >= c->resistance_crit_ohm;
    if (resistance_warn) {
        faults |= BAT_FAULT_RESISTANCE_HIGH;
    }

    bool rain_warn = bat_is_finite(f->rain_pct) && f->rain_pct >= c->rain_wet_warn_pct;
    bool rain_crit = bat_is_finite(f->rain_pct) && f->rain_pct >= c->rain_wet_crit_pct;
    bool rain_conflict = false;
    if (bat_is_finite(f->rain_pct)) {
        if (raw->rain_digital_dry != 0u && f->rain_pct >= 80.0f) {
            rain_conflict = true;
        } else if (raw->rain_digital_dry == 0u && f->rain_pct <= 25.0f) {
            rain_conflict = true;
        }
    }
    if (rain_warn) {
        faults |= BAT_FAULT_RAIN_WET;
    }
    if (rain_conflict) {
        faults |= BAT_FAULT_RAIN_CONFLICT;
    }

    bool humidity_warn = f->humidity_valid && bat_is_finite(f->humidity_pct) &&
                         f->humidity_pct >= c->humidity_warn_pct;
    bool humidity_crit = f->humidity_valid && bat_is_finite(f->humidity_pct) &&
                         f->humidity_pct >= c->humidity_crit_pct;
    if (humidity_warn) {
        faults |= BAT_FAULT_HUMIDITY_HIGH;
    }

    if (raw->estop_active) {
        faults |= BAT_FAULT_ESTOP;
    }
    if (!raw->comms_ok) {
        faults |= BAT_FAULT_COMMS_TIMEOUT;
    }
    if (!raw->self_test_ok) {
        faults |= BAT_FAULT_SELF_TEST;
    }

    bool ml_domain_ok = true;
    if (c->ml_enabled != 0u) {
        if (c->domain_gate_enabled != 0u) {
            if (!bat_is_finite(voltage) || voltage < c->domain_voltage_min_v ||
                voltage > c->domain_voltage_max_v || !bat_is_finite(current) ||
                current < c->domain_current_min_a || current > c->domain_current_max_a) {
                ml_domain_ok = false;
                faults |= BAT_FAULT_PROFILE_MISMATCH;
            }
        }
    } else {
        ml_domain_ok = false;
    }

    bool ml_usable = ml_domain_ok && ml != 0 && ml->valid != 0u;
    bool ml_unsafe = false;
    bool ml_low_confidence = false;
    if (ml_usable) {
        ml_unsafe = ml->label == BAT_LABEL_UNSAFE && ml->p_unsafe >= c->ml_unsafe_threshold;
        float confidence = maxf_local(ml->p_unsafe, 1.0f - ml->p_unsafe);
        ml_low_confidence = ml->low_confidence != 0u || confidence < c->ml_min_confidence;
        if (ml_unsafe && !ml_low_confidence) {
            faults |= BAT_FAULT_ML_UNSAFE;
        }
        if (ml_low_confidence) {
            faults |= BAT_FAULT_ML_LOW_CONFIDENCE;
        }
    }

    float temp_risk = maxf_local(risk_high(t_max, c->temp_warn_c, c->temp_crit_c),
                                 risk_low(t_min, c->temp_low_warn_c, c->temp_low_crit_c));
    float spread_risk = risk_high(spread, c->temp_spread_warn_c, c->temp_spread_crit_c);
    float rate_risk = risk_high(rate, c->temp_rate_warn_c_s, c->temp_rate_crit_c_s);
    float voltage_risk = maxf_local(risk_high(voltage, c->volt_warn_high_v, c->volt_crit_high_v),
                                    risk_low(voltage, c->volt_warn_low_v, c->volt_crit_low_v));
    if (volt_reverse) {
        voltage_risk = 100.0f;
    }
    float current_risk = risk_high(abs_current, c->current_warn_a, c->current_crit_a);
    float resistance_risk = risk_high(f->resistance_ohm, c->resistance_warn_ohm,
                                      c->resistance_crit_ohm);
    float rain_risk = risk_high(f->rain_pct, c->rain_wet_warn_pct, c->rain_wet_crit_pct);
    float humidity_risk = f->humidity_valid
                              ? risk_high(f->humidity_pct, c->humidity_warn_pct,
                                          c->humidity_crit_pct)
                              : 0.0f;
    float sensor_risk = 0.0f;
    if ((faults & (BAT_FAULT_TEMP_SENSOR_OPEN | BAT_FAULT_TEMP_SENSOR_SHORT |
                   BAT_FAULT_SENSOR_PLAUSIBILITY | BAT_FAULT_TEMP_SENSOR_STUCK)) != 0u) {
        sensor_risk = 100.0f;
    }

    float rule_score = 0.32f * temp_risk + 0.08f * spread_risk + 0.12f * rate_risk +
                       0.18f * voltage_risk + 0.14f * current_risk + 0.05f * resistance_risk +
                       0.04f * rain_risk + 0.02f * humidity_risk + 0.05f * sensor_risk;
    rule_score = bat_clampf(rule_score, 0.0f, 100.0f);

    if (temp_warn || temp_low_warn || spread_warn || rate_warn || volt_high_warn ||
        volt_low_warn || overcurrent_warn || rain_warn || humidity_warn ||
        (faults & (BAT_FAULT_TEMP_SENSOR_STUCK | BAT_FAULT_COMMS_TIMEOUT |
                   BAT_FAULT_RAIN_CONFLICT)) != 0u) {
        rule_score = maxf_local(rule_score, c->score_warn);
    }
    if (temp_crit || temp_low_crit || spread_crit || rate_crit || volt_high_crit ||
        volt_low_crit || overcurrent_crit || humidity_crit || rain_crit ||
        (faults & (BAT_FAULT_TEMP_SENSOR_OPEN | BAT_FAULT_TEMP_SENSOR_SHORT |
                   BAT_FAULT_SENSOR_PLAUSIBILITY | BAT_FAULT_SELF_TEST)) != 0u) {
        rule_score = maxf_local(rule_score, c->score_crit);
    }
    if (temp_trip || volt_reverse || overcurrent_trip || volt_abs_trip || raw->estop_active ||
        no_valid_temp) {
        rule_score = 100.0f;
    }

    float risk_score = rule_score;
    float ml_score = -1.0f;
    if (ml_usable) {
        ml_score = bat_clampf(ml->p_unsafe * 100.0f, 0.0f, 100.0f);
        float ml_weight = ml_low_confidence ? 0.20f : 0.50f;
        risk_score = rule_score * (1.0f - ml_weight) + ml_score * ml_weight;
        if (ml_unsafe && !ml_low_confidence) {
            risk_score = maxf_local(risk_score, c->score_warn + 1.0f);
        }
    }
    if ((faults & HARD_TRIP_MASK) != 0u || no_valid_temp) {
        risk_score = 100.0f;
    }
    risk_score = bat_clampf(risk_score, 0.0f, 100.0f);

    bool hard_condition = ((faults & HARD_TRIP_MASK) != 0u) || no_valid_temp;
    bool warn_condition = temp_warn || temp_low_warn || spread_warn || rate_warn ||
                          volt_high_warn || volt_low_warn || overcurrent_warn || rain_warn ||
                          humidity_warn || resistance_warn || risk_score >= c->score_warn ||
                          (faults & (BAT_FAULT_TEMP_SENSOR_STUCK | BAT_FAULT_COMMS_TIMEOUT |
                                     BAT_FAULT_RAIN_CONFLICT)) != 0u;
    bool crit_condition = temp_crit || temp_low_crit || spread_crit || rate_crit ||
                          volt_high_crit || volt_low_crit || overcurrent_crit || rain_crit ||
                          humidity_crit || resistance_crit || risk_score >= c->score_crit ||
                          (faults & (BAT_FAULT_TEMP_SENSOR_OPEN | BAT_FAULT_TEMP_SENSOR_SHORT |
                                     BAT_FAULT_SENSOR_PLAUSIBILITY | BAT_FAULT_SELF_TEST)) != 0u;

    if (hard_condition) {
        if (db_update(&a->trip_db, true, now_ms, c->trip_debounce_ms)) {
            a->trip_latched = 1;
            a->reset_requested = 0;
            a->latched_faults = faults | BAT_FAULT_TRIP_LATCHED;
            a->state = BAT_STATE_TRIPPED;
        }
    } else {
        db_update(&a->trip_db, false, now_ms, c->trip_debounce_ms);
    }

    if (a->trip_latched) {
        a->state = BAT_STATE_TRIPPED;
        bool safe_to_reset = (faults & HARD_TRIP_MASK) == 0u && !no_valid_temp &&
                             risk_score < c->score_crit && raw->self_test_ok != 0u &&
                             !raw->estop_active;
        if (a->reset_requested && safe_to_reset &&
            db_update(&a->recover_crit_db, true, now_ms, c->recovery_ms)) {
            a->trip_latched = 0;
            a->reset_requested = 0;
            a->latched_faults = 0;
            db_update(&a->recover_crit_db, false, now_ms, c->recovery_ms);
            a->state = (risk_score >= c->score_warn_exit || warn_condition) ? BAT_STATE_WARNING
                                                                            : BAT_STATE_NORMAL;
        } else if (!safe_to_reset) {
            db_update(&a->recover_crit_db, false, now_ms, c->recovery_ms);
        }
    } else {
        if (crit_condition) {
            if (db_update(&a->crit_db, true, now_ms, c->crit_debounce_ms)) {
                a->state = BAT_STATE_CRITICAL;
            }
        } else {
            db_update(&a->crit_db, false, now_ms, c->crit_debounce_ms);
            if (a->state == BAT_STATE_CRITICAL) {
                bool safe = risk_score < c->score_crit_exit && !crit_condition;
                if (safe && db_update(&a->recover_crit_db, true, now_ms, c->recovery_ms)) {
                    a->state = (risk_score >= c->score_warn_exit || warn_condition)
                                   ? BAT_STATE_WARNING
                                   : BAT_STATE_NORMAL;
                    db_update(&a->recover_crit_db, false, now_ms, c->recovery_ms);
                } else if (!safe) {
                    db_update(&a->recover_crit_db, false, now_ms, c->recovery_ms);
                }
            }
        }

        if (a->state != BAT_STATE_CRITICAL) {
            if (warn_condition) {
                if (db_update(&a->warn_db, true, now_ms, c->warn_debounce_ms) &&
                    a->state == BAT_STATE_NORMAL) {
                    a->state = BAT_STATE_WARNING;
                }
            } else {
                db_update(&a->warn_db, false, now_ms, c->warn_debounce_ms);
                if (a->state == BAT_STATE_WARNING) {
                    bool safe = risk_score < c->score_warn_exit && !warn_condition;
                    if (safe && db_update(&a->recover_warn_db, true, now_ms, c->recovery_ms)) {
                        a->state = BAT_STATE_NORMAL;
                        db_update(&a->recover_warn_db, false, now_ms, c->recovery_ms);
                    } else if (!safe) {
                        db_update(&a->recover_warn_db, false, now_ms, c->recovery_ms);
                    }
                }
            }
        }

        if (a->state == BAT_STATE_INIT && f->age_ms >= c->warmup_ms && !warn_condition &&
            !crit_condition && (faults & (BAT_FAULT_CONFIG_INVALID | BAT_FAULT_SELF_TEST)) == 0u) {
            a->state = BAT_STATE_NORMAL;
        }
    }

    a->active_faults = faults;
    a->rule_score = rule_score;
    a->ml_score = ml_score;
    a->risk_score = risk_score;
    a->ml_p_unsafe = ml != 0 ? ml->p_unsafe : 0.0f;
    a->ml_label = ml != 0 ? ml->label : BAT_LABEL_SAFE;
    a->ml_valid = ml_usable ? 1u : 0u;
    a->ml_low_confidence = ml_low_confidence ? 1u : 0u;
    a->last_ms = now_ms;

    memset(out, 0, sizeof(*out));
    out->state = a->state;
    out->active_faults = faults;
    out->latched_faults = a->latched_faults;
    out->rule_score = rule_score;
    out->ml_score = ml_score;
    out->risk_score = risk_score;
    out->ml_p_unsafe = a->ml_p_unsafe;
    out->ml_label = a->ml_label;
    out->ml_valid = a->ml_valid;
    out->ml_low_confidence = a->ml_low_confidence;
    out->trip_latched = a->trip_latched;
    out->relay_allowed = (a->state == BAT_STATE_NORMAL || a->state == BAT_STATE_WARNING) &&
                         raw->self_test_ok != 0u && !raw->estop_active && !a->trip_latched &&
                         (faults & HARD_TRIP_MASK) == 0u;
    out->buzzer_on = (a->state == BAT_STATE_WARNING || a->state == BAT_STATE_CRITICAL ||
                      a->state == BAT_STATE_TRIPPED);
    out->reason = dominant_fault_name(faults);
}

#include "battery_profiles.h"

#include <string.h>

static void set_common(ba_config_t *c, uint8_t num_temp)
{
    c->num_temp_sensors = num_temp;
    c->score_warn = 40.0f;
    c->score_crit = 70.0f;
    c->score_trip = 85.0f;
    c->score_warn_exit = 30.0f;
    c->score_crit_exit = 60.0f;
    c->ml_unsafe_threshold = 0.50f;
    c->ml_min_confidence = 0.65f;
    c->warn_debounce_ms = 2000u;
    c->crit_debounce_ms = 1000u;
    c->trip_debounce_ms = 500u;
    c->recovery_ms = 10000u;
    c->warmup_ms = 3000u;
    c->comms_timeout_ms = 10000u;
    c->stuck_window_ms = 8000u;
    c->temp_min_plausible_c = -40.0f;
    c->temp_max_plausible_c = 150.0f;
    c->humidity_warn_pct = 80.0f;
    c->humidity_crit_pct = 92.0f;
    c->rain_wet_warn_pct = 65.0f;
    c->rain_wet_crit_pct = 82.0f;
    c->current_zero_tolerance_a = 0.08f;
    c->domain_gate_enabled = 1u;
}

bool ba_config_from_profile(ba_config_t *config, bat_profile_t profile, uint8_t num_temp)
{
    if (config == 0) {
        return false;
    }
    if (num_temp == 0 || num_temp > BAT_MAX_TEMP_SENSORS) {
        return false;
    }
    memset(config, 0, sizeof(*config));
    set_common(config, num_temp);

    switch (profile) {
    case BAT_PROFILE_BENCH_6V:
        config->temp_warn_c = 60.0f;
        config->temp_crit_c = 75.0f;
        config->temp_trip_c = 85.0f;
        config->temp_low_warn_c = -10.0f;
        config->temp_low_crit_c = -20.0f;
        config->temp_spread_warn_c = 12.0f;
        config->temp_spread_crit_c = 20.0f;
        config->temp_rate_warn_c_s = 0.50f;
        config->temp_rate_crit_c_s = 2.00f;
        config->volt_warn_high_v = 6.90f;
        config->volt_crit_high_v = 7.40f;
        config->volt_warn_low_v = 5.60f;
        config->volt_crit_low_v = 5.30f;
        config->volt_abs_max_v = 8.50f;
        config->volt_reverse_v = -0.50f;
        config->current_warn_a = 0.90f;
        config->current_crit_a = 1.50f;
        config->current_abs_max_a = 2.00f;
        config->resistance_warn_ohm = 0.50f;
        config->resistance_crit_ohm = 1.00f;
        config->ml_enabled = 1u;
        config->domain_voltage_min_v = 5.00f;
        config->domain_voltage_max_v = 8.50f;
        config->domain_current_min_a = -2.00f;
        config->domain_current_max_a = 1.50f;
        return true;
    case BAT_PROFILE_LEAD_12V:
        config->temp_warn_c = 55.0f;
        config->temp_crit_c = 65.0f;
        config->temp_trip_c = 75.0f;
        config->temp_low_warn_c = -15.0f;
        config->temp_low_crit_c = -25.0f;
        config->temp_spread_warn_c = 10.0f;
        config->temp_spread_crit_c = 18.0f;
        config->temp_rate_warn_c_s = 0.40f;
        config->temp_rate_crit_c_s = 1.50f;
        config->volt_warn_high_v = 14.40f;
        config->volt_crit_high_v = 15.00f;
        config->volt_warn_low_v = 11.80f;
        config->volt_crit_low_v = 10.80f;
        config->volt_abs_max_v = 16.00f;
        config->volt_reverse_v = -1.00f;
        config->current_warn_a = 10.0f;
        config->current_crit_a = 20.0f;
        config->current_abs_max_a = 30.0f;
        config->resistance_warn_ohm = 0.20f;
        config->resistance_crit_ohm = 0.50f;
        config->ml_enabled = 0u;
        return true;
    case BAT_PROFILE_LFP_24V:
        config->temp_warn_c = 55.0f;
        config->temp_crit_c = 65.0f;
        config->temp_trip_c = 75.0f;
        config->temp_low_warn_c = -10.0f;
        config->temp_low_crit_c = -20.0f;
        config->temp_spread_warn_c = 8.0f;
        config->temp_spread_crit_c = 15.0f;
        config->temp_rate_warn_c_s = 0.50f;
        config->temp_rate_crit_c_s = 2.00f;
        config->volt_warn_high_v = 28.00f;
        config->volt_crit_high_v = 29.20f;
        config->volt_warn_low_v = 22.00f;
        config->volt_crit_low_v = 20.00f;
        config->volt_abs_max_v = 31.00f;
        config->volt_reverse_v = -2.00f;
        config->current_warn_a = 30.0f;
        config->current_crit_a = 60.0f;
        config->current_abs_max_a = 100.0f;
        config->resistance_warn_ohm = 0.10f;
        config->resistance_crit_ohm = 0.30f;
        config->ml_enabled = 0u;
        return true;
    case BAT_PROFILE_LFP_48V:
        config->temp_warn_c = 55.0f;
        config->temp_crit_c = 65.0f;
        config->temp_trip_c = 75.0f;
        config->temp_low_warn_c = -10.0f;
        config->temp_low_crit_c = -20.0f;
        config->temp_spread_warn_c = 8.0f;
        config->temp_spread_crit_c = 15.0f;
        config->temp_rate_warn_c_s = 0.50f;
        config->temp_rate_crit_c_s = 2.00f;
        config->volt_warn_high_v = 55.00f;
        config->volt_crit_high_v = 58.00f;
        config->volt_warn_low_v = 42.00f;
        config->volt_crit_low_v = 38.00f;
        config->volt_abs_max_v = 60.00f;
        config->volt_reverse_v = -4.00f;
        config->current_warn_a = 50.0f;
        config->current_crit_a = 100.0f;
        config->current_abs_max_a = 150.0f;
        config->resistance_warn_ohm = 0.05f;
        config->resistance_crit_ohm = 0.15f;
        config->ml_enabled = 0u;
        return true;
    case BAT_PROFILE_COLD_CHAIN:
        config->temp_warn_c = 8.0f;
        config->temp_crit_c = 10.0f;
        config->temp_trip_c = 12.0f;
        config->temp_low_warn_c = 2.0f;
        config->temp_low_crit_c = 0.0f;
        config->temp_min_plausible_c = -40.0f;
        config->temp_max_plausible_c = 60.0f;
        config->temp_spread_warn_c = 3.0f;
        config->temp_spread_crit_c = 5.0f;
        config->temp_rate_warn_c_s = 0.20f;
        config->temp_rate_crit_c_s = 0.80f;
        config->volt_warn_high_v = 14.40f;
        config->volt_crit_high_v = 14.80f;
        config->volt_warn_low_v = 11.80f;
        config->volt_crit_low_v = 10.80f;
        config->volt_abs_max_v = 15.50f;
        config->volt_reverse_v = -1.00f;
        config->current_warn_a = 8.0f;
        config->current_crit_a = 15.0f;
        config->current_abs_max_a = 20.0f;
        config->resistance_warn_ohm = 0.20f;
        config->resistance_crit_ohm = 0.50f;
        config->ml_enabled = 0u;
        return true;
    case BAT_PROFILE_CUSTOM:
    default:
        config->temp_warn_c = 55.0f;
        config->temp_crit_c = 65.0f;
        config->temp_trip_c = 75.0f;
        config->temp_low_warn_c = -10.0f;
        config->temp_low_crit_c = -20.0f;
        config->temp_spread_warn_c = 10.0f;
        config->temp_spread_crit_c = 18.0f;
        config->temp_rate_warn_c_s = 0.50f;
        config->temp_rate_crit_c_s = 2.00f;
        config->volt_warn_high_v = 14.40f;
        config->volt_crit_high_v = 15.00f;
        config->volt_warn_low_v = 11.80f;
        config->volt_crit_low_v = 10.80f;
        config->volt_abs_max_v = 16.00f;
        config->volt_reverse_v = -1.00f;
        config->current_warn_a = 10.0f;
        config->current_crit_a = 20.0f;
        config->current_abs_max_a = 30.0f;
        config->resistance_warn_ohm = 0.20f;
        config->resistance_crit_ohm = 0.50f;
        config->ml_enabled = 0u;
        return profile == BAT_PROFILE_CUSTOM;
    }
}

const char *ba_profile_name(bat_profile_t profile)
{
    switch (profile) {
    case BAT_PROFILE_BENCH_6V:
        return "bench-6v";
    case BAT_PROFILE_LEAD_12V:
        return "lead-12v";
    case BAT_PROFILE_LFP_24V:
        return "lfp-24v";
    case BAT_PROFILE_LFP_48V:
        return "lfp-48v";
    case BAT_PROFILE_COLD_CHAIN:
        return "cold-chain";
    case BAT_PROFILE_CUSTOM:
        return "custom";
    default:
        return "unknown";
    }
}

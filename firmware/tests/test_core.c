#include "battery_assessment.h"
#include "battery_profiles.h"
#include "feature_builder.h"
#include "filters.h"
#include "safety_controller.h"
#include "telemetry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                                     \
    do {                                                                                     \
        if (!(condition)) {                                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);            \
            failures++;                                                                      \
        }                                                                                    \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                                             \
    do {                                                                                     \
        float a_ = (float)(actual);                                                          \
        float e_ = (float)(expected);                                                        \
        if (fabsf(a_ - e_) > (float)(tolerance)) {                                           \
            fprintf(stderr, "FAIL %s:%d: %f != %f (+/- %f)\n", __FILE__, __LINE__, a_, e_,   \
                    (float)(tolerance));                                                     \
            failures++;                                                                      \
        }                                                                                    \
    } while (0)

static void base_inputs(bat_raw_sample_t *raw, bat_features_t *features, float temperature,
                        float voltage, float current, float rain, uint8_t rain_dry,
                        uint32_t now_ms)
{
    memset(raw, 0, sizeof(*raw));
    memset(features, 0, sizeof(*features));
    raw->temp_valid_mask = 0x00FFu;
    raw->channel_count = 8;
    raw->voltage_v = voltage;
    raw->current_a = current;
    raw->rain_analog = rain;
    raw->rain_digital_dry = rain_dry;
    raw->comms_ok = 1;
    raw->self_test_ok = 1;
    raw->timestamp_ms = now_ms;
    features->num_temp = 8;
    features->temp_valid_mask = 0x00FFu;
    features->voltage_v = voltage;
    features->current_a = current;
    features->power_w = voltage * current;
    features->rain_pct = bat_clampf((rain / 1023.0f) * 100.0f, 0.0f, 100.0f);
    features->rain_wet = (rain_dry == 0u || features->rain_pct >= 60.0f) ? 1u : 0u;
    features->humidity_valid = 0;
    features->humidity_pct = NAN;
    features->resistance_ohm = NAN;
    features->age_ms = now_ms;
    features->sample_count = 1;
    features->t_min = temperature;
    features->t_max = temperature;
    features->t_avg = temperature;
    features->t_spread = 0.0f;
    for (uint8_t i = 0; i < 8; i++) {
        raw->temp_c[i] = temperature;
        raw->temp_status[i] = SENSOR_STATUS_OK;
        features->temp_c[i] = temperature;
    }
}

static void prime_normal(ba_t *assessment, uint32_t *now_ms)
{
    for (int i = 0; i < 6; i++) {
        *now_ms += 1000u;
        bat_raw_sample_t raw;
        bat_features_t features;
        bat_assessment_t out;
        base_inputs(&raw, &features, 45.0f, 6.2f, 0.3f, 500.0f, 1u, *now_ms);
        features.age_ms = *now_ms;
        ba_update(assessment, &raw, &features, NULL, *now_ms, &out);
    }
}

static void test_filters(void)
{
    float_window_t window;
    fw_init(&window, 5);
    fw_push(&window, 1.0f);
    fw_push(&window, 3.0f);
    fw_push(&window, 2.0f);
    fw_push(&window, 5.0f);
    fw_push(&window, 4.0f);
    CHECK(fw_count(&window) == 5);
    CHECK_NEAR(fw_min(&window), 1.0f, 0.0001f);
    CHECK_NEAR(fw_max(&window), 5.0f, 0.0001f);
    CHECK_NEAR(fw_range(&window), 4.0f, 0.0001f);
    CHECK_NEAR(fw_median(&window), 3.0f, 0.0001f);
    CHECK_NEAR(fw_mean(&window), 3.0f, 0.0001f);

    float state = NAN;
    CHECK_NEAR(ema_update(&state, 10.0f, 0.5f), 10.0f, 0.0001f);
    CHECK_NEAR(ema_update(&state, 20.0f, 0.5f), 15.0f, 0.0001f);
    CHECK_NEAR(bat_clampf(12.0f, 0.0f, 10.0f), 10.0f, 0.0001f);
    CHECK(!bat_is_finite(NAN));
}

static void test_feature_builder(void)
{
    fb_t builder;
    fb_config_t config;
    fb_default_config(&config);
    config.window_size = 4;
    CHECK(fb_init(&builder, 8, &config, 0u));

    bat_features_t features;
    for (int step = 0; step < 8; step++) {
        bat_raw_sample_t raw;
        memset(&raw, 0, sizeof(raw));
        raw.temp_valid_mask = 0x00FFu;
        raw.channel_count = 8;
        raw.voltage_v = 6.2f + 0.01f * (float)step;
        raw.current_a = 0.3f;
        raw.rain_analog = 500.0f;
        raw.rain_digital_dry = 1u;
        raw.timestamp_ms = (uint32_t)(step * 1000);
        for (uint8_t i = 0; i < 8; i++) {
            raw.temp_c[i] = (i == 0u) ? (40.0f + 0.5f * (float)step) : 40.0f;
            raw.temp_status[i] = SENSOR_STATUS_OK;
        }
        fb_update(&builder, &raw, &features, (uint32_t)(step * 1000));
    }

    CHECK(features.temp_valid_mask == 0x00FFu);
    CHECK(features.t_max > features.t_min);
    CHECK(features.t_spread > 1.0f);
    CHECK(features.dt_dt_c_per_s > 0.0f);
    CHECK((features.temp_stuck_mask & 0x0002u) != 0u);
    CHECK((features.temp_stuck_mask & 0x0001u) == 0u);
    CHECK_NEAR(features.rain_pct, 48.87f, 0.5f);
    CHECK(features.rain_wet == 0u);
}

static void test_profiles(void)
{
    ba_config_t config;
    const bat_profile_t profiles[] = {
        BAT_PROFILE_BENCH_6V, BAT_PROFILE_LEAD_12V, BAT_PROFILE_LFP_24V,
        BAT_PROFILE_LFP_48V, BAT_PROFILE_COLD_CHAIN, BAT_PROFILE_CUSTOM,
    };
    const char *names[] = {"bench-6v", "lead-12v", "lfp-24v", "lfp-48v", "cold-chain",
                           "custom"};
    for (size_t i = 0; i < sizeof(profiles) / sizeof(profiles[0]); i++) {
        CHECK(ba_config_from_profile(&config, profiles[i], 4));
        CHECK(ba_validate_config(&config));
        CHECK(strcmp(ba_profile_name(profiles[i]), names[i]) == 0);
    }

    CHECK(ba_config_from_profile(&config, BAT_PROFILE_COLD_CHAIN, 2));
    CHECK_NEAR(config.volt_warn_low_v, 11.80f, 0.001f);
    CHECK_NEAR(config.volt_crit_low_v, 10.80f, 0.001f);
    CHECK_NEAR(config.volt_warn_high_v, 14.40f, 0.001f);
    CHECK_NEAR(config.current_abs_max_a, 20.0f, 0.001f);
}

static void test_assessment_normal(void)
{
    ba_config_t config;
    ba_t assessment;
    bat_assessment_t out;
    CHECK(ba_config_from_profile(&config, BAT_PROFILE_BENCH_6V, 8));
    CHECK(ba_init(&assessment, &config, 0u));

    uint32_t now = 0;
    prime_normal(&assessment, &now);
    bat_raw_sample_t raw;
    bat_features_t features;
    base_inputs(&raw, &features, 45.0f, 6.2f, 0.3f, 500.0f, 1u, now);
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, NULL, now, &out);

    CHECK(out.state == BAT_STATE_NORMAL);
    CHECK(out.relay_allowed == 1u);
    CHECK(out.active_faults == 0u);
    CHECK(out.risk_score < config.score_warn);
}

static void test_assessment_temp_trip_and_reset(void)
{
    ba_config_t config;
    ba_t assessment;
    bat_assessment_t out;
    CHECK(ba_config_from_profile(&config, BAT_PROFILE_BENCH_6V, 8));
    CHECK(ba_init(&assessment, &config, 0u));

    uint32_t now = 0;
    prime_normal(&assessment, &now);
    bat_raw_sample_t raw;
    bat_features_t features;

    now += 1000u;
    base_inputs(&raw, &features, 90.0f, 6.2f, 0.3f, 500.0f, 1u, now);
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, NULL, now, &out);
    now += 500u;
    base_inputs(&raw, &features, 90.0f, 6.2f, 0.3f, 500.0f, 1u, now);
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, NULL, now, &out);
    CHECK(out.state == BAT_STATE_TRIPPED);
    CHECK(out.relay_allowed == 0u);
    CHECK(out.trip_latched == 1u);
    CHECK((out.active_faults & BAT_FAULT_TEMP_TRIP) != 0u);

    now += 1000u;
    base_inputs(&raw, &features, 40.0f, 6.2f, 0.3f, 500.0f, 1u, now);
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, NULL, now, &out);
    CHECK(out.state == BAT_STATE_TRIPPED);

    ba_manual_reset(&assessment);
    now += 1000u;
    base_inputs(&raw, &features, 40.0f, 6.2f, 0.3f, 500.0f, 1u, now);
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, NULL, now, &out);
    for (int i = 0; i < 12; i++) {
        now += 1000u;
        base_inputs(&raw, &features, 40.0f, 6.2f, 0.3f, 500.0f, 1u, now);
        features.age_ms = now;
        ba_update(&assessment, &raw, &features, NULL, now, &out);
    }
    CHECK(out.state == BAT_STATE_NORMAL || out.state == BAT_STATE_WARNING);
    CHECK(out.trip_latched == 0u);
    CHECK(out.relay_allowed == 1u);
}

static void test_assessment_overcurrent_and_reverse(void)
{
    ba_config_t config;
    ba_t assessment;
    bat_assessment_t out;
    bat_raw_sample_t raw;
    bat_features_t features;
    uint32_t now = 0;
    CHECK(ba_config_from_profile(&config, BAT_PROFILE_BENCH_6V, 8));
    CHECK(ba_init(&assessment, &config, 0u));
    prime_normal(&assessment, &now);

    now += 1000u;
    base_inputs(&raw, &features, 45.0f, 6.2f, 3.0f, 500.0f, 1u, now);
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, NULL, now, &out);
    now += 500u;
    base_inputs(&raw, &features, 45.0f, 6.2f, 3.0f, 500.0f, 1u, now);
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, NULL, now, &out);
    CHECK(out.state == BAT_STATE_TRIPPED);
    CHECK((out.active_faults & BAT_FAULT_OVERCURRENT) != 0u);

    CHECK(ba_init(&assessment, &config, 0u));
    now = 0;
    prime_normal(&assessment, &now);
    now += 1000u;
    base_inputs(&raw, &features, 45.0f, -1.0f, 0.3f, 500.0f, 1u, now);
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, NULL, now, &out);
    now += 500u;
    base_inputs(&raw, &features, 45.0f, -1.0f, 0.3f, 500.0f, 1u, now);
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, NULL, now, &out);
    CHECK(out.state == BAT_STATE_TRIPPED);
    CHECK((out.active_faults & BAT_FAULT_VOLT_REVERSE) != 0u);
}

static void test_assessment_sensor_faults(void)
{
    ba_config_t config;
    ba_t assessment;
    bat_assessment_t out;
    bat_raw_sample_t raw;
    bat_features_t features;
    uint32_t now = 0;
    CHECK(ba_config_from_profile(&config, BAT_PROFILE_BENCH_6V, 8));
    CHECK(ba_init(&assessment, &config, 0u));
    prime_normal(&assessment, &now);

    now += 1000u;
    base_inputs(&raw, &features, 45.0f, 6.2f, 0.3f, 500.0f, 1u, now);
    raw.temp_status[0] = SENSOR_STATUS_OPEN;
    features.temp_valid_mask = 0x00FEu;
    features.temp_c[0] = NAN;
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, NULL, now, &out);
    CHECK((out.active_faults & BAT_FAULT_TEMP_SENSOR_OPEN) != 0u);
    for (int i = 0; i < 2; i++) {
        now += 1000u;
        base_inputs(&raw, &features, 45.0f, 6.2f, 0.3f, 500.0f, 1u, now);
        raw.temp_status[0] = SENSOR_STATUS_OPEN;
        features.temp_valid_mask = 0x00FEu;
        features.temp_c[0] = NAN;
        features.age_ms = now;
        ba_update(&assessment, &raw, &features, NULL, now, &out);
    }
    CHECK(out.state == BAT_STATE_CRITICAL);
    CHECK(out.relay_allowed == 0u);

    CHECK(ba_init(&assessment, &config, 0u));
    now = 0;
    prime_normal(&assessment, &now);
    now += 1000u;
    base_inputs(&raw, &features, 45.0f, 6.2f, 0.3f, 500.0f, 1u, now);
    features.temp_valid_mask = 0u;
    features.t_max = NAN;
    features.t_min = NAN;
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, NULL, now, &out);
    now += 500u;
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, NULL, now, &out);
    CHECK(out.state == BAT_STATE_TRIPPED);
    CHECK(out.relay_allowed == 0u);
}

static void test_assessment_ml_and_comms(void)
{
    ba_config_t config;
    ba_t assessment;
    bat_assessment_t out;
    bat_raw_sample_t raw;
    bat_features_t features;
    rf_result_t ml;
    uint32_t now = 0;
    CHECK(ba_config_from_profile(&config, BAT_PROFILE_BENCH_6V, 8));
    CHECK(ba_init(&assessment, &config, 0u));
    prime_normal(&assessment, &now);

    ml.valid = 1;
    ml.p_unsafe = 0.95f;
    ml.label = BAT_LABEL_UNSAFE;
    ml.low_confidence = 0;
    for (int i = 0; i < 4; i++) {
        now += 1000u;
        base_inputs(&raw, &features, 45.0f, 6.2f, 0.3f, 500.0f, 1u, now);
        features.age_ms = now;
        ba_update(&assessment, &raw, &features, &ml, now, &out);
    }
    CHECK((out.active_faults & BAT_FAULT_ML_UNSAFE) != 0u);
    CHECK(out.state == BAT_STATE_WARNING || out.state == BAT_STATE_CRITICAL);
    CHECK(out.ml_valid == 1u);

    CHECK(ba_init(&assessment, &config, 0u));
    now = 0;
    prime_normal(&assessment, &now);
    for (int i = 0; i < 4; i++) {
        now += 1000u;
        base_inputs(&raw, &features, 45.0f, 6.2f, 0.3f, 500.0f, 1u, now);
        raw.comms_ok = 0;
        features.age_ms = now;
        ba_update(&assessment, &raw, &features, NULL, now, &out);
    }
    CHECK((out.active_faults & BAT_FAULT_COMMS_TIMEOUT) != 0u);
    CHECK(out.state == BAT_STATE_WARNING);

    CHECK(ba_init(&assessment, &config, 0u));
    now = 0;
    prime_normal(&assessment, &now);
    ml.valid = 1;
    ml.p_unsafe = 0.99f;
    ml.label = BAT_LABEL_UNSAFE;
    ml.low_confidence = 0;
    now += 1000u;
    base_inputs(&raw, &features, 45.0f, 12.0f, 0.3f, 500.0f, 1u, now);
    features.age_ms = now;
    ba_update(&assessment, &raw, &features, &ml, now, &out);
    CHECK((out.active_faults & BAT_FAULT_PROFILE_MISMATCH) != 0u);
    CHECK(out.ml_valid == 0u);
}

static void test_assessment_invalid_config(void)
{
    ba_config_t config;
    ba_t assessment;
    bat_assessment_t out;
    memset(&config, 0, sizeof(config));
    CHECK(!ba_init(&assessment, &config, 0u));
    bat_raw_sample_t raw;
    bat_features_t features;
    base_inputs(&raw, &features, 45.0f, 6.2f, 0.3f, 500.0f, 1u, 0u);
    ba_update(&assessment, &raw, &features, NULL, 0u, &out);
    CHECK(out.state == BAT_STATE_TRIPPED);
    CHECK(out.relay_allowed == 0u);
    CHECK((out.active_faults & BAT_FAULT_CONFIG_INVALID) != 0u);
}

static void test_safety_controller(void)
{
    sc_t controller;
    sc_outputs_t outputs;
    sc_inputs_t inputs;
    bat_assessment_t assessment;
    sc_init(&controller, 0u);
    memset(&assessment, 0, sizeof(assessment));
    assessment.state = BAT_STATE_NORMAL;
    assessment.relay_allowed = 1u;
    memset(&inputs, 0, sizeof(inputs));
    inputs.self_test_ok = 1u;
    inputs.watchdog_ok = 1u;
    inputs.heartbeat_ok = 1u;

    sc_update(&controller, &assessment, &inputs, 0u, &outputs);
    CHECK(outputs.relay_closed == 0u);
    sc_update(&controller, &assessment, &inputs, 1500u, &outputs);
    CHECK(outputs.relay_closed == 1u);
    CHECK(outputs.led_mode == SC_LED_HEARTBEAT);

    assessment.state = BAT_STATE_TRIPPED;
    assessment.relay_allowed = 0u;
    assessment.trip_latched = 1u;
    sc_update(&controller, &assessment, &inputs, 2000u, &outputs);
    CHECK(outputs.relay_closed == 0u);
    CHECK(outputs.buzzer_on == 1u);
    CHECK(outputs.led_mode == SC_LED_TRIPPED);

    assessment.state = BAT_STATE_NORMAL;
    assessment.relay_allowed = 1u;
    inputs.estop_active = 1u;
    sc_update(&controller, &assessment, &inputs, 3000u, &outputs);
    CHECK(outputs.relay_closed == 0u);
}

static void test_telemetry(void)
{
    bat_raw_sample_t raw;
    bat_features_t features;
    bat_assessment_t out;
    base_inputs(&raw, &features, 45.0f, 6.2f, 0.3f, 500.0f, 1u, 1234u);
    memset(&out, 0, sizeof(out));
    out.state = BAT_STATE_NORMAL;
    out.reason = "none";
    out.risk_score = 12.5f;
    out.rule_score = 10.0f;
    out.ml_p_unsafe = 0.1234f;
    out.ml_valid = 1u;

    char buffer[512];
    size_t length = telemetry_build_json(&raw, &features, &out, 7u, buffer, sizeof(buffer));
    CHECK(length > 0u);
    CHECK(strstr(buffer, "\"seq\":7") != NULL);
    CHECK(strstr(buffer, "\"state\":\"NORMAL\"") != NULL);
    CHECK(strstr(buffer, "\"faults\":\"0x00000000\"") != NULL);
    CHECK(strstr(buffer, "\"ver\":\"1.0.0\"") != NULL);

    char frame[600];
    size_t frame_length = telemetry_frame(buffer, frame, sizeof(frame));
    CHECK(frame_length > 0u);
    CHECK(frame[0] == '$');
    CHECK(frame[frame_length - 1u] == '\n');
    CHECK(strchr(frame, '*') != NULL);

    uint16_t crc = telemetry_crc16_ccitt((const uint8_t *)"123456789", 9u);
    CHECK(crc == 0x29B1u);

    length = telemetry_build_csv(&raw, &features, &out, 8u, buffer, sizeof(buffer));
    CHECK(length > 0u);
    CHECK(strstr(buffer, "NORMAL") != NULL);
}

int main(void)
{
    test_filters();
    test_profiles();
    test_feature_builder();
    test_assessment_normal();
    test_assessment_temp_trip_and_reset();
    test_assessment_overcurrent_and_reverse();
    test_assessment_sensor_faults();
    test_assessment_ml_and_comms();
    test_assessment_invalid_config();
    test_safety_controller();
    test_telemetry();

    if (failures != 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    printf("test_core: all tests passed\n");
    return 0;
}

#include "BatteryMonitor.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

int failures;

void check(bool condition, const char *text, int line)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL test_cpp.cpp:%d: %s\n", line, text);
        failures++;
    }
}

}  // namespace

#define CHECK(condition) check((condition), #condition, __LINE__)

int main()
{
    battery::BatteryMonitor monitor(BAT_PROFILE_BENCH_6V, 8, 0);
    CHECK(monitor.valid());

    bat_raw_sample_t raw;
    bat_features_t features;
    std::memset(&raw, 0, sizeof(raw));
    std::memset(&features, 0, sizeof(features));
    raw.temp_valid_mask = 0x00FFu;
    raw.channel_count = 8;
    raw.voltage_v = 6.2f;
    raw.current_a = 0.3f;
    raw.rain_analog = 500.0f;
    raw.rain_digital_dry = 1u;
    raw.comms_ok = 1u;
    raw.self_test_ok = 1u;
    features.num_temp = 8;
    features.temp_valid_mask = 0x00FFu;
    features.voltage_v = 6.2f;
    features.current_a = 0.3f;
    features.rain_pct = 48.9f;
    features.t_min = 45.0f;
    features.t_max = 45.0f;
    features.t_avg = 45.0f;
    features.humidity_valid = 0;
    features.humidity_pct = NAN;
    features.resistance_ohm = NAN;
    for (int i = 0; i < 8; i++) {
        raw.temp_c[i] = 45.0f;
        raw.temp_status[i] = SENSOR_STATUS_OK;
        features.temp_c[i] = 45.0f;
    }

    rf_result_t ml;
    ml.p_unsafe = 0.1f;
    ml.label = BAT_LABEL_SAFE;
    ml.valid = 1;
    ml.low_confidence = 0;

    for (int step = 0; step < 6; step++) {
        std::uint32_t now = static_cast<std::uint32_t>(step * 1000);
        features.age_ms = now;
        raw.timestamp_ms = now;
        monitor.update(raw, features, ml, now);
    }
    CHECK(monitor.assessment().state == BAT_STATE_NORMAL);
    CHECK(monitor.assessment().relay_allowed == 1u);

    sc_inputs_t inputs;
    std::memset(&inputs, 0, sizeof(inputs));
    inputs.self_test_ok = 1u;
    inputs.watchdog_ok = 1u;
    inputs.heartbeat_ok = 1u;
    monitor.safetyUpdate(inputs, 6000);
    CHECK(monitor.outputs().relay_closed == 1u);

    char buffer[512];
    std::size_t length = monitor.json(1, buffer, sizeof(buffer));
    CHECK(length > 0u);
    CHECK(std::strstr(buffer, "NORMAL") != nullptr);

    if (failures != 0) {
        std::fprintf(stderr, "%d C++ test(s) failed\n", failures);
        return 1;
    }
    std::printf("test_cpp: all tests passed\n");
    return 0;
}

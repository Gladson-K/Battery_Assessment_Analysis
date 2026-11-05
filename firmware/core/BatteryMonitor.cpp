#include "BatteryMonitor.hpp"

#include <cstring>

namespace battery {

BatteryMonitor::BatteryMonitor(const ba_config_t &config, std::uint32_t now_ms)
    : valid_(false)
{
    std::memset(&output_, 0, sizeof(output_));
    std::memset(&safety_output_, 0, sizeof(safety_output_));
    std::memset(&last_features_, 0, sizeof(last_features_));
    std::memset(&last_raw_, 0, sizeof(last_raw_));
    valid_ = ba_init(&assessment_, &config, now_ms);
    sc_init(&safety_, now_ms);
}

BatteryMonitor::BatteryMonitor(bat_profile_t profile, std::uint8_t num_temp,
                               std::uint32_t now_ms)
    : valid_(false)
{
    ba_config_t config;
    std::memset(&config, 0, sizeof(config));
    std::memset(&output_, 0, sizeof(output_));
    std::memset(&safety_output_, 0, sizeof(safety_output_));
    std::memset(&last_features_, 0, sizeof(last_features_));
    std::memset(&last_raw_, 0, sizeof(last_raw_));
    if (ba_config_from_profile(&config, profile, num_temp)) {
        valid_ = ba_init(&assessment_, &config, now_ms);
    } else {
        valid_ = ba_init(&assessment_, nullptr, now_ms);
    }
    sc_init(&safety_, now_ms);
}

bool BatteryMonitor::valid() const
{
    return valid_ && assessment_.config_valid != 0u;
}

void BatteryMonitor::update(const bat_raw_sample_t &raw, const bat_features_t &features,
                            const rf_result_t &ml, std::uint32_t now_ms)
{
    last_raw_ = raw;
    last_features_ = features;
    ba_update(&assessment_, &raw, &features, &ml, now_ms, &output_);
}

void BatteryMonitor::requestReset()
{
    ba_manual_reset(&assessment_);
}

void BatteryMonitor::safetyUpdate(const sc_inputs_t &inputs, std::uint32_t now_ms)
{
    sc_update(&safety_, &output_, &inputs, now_ms, &safety_output_);
}

const bat_assessment_t &BatteryMonitor::assessment() const
{
    return output_;
}

const sc_outputs_t &BatteryMonitor::outputs() const
{
    return safety_output_;
}

const ba_config_t &BatteryMonitor::config() const
{
    return assessment_.config;
}

std::size_t BatteryMonitor::json(std::uint16_t sequence, char *buffer, std::size_t capacity) const
{
    return telemetry_build_json(&last_raw_, &last_features_, &output_, sequence, buffer, capacity);
}

std::size_t BatteryMonitor::csv(std::uint16_t sequence, char *buffer, std::size_t capacity) const
{
    return telemetry_build_csv(&last_raw_, &last_features_, &output_, sequence, buffer, capacity);
}

}  // namespace battery

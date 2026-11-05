#ifndef BATTERY_MONITOR_HPP
#define BATTERY_MONITOR_HPP

#include "battery_assessment.h"
#include "battery_profiles.h"
#include "battery_types.h"
#include "safety_controller.h"
#include "telemetry.h"

#include <cstddef>
#include <cstdint>

namespace battery {

class BatteryMonitor {
public:
    BatteryMonitor(const ba_config_t &config, std::uint32_t now_ms);
    explicit BatteryMonitor(bat_profile_t profile, std::uint8_t num_temp, std::uint32_t now_ms);

    bool valid() const;
    void update(const bat_raw_sample_t &raw, const bat_features_t &features,
                const rf_result_t &ml, std::uint32_t now_ms);
    void requestReset();
    void safetyUpdate(const sc_inputs_t &inputs, std::uint32_t now_ms);

    const bat_assessment_t &assessment() const;
    const sc_outputs_t &outputs() const;
    const ba_config_t &config() const;

    std::size_t json(std::uint16_t sequence, char *buffer, std::size_t capacity) const;
    std::size_t csv(std::uint16_t sequence, char *buffer, std::size_t capacity) const;

private:
    ba_t assessment_;
    bat_assessment_t output_;
    sc_t safety_;
    sc_outputs_t safety_output_;
    bat_features_t last_features_;
    bat_raw_sample_t last_raw_;
    bool valid_;
};

}  // namespace battery

#endif

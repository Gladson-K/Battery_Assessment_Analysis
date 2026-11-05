#include "acs712.h"

#include "adc_reader.h"

acs712_t acs712_default(void)
{
    acs712_t sensor;
    sensor.sensitivity_v_per_a = BOARD_ACS712_SENSITIVITY;
    sensor.zero_volts = BOARD_ACS712_ZERO_V;
    sensor.divider_ratio = BOARD_ACS712_DIVIDER_RATIO;
    return sensor;
}

bool acs712_calibrate(acs712_t *sensor, uint16_t samples)
{
    if (sensor == 0 || samples == 0) {
        return false;
    }
    float sum = 0.0f;
    uint16_t valid = 0;
    for (uint16_t i = 0; i < samples; ++i) {
        uint16_t raw = 0;
        if (adc_read_channel(&hadc1, BOARD_ADC_CHANNEL_CURRENT, &raw)) {
            if (raw > 4u && raw < 4092u) {
                sum += (float)raw * BOARD_ADC_VREF / BOARD_ADC_MAX;
                ++valid;
            }
        }
        board_delay_ms(2);
    }
    if (valid < samples / 2u) {
        return false;
    }
    sensor->zero_volts = sum / (float)valid;
    return true;
}

sensor_value_t acs712_read(const acs712_t *sensor)
{
    uint16_t raw = 0;
    if (sensor == 0) {
        return sensor_value_make(0.0f, SENSOR_STATUS_NOISY);
    }
    if (!adc_read_channel(&hadc1, BOARD_ADC_CHANNEL_CURRENT, &raw)) {
        return sensor_value_make(0.0f, SENSOR_STATUS_NOISY);
    }
    if (raw >= 4093u || raw <= 2u) {
        return sensor_value_make(0.0f, SENSOR_STATUS_RAIL);
    }
    float volts = (float)raw * BOARD_ADC_VREF / BOARD_ADC_MAX;
    float current = (volts - sensor->zero_volts) / sensor->sensitivity_v_per_a;
    if (current < -30.0f || current > 30.0f) {
        return sensor_value_make(current, SENSOR_STATUS_OUT_OF_RANGE);
    }
    return sensor_value_make(current, SENSOR_STATUS_OK);
}

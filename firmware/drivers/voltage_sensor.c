#include "voltage_sensor.h"

#include "adc_reader.h"
#include "board_config.h"

voltage_sensor_t voltage_sensor_default(void)
{
    voltage_sensor_t sensor;
    sensor.divider_ratio = BOARD_VOLTAGE_DIVIDER_RATIO;
    sensor.offset_v = 0.0f;
    return sensor;
}

sensor_value_t voltage_sensor_read(const voltage_sensor_t *sensor)
{
    uint16_t raw = 0;
    if (!adc_read_channel(&hadc1, BOARD_ADC_CHANNEL_VOLTAGE, &raw)) {
        return sensor_value_make(0.0f, SENSOR_STATUS_NOISY);
    }
    if (raw >= 4093u) {
        return sensor_value_make(0.0f, SENSOR_STATUS_RAIL);
    }
    float ratio = sensor != 0 ? sensor->divider_ratio : BOARD_VOLTAGE_DIVIDER_RATIO;
    float offset = sensor != 0 ? sensor->offset_v : 0.0f;
    float volts = ((float)raw * BOARD_ADC_VREF / BOARD_ADC_MAX) * ratio + offset;
    if (volts < 0.0f || volts > 80.0f) {
        return sensor_value_make(volts, SENSOR_STATUS_OUT_OF_RANGE);
    }
    return sensor_value_make(volts, SENSOR_STATUS_OK);
}

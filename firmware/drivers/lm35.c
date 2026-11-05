#include "lm35.h"

#include "adc_reader.h"
#include "board_config.h"

lm35_t lm35_default(void)
{
    lm35_t sensor;
    sensor.offset_c = 0.0f;
    return sensor;
}

sensor_value_t lm35_read(const mux74hc4067_t *mux, uint8_t channel, const lm35_t *sensor)
{
    uint16_t raw = 0;
    if (channel > 15u) {
        return sensor_value_make(0.0f, SENSOR_STATUS_OUT_OF_RANGE);
    }
    mux74hc4067_select(mux, channel);
    if (!adc_read_channel(&hadc1, BOARD_ADC_CHANNEL_MUX, &raw)) {
        return sensor_value_make(0.0f, SENSOR_STATUS_NOISY);
    }
    if (raw <= 1u) {
        return sensor_value_make(0.0f, SENSOR_STATUS_OPEN);
    }
    if (raw >= 4094u) {
        return sensor_value_make(0.0f, SENSOR_STATUS_RAIL);
    }
    float volts = ((float)raw * BOARD_ADC_VREF / BOARD_ADC_MAX) + BOARD_ADC_CAL_OFFSET;
    float temperature = volts * 100.0f + (sensor != 0 ? sensor->offset_c : 0.0f);
    if (temperature < -2.0f || temperature > 150.0f) {
        return sensor_value_make(temperature, SENSOR_STATUS_OUT_OF_RANGE);
    }
    return sensor_value_make(temperature, SENSOR_STATUS_OK);
}

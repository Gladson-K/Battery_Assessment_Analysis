#include "ntc.h"

#include "adc_reader.h"
#include "board_config.h"

#include <math.h>

ntc_t ntc_default(void)
{
    ntc_t sensor;
    sensor.beta = BOARD_NTC_BETA;
    sensor.series_resistance = BOARD_NTC_SERIES_R;
    sensor.nominal_resistance = BOARD_NTC_NOMINAL_R;
    sensor.nominal_temperature_c = BOARD_NTC_NOMINAL_T_C;
    sensor.offset_c = 0.0f;
    return sensor;
}

sensor_value_t ntc_read(const ntc_t *sensor, uint32_t adc_channel)
{
    if (sensor == 0) {
        return sensor_value_make(0.0f, SENSOR_STATUS_NOISY);
    }
    uint16_t raw = 0;
    if (!adc_read_channel(&hadc1, adc_channel, &raw)) {
        return sensor_value_make(0.0f, SENSOR_STATUS_NOISY);
    }
    if (raw <= 2u) {
        return sensor_value_make(0.0f, SENSOR_STATUS_SHORT);
    }
    if (raw >= 4093u) {
        return sensor_value_make(0.0f, SENSOR_STATUS_OPEN);
    }

    float resistance =
        sensor->series_resistance * (float)raw / (BOARD_ADC_MAX - (float)raw);
    const float inv_nominal = 1.0f / (sensor->nominal_temperature_c + 273.15f);
    float inv_temperature =
        (1.0f / sensor->beta) * logf(resistance / sensor->nominal_resistance) + inv_nominal;
    if (inv_temperature <= 0.0f) {
        return sensor_value_make(0.0f, SENSOR_STATUS_OUT_OF_RANGE);
    }
    float temperature = (1.0f / inv_temperature) - 273.15f + sensor->offset_c;
    if (temperature < -55.0f || temperature > 150.0f) {
        return sensor_value_make(temperature, SENSOR_STATUS_OUT_OF_RANGE);
    }
    return sensor_value_make(temperature, SENSOR_STATUS_OK);
}

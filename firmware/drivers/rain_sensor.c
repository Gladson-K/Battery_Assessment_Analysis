#include "rain_sensor.h"

#include "adc_reader.h"
#include "board.h"

rain_reading_t rain_sensor_read(void)
{
    rain_reading_t reading;
    reading.wet_percent = 0.0f;
    reading.raw_10bit = 0;
    reading.digital_dry = board_rain_digital_dry();
    reading.status = SENSOR_STATUS_OK;

    uint16_t raw = 0;
    if (!adc_read_channel(&hadc1, BOARD_ADC_CHANNEL_RAIN, &raw)) {
        reading.status = SENSOR_STATUS_NOISY;
        return reading;
    }
    if (raw >= 4093u) {
        reading.status = SENSOR_STATUS_RAIL;
        return reading;
    }
    uint16_t raw10 = (uint16_t)(raw >> 2);
    reading.raw_10bit = raw10;
    reading.wet_percent = (float)raw10 * 100.0f / 1023.0f;
    return reading;
}

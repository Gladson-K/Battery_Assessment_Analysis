#include "sht31.h"

#include "crc8.h"
#include "board.h"

#define SHT31_CMD_SOFT_RESET 0x30A2u
#define SHT31_CMD_SINGLE_SHOT 0x2400u

static bool sht31_command(const sht31_t *sensor, uint16_t command)
{
    uint8_t data[2] = {(uint8_t)(command >> 8), (uint8_t)(command & 0xFFu)};
    return HAL_I2C_Master_Transmit(sensor->i2c, (uint16_t)(sensor->address << 1), data, 2, 20) ==
           HAL_OK;
}

bool sht31_init(sht31_t *sensor)
{
    if (sensor == 0 || sensor->i2c == 0) {
        return false;
    }
    if (!sht31_command(sensor, SHT31_CMD_SOFT_RESET)) {
        return false;
    }
    board_delay_ms(3);
    return true;
}

sht31_reading_t sht31_read(const sht31_t *sensor)
{
    sht31_reading_t reading;
    reading.temperature = sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    reading.humidity = sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    reading.valid = 0;

    if (sensor == 0 || sensor->i2c == 0) {
        return reading;
    }
    if (!sht31_command(sensor, SHT31_CMD_SINGLE_SHOT)) {
        return reading;
    }
    board_delay_ms(20);

    uint8_t data[6] = {0};
    if (HAL_I2C_Master_Receive(sensor->i2c, (uint16_t)((sensor->address << 1) | 1u), data, 6,
                               20) != HAL_OK) {
        return reading;
    }
    if (crc8_maxim(&data[0], 2) != data[2] || crc8_maxim(&data[3], 2) != data[5]) {
        reading.temperature.status = SENSOR_STATUS_NOISY;
        reading.humidity.status = SENSOR_STATUS_NOISY;
        return reading;
    }

    uint16_t raw_temperature = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    uint16_t raw_humidity = (uint16_t)(((uint16_t)data[3] << 8) | data[4]);
    float temperature = -45.0f + 175.0f * (float)raw_temperature / 65535.0f;
    float humidity = 100.0f * (float)raw_humidity / 65535.0f;
    reading.temperature =
        sensor_value_make(temperature,
                          (temperature < -55.0f || temperature > 150.0f)
                              ? SENSOR_STATUS_OUT_OF_RANGE
                              : SENSOR_STATUS_OK);
    reading.humidity =
        sensor_value_make(humidity,
                          (humidity < 0.0f || humidity > 100.0f) ? SENSOR_STATUS_OUT_OF_RANGE
                                                                : SENSOR_STATUS_OK);
    reading.valid = 1;
    return reading;
}

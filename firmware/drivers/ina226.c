#include "ina226.h"

#include <math.h>

#define INA226_REG_CONFIG 0x00u
#define INA226_REG_BUS_VOLTAGE 0x02u
#define INA226_REG_POWER 0x03u
#define INA226_REG_CURRENT 0x04u
#define INA226_REG_CALIBRATION 0x05u
#define INA226_REG_MANUFACTURER 0xFEu
#define INA226_REG_DIE 0xFFu

#define INA226_CONFIG 0x4127u
#define INA226_MANUFACTURER_ID 0x5449u
#define INA226_DIE_ID 0x2260u

static bool ina226_write_reg(const ina226_t *sensor, uint8_t reg, uint16_t value)
{
    uint8_t data[2] = {(uint8_t)(value >> 8), (uint8_t)(value & 0xFFu)};
    return HAL_I2C_Mem_Write(sensor->i2c, (uint16_t)(sensor->address << 1), reg,
                             I2C_MEMADD_SIZE_8BIT, data, 2, 20) == HAL_OK;
}

static bool ina226_read_reg(const ina226_t *sensor, uint8_t reg, uint16_t *value)
{
    uint8_t data[2] = {0};
    if (HAL_I2C_Mem_Read(sensor->i2c, (uint16_t)(sensor->address << 1), reg,
                         I2C_MEMADD_SIZE_8BIT, data, 2, 20) != HAL_OK) {
        return false;
    }
    *value = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    return true;
}

bool ina226_init(ina226_t *sensor)
{
    if (sensor == 0 || sensor->i2c == 0 || sensor->shunt_resistance <= 0.0f ||
        sensor->max_current <= 0.0f) {
        return false;
    }
    uint16_t manufacturer = 0;
    uint16_t die = 0;
    if (!ina226_read_reg(sensor, INA226_REG_MANUFACTURER, &manufacturer) ||
        !ina226_read_reg(sensor, INA226_REG_DIE, &die)) {
        return false;
    }
    if (manufacturer != INA226_MANUFACTURER_ID || die != INA226_DIE_ID) {
        return false;
    }

    sensor->current_lsb = sensor->max_current / 32768.0f;
    float calibration = 0.00512f / (sensor->current_lsb * sensor->shunt_resistance);
    if (calibration > 65535.0f || calibration < 1.0f) {
        return false;
    }
    if (!ina226_write_reg(sensor, INA226_REG_CONFIG, INA226_CONFIG)) {
        return false;
    }
    return ina226_write_reg(sensor, INA226_REG_CALIBRATION, (uint16_t)calibration);
}

ina226_reading_t ina226_read(const ina226_t *sensor)
{
    ina226_reading_t reading;
    reading.bus_voltage = sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    reading.current = sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    reading.power = sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    reading.valid = 0;

    uint16_t raw_bus = 0;
    uint16_t raw_current = 0;
    uint16_t raw_power = 0;
    if (sensor == 0 || sensor->i2c == 0 ||
        !ina226_read_reg(sensor, INA226_REG_BUS_VOLTAGE, &raw_bus) ||
        !ina226_read_reg(sensor, INA226_REG_CURRENT, &raw_current) ||
        !ina226_read_reg(sensor, INA226_REG_POWER, &raw_power)) {
        return reading;
    }

    float voltage = (float)raw_bus * 0.00125f;
    float current = (float)(int16_t)raw_current * sensor->current_lsb;
    float power = (float)raw_power * 25.0f * sensor->current_lsb;

    reading.bus_voltage =
        sensor_value_make(voltage, (voltage > 36.0f || voltage < 0.0f)
                                       ? SENSOR_STATUS_OUT_OF_RANGE
                                       : SENSOR_STATUS_OK);
    reading.current =
        sensor_value_make(current, (fabsf(current) > sensor->max_current * 1.2f)
                                       ? SENSOR_STATUS_OUT_OF_RANGE
                                       : SENSOR_STATUS_OK);
    reading.power = sensor_value_make(power, SENSOR_STATUS_OK);
    reading.valid = 1;
    return reading;
}

#include "ina228.h"

#include "ina228_math.h"

#include <math.h>

#define INA228_REG_CONFIG 0x00u
#define INA228_REG_ADC_CONFIG 0x01u
#define INA228_REG_SHUNT_CAL 0x02u
#define INA228_REG_VBUS 0x05u
#define INA228_REG_CURRENT 0x07u
#define INA228_REG_POWER 0x08u
#define INA228_REG_MANUFACTURER 0x3Eu
#define INA228_REG_DEVICE 0x3Fu

#define INA228_CONFIG 0x0000u
#define INA228_ADC_CONFIG 0xFB69u
#define INA228_MANUFACTURER_ID 0x5449u
#define INA228_DEVICE_ID_MASK 0xFFF0u
#define INA228_DEVICE_ID 0x2280u
#define INA228_MAX_BUS_VOLTAGE_V 85.0f

static bool ina228_write_reg(const ina228_t *sensor, uint8_t reg, uint16_t value)
{
    uint8_t data[2] = {(uint8_t)(value >> 8), (uint8_t)(value & 0xFFu)};
    return HAL_I2C_Mem_Write(sensor->i2c, (uint16_t)(sensor->address << 1), reg,
                             I2C_MEMADD_SIZE_8BIT, data, 2, 20) == HAL_OK;
}

static bool ina228_read_reg16(const ina228_t *sensor, uint8_t reg, uint16_t *value)
{
    uint8_t data[2] = {0};
    if (HAL_I2C_Mem_Read(sensor->i2c, (uint16_t)(sensor->address << 1), reg,
                         I2C_MEMADD_SIZE_8BIT, data, 2, 20) != HAL_OK) {
        return false;
    }
    *value = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    return true;
}

static bool ina228_read_reg24(const ina228_t *sensor, uint8_t reg, uint32_t *value)
{
    uint8_t data[3] = {0};
    if (HAL_I2C_Mem_Read(sensor->i2c, (uint16_t)(sensor->address << 1), reg,
                         I2C_MEMADD_SIZE_8BIT, data, 3, 20) != HAL_OK) {
        return false;
    }
    *value = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    return true;
}

bool ina228_init(ina228_t *sensor)
{
    if (sensor == 0 || sensor->i2c == 0 || sensor->shunt_resistance <= 0.0f ||
        sensor->max_current <= 0.0f) {
        return false;
    }

    uint16_t manufacturer = 0;
    uint16_t device = 0;
    if (!ina228_read_reg16(sensor, INA228_REG_MANUFACTURER, &manufacturer) ||
        !ina228_read_reg16(sensor, INA228_REG_DEVICE, &device)) {
        return false;
    }
    if (manufacturer != INA228_MANUFACTURER_ID ||
        (device & INA228_DEVICE_ID_MASK) != INA228_DEVICE_ID) {
        return false;
    }

    float current_lsb = 0.0f;
    uint16_t shunt_cal = 0;
    if (!ina228_calibrate(sensor->max_current, sensor->shunt_resistance, &current_lsb,
                          &shunt_cal)) {
        return false;
    }
    sensor->current_lsb = current_lsb;

    if (!ina228_write_reg(sensor, INA228_REG_CONFIG, INA228_CONFIG) ||
        !ina228_write_reg(sensor, INA228_REG_ADC_CONFIG, INA228_ADC_CONFIG)) {
        return false;
    }
    return ina228_write_reg(sensor, INA228_REG_SHUNT_CAL, shunt_cal);
}

ina228_reading_t ina228_read(const ina228_t *sensor)
{
    ina228_reading_t reading;
    reading.bus_voltage = sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    reading.current = sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    reading.power = sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    reading.valid = 0;

    uint32_t raw_bus = 0;
    uint32_t raw_current = 0;
    uint32_t raw_power = 0;
    if (sensor == 0 || sensor->i2c == 0 ||
        !ina228_read_reg24(sensor, INA228_REG_VBUS, &raw_bus) ||
        !ina228_read_reg24(sensor, INA228_REG_CURRENT, &raw_current) ||
        !ina228_read_reg24(sensor, INA228_REG_POWER, &raw_power)) {
        return reading;
    }

    float voltage = ina228_vbus_voltage(raw_bus);
    float current = ina228_current_a(raw_current, sensor->current_lsb);
    float power = ina228_power_w(raw_power, sensor->current_lsb);

    reading.bus_voltage =
        sensor_value_make(voltage, (voltage > INA228_MAX_BUS_VOLTAGE_V || voltage < 0.0f)
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

#include "tmp117.h"

#define TMP117_REG_TEMPERATURE 0x00u
#define TMP117_REG_DEVICE_ID 0x0Fu
#define TMP117_DEVICE_ID 0x0117u

bool tmp117_init(tmp117_t *sensor)
{
    if (sensor == 0 || sensor->i2c == 0) {
        return false;
    }
    uint8_t id[2] = {0};
    if (HAL_I2C_Mem_Read(sensor->i2c, (uint16_t)(sensor->address << 1), TMP117_REG_DEVICE_ID,
                         I2C_MEMADD_SIZE_8BIT, id, 2, 20) != HAL_OK) {
        return false;
    }
    uint16_t device_id = (uint16_t)(((uint16_t)id[0] << 8) | id[1]);
    return (device_id & 0x0FFFu) == TMP117_DEVICE_ID;
}

sensor_value_t tmp117_read(const tmp117_t *sensor)
{
    if (sensor == 0 || sensor->i2c == 0) {
        return sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    }
    uint8_t data[2] = {0};
    if (HAL_I2C_Mem_Read(sensor->i2c, (uint16_t)(sensor->address << 1), TMP117_REG_TEMPERATURE,
                         I2C_MEMADD_SIZE_8BIT, data, 2, 20) != HAL_OK) {
        return sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    }
    int16_t raw = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
    float temperature = (float)raw * 0.0078125f;
    if (temperature < -55.0f || temperature > 150.0f) {
        return sensor_value_make(temperature, SENSOR_STATUS_OUT_OF_RANGE);
    }
    return sensor_value_make(temperature, SENSOR_STATUS_OK);
}

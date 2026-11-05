#include "mlx90614.h"

#include "crc8.h"

#define MLX90614_REG_OBJECT 0x07u

static bool mlx90614_read_raw(const mlx90614_t *sensor, uint16_t *raw)
{
    uint8_t data[3] = {0};
    if (HAL_I2C_Mem_Read(sensor->i2c, (uint16_t)(sensor->address << 1), MLX90614_REG_OBJECT,
                         I2C_MEMADD_SIZE_8BIT, data, 3, 20) != HAL_OK) {
        return false;
    }
    uint8_t packet[5] = {
        (uint8_t)(sensor->address << 1),
        MLX90614_REG_OBJECT,
        (uint8_t)((sensor->address << 1) | 1u),
        data[0],
        data[1],
    };
    if (crc8_smbus(packet, sizeof(packet)) != data[2]) {
        return false;
    }
    *raw = (uint16_t)(((uint16_t)data[1] << 8) | data[0]);
    return true;
}

bool mlx90614_init(mlx90614_t *sensor)
{
    if (sensor == 0 || sensor->i2c == 0) {
        return false;
    }
    uint16_t raw = 0;
    if (!mlx90614_read_raw(sensor, &raw)) {
        return false;
    }
    return raw != 0u && raw != 0xFFFFu;
}

sensor_value_t mlx90614_read_object(const mlx90614_t *sensor)
{
    if (sensor == 0 || sensor->i2c == 0) {
        return sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    }
    uint16_t raw = 0;
    if (!mlx90614_read_raw(sensor, &raw)) {
        return sensor_value_make(0.0f, SENSOR_STATUS_NOISY);
    }
    if (raw == 0u || raw == 0xFFFFu) {
        return sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    }
    float temperature = (float)raw * 0.02f - 273.15f;
    if (temperature < -70.0f || temperature > 380.0f) {
        return sensor_value_make(temperature, SENSOR_STATUS_OUT_OF_RANGE);
    }
    return sensor_value_make(temperature, SENSOR_STATUS_OK);
}

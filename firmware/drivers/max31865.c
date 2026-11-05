#include "max31865.h"

#include "board.h"

#include <math.h>

#define MAX31865_REG_CONFIG 0x00u
#define MAX31865_REG_RTD_MSB 0x01u
#define MAX31865_REG_FAULT 0x07u

static void max31865_cs(const max31865_t *sensor, uint8_t asserted)
{
    HAL_GPIO_WritePin(sensor->cs_port, sensor->cs_pin,
                      asserted ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static uint8_t max31865_transfer(const max31865_t *sensor, uint8_t value)
{
    uint8_t received = 0;
    HAL_SPI_TransmitReceive(sensor->spi, &value, &received, 1, 20);
    return received;
}

static void max31865_write_reg(const max31865_t *sensor, uint8_t reg, uint8_t value)
{
    max31865_cs(sensor, 1);
    (void)max31865_transfer(sensor, (uint8_t)(reg & 0x7Fu));
    (void)max31865_transfer(sensor, value);
    max31865_cs(sensor, 0);
}

static uint8_t max31865_read_reg(const max31865_t *sensor, uint8_t reg)
{
    max31865_cs(sensor, 1);
    (void)max31865_transfer(sensor, reg);
    uint8_t value = max31865_transfer(sensor, 0x00u);
    max31865_cs(sensor, 0);
    return value;
}

static float pt100_temperature(float resistance, float nominal_resistance)
{
    const float a = 3.9083e-3f;
    const float b = -5.775e-7f;
    const float c = -4.183e-12f;
    float ratio = resistance / nominal_resistance;
    if (ratio >= 1.0f) {
        return (-a + sqrtf(a * a - 4.0f * b * (1.0f - ratio))) / (2.0f * b);
    }
    float temperature = (ratio - 1.0f) / a;
    for (uint8_t i = 0; i < 8u; ++i) {
        float function =
            1.0f + a * temperature + b * temperature * temperature +
            c * (temperature - 100.0f) * temperature * temperature * temperature - ratio;
        float derivative =
            a + 2.0f * b * temperature +
            c * (4.0f * temperature * temperature * temperature -
                 300.0f * temperature * temperature);
        if (fabsf(derivative) < 1e-9f) {
            break;
        }
        temperature -= function / derivative;
    }
    return temperature;
}

bool max31865_init(max31865_t *sensor)
{
    if (sensor == 0 || sensor->spi == 0 || sensor->cs_port == 0) {
        return false;
    }
    uint8_t config = sensor->three_wire ? 0xD0u : 0xC0u;
    max31865_write_reg(sensor, MAX31865_REG_CONFIG, config);
    board_delay_ms(70);
    return true;
}

sensor_value_t max31865_read(const max31865_t *sensor)
{
    if (sensor == 0 || sensor->spi == 0) {
        return sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    }
    uint8_t config = sensor->three_wire ? 0xD0u : 0xC0u;
    max31865_write_reg(sensor, MAX31865_REG_CONFIG, config);
    board_delay_ms(70);

    uint8_t msb = max31865_read_reg(sensor, MAX31865_REG_RTD_MSB);
    uint8_t lsb = max31865_read_reg(sensor, MAX31865_REG_RTD_MSB + 1u);
    uint16_t raw = (uint16_t)(((uint16_t)msb << 8) | lsb);
    if ((raw & 0x0001u) != 0u) {
        uint8_t fault = max31865_read_reg(sensor, MAX31865_REG_FAULT);
        return sensor_value_make(0.0f, (fault & 0x04u) ? SENSOR_STATUS_SHORT
                                                       : SENSOR_STATUS_OPEN);
    }

    uint16_t rtd = (uint16_t)(raw >> 1);
    float resistance = (float)rtd * sensor->reference_resistance / 32768.0f;
    if (resistance < 10.0f) {
        return sensor_value_make(0.0f, SENSOR_STATUS_SHORT);
    }
    if (resistance > 400.0f) {
        return sensor_value_make(0.0f, SENSOR_STATUS_OPEN);
    }
    float temperature = pt100_temperature(resistance, sensor->nominal_resistance);
    if (temperature < -200.0f || temperature > 850.0f) {
        return sensor_value_make(temperature, SENSOR_STATUS_OUT_OF_RANGE);
    }
    return sensor_value_make(temperature, SENSOR_STATUS_OK);
}

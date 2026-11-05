#ifndef INA226_H
#define INA226_H

#include "sensor_common.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    I2C_HandleTypeDef *i2c;
    uint8_t address;
    float shunt_resistance;
    float current_lsb;
    float max_current;
} ina226_t;

typedef struct {
    sensor_value_t bus_voltage;
    sensor_value_t current;
    sensor_value_t power;
    uint8_t valid;
} ina226_reading_t;

bool ina226_init(ina226_t *sensor);
ina226_reading_t ina226_read(const ina226_t *sensor);

#ifdef __cplusplus
}
#endif

#endif

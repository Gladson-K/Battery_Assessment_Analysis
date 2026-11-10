#ifndef INA228_H
#define INA228_H

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
} ina228_t;

typedef struct {
    sensor_value_t bus_voltage;
    sensor_value_t current;
    sensor_value_t power;
    uint8_t valid;
} ina228_reading_t;

bool ina228_init(ina228_t *sensor);
ina228_reading_t ina228_read(const ina228_t *sensor);

#ifdef __cplusplus
}
#endif

#endif

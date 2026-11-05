#ifndef SHT31_H
#define SHT31_H

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
} sht31_t;

typedef struct {
    sensor_value_t temperature;
    sensor_value_t humidity;
    uint8_t valid;
} sht31_reading_t;

bool sht31_init(sht31_t *sensor);
sht31_reading_t sht31_read(const sht31_t *sensor);

#ifdef __cplusplus
}
#endif

#endif

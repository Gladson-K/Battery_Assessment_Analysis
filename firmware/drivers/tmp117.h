#ifndef TMP117_H
#define TMP117_H

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
} tmp117_t;

bool tmp117_init(tmp117_t *sensor);
sensor_value_t tmp117_read(const tmp117_t *sensor);

#ifdef __cplusplus
}
#endif

#endif

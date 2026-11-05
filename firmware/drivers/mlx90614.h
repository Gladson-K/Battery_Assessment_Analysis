#ifndef MLX90614_H
#define MLX90614_H

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
} mlx90614_t;

bool mlx90614_init(mlx90614_t *sensor);
sensor_value_t mlx90614_read_object(const mlx90614_t *sensor);

#ifdef __cplusplus
}
#endif

#endif

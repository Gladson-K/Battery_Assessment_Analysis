#ifndef MAX31865_H
#define MAX31865_H

#include "sensor_common.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    SPI_HandleTypeDef *spi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    float reference_resistance;
    float nominal_resistance;
    uint8_t three_wire;
} max31865_t;

bool max31865_init(max31865_t *sensor);
sensor_value_t max31865_read(const max31865_t *sensor);

#ifdef __cplusplus
}
#endif

#endif

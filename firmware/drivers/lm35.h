#ifndef LM35_H
#define LM35_H

#include "mux74hc4067.h"
#include "sensor_common.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float offset_c;
} lm35_t;

lm35_t lm35_default(void);
sensor_value_t lm35_read(const mux74hc4067_t *mux, uint8_t channel, const lm35_t *sensor);

#ifdef __cplusplus
}
#endif

#endif

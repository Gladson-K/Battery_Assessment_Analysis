#ifndef ACS712_H
#define ACS712_H

#include "board_config.h"
#include "sensor_common.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sensitivity_v_per_a;
    float zero_volts;
    float divider_ratio;
} acs712_t;

acs712_t acs712_default(void);
bool acs712_calibrate(acs712_t *sensor, uint16_t samples);
sensor_value_t acs712_read(const acs712_t *sensor);

#ifdef __cplusplus
}
#endif

#endif

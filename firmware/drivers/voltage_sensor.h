#ifndef VOLTAGE_SENSOR_H
#define VOLTAGE_SENSOR_H

#include "sensor_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float divider_ratio;
    float offset_v;
} voltage_sensor_t;

voltage_sensor_t voltage_sensor_default(void);
sensor_value_t voltage_sensor_read(const voltage_sensor_t *sensor);

#ifdef __cplusplus
}
#endif

#endif

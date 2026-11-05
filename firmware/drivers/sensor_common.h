#ifndef SENSOR_COMMON_H
#define SENSOR_COMMON_H

#include "battery_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float value;
    sensor_status_t status;
} sensor_value_t;

static inline sensor_value_t sensor_value_make(float value, sensor_status_t status)
{
    sensor_value_t result;
    result.value = value;
    result.status = status;
    return result;
}

#ifdef __cplusplus
}
#endif

#endif

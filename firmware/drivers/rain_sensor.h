#ifndef RAIN_SENSOR_H
#define RAIN_SENSOR_H

#include "sensor_common.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float wet_percent;
    uint16_t raw_10bit;
    uint8_t digital_dry;
    sensor_status_t status;
} rain_reading_t;

rain_reading_t rain_sensor_read(void);

#ifdef __cplusplus
}
#endif

#endif

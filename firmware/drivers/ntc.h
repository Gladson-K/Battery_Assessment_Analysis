#ifndef NTC_H
#define NTC_H

#include "sensor_common.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float beta;
    float series_resistance;
    float nominal_resistance;
    float nominal_temperature_c;
    float offset_c;
} ntc_t;

ntc_t ntc_default(void);
sensor_value_t ntc_read(const ntc_t *sensor, uint32_t adc_channel);

#ifdef __cplusplus
}
#endif

#endif

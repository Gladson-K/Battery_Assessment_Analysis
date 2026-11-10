#ifndef INA228_MATH_H
#define INA228_MATH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INA228_CURRENT_LSB_DIVISOR 524288.0f
#define INA228_SHUNT_CAL_FACTOR 13107.2e6f
#define INA228_VBUS_LSB_V 0.0001953125f
#define INA228_POWER_LSB_FACTOR 3.2f
#define INA228_TEMP_LSB_C 0.0078125f

bool ina228_calibrate(float max_current_a, float shunt_ohm, float *current_lsb,
                      uint16_t *shunt_cal);
float ina228_vbus_voltage(uint32_t raw24);
float ina228_current_a(uint32_t raw24, float current_lsb);
float ina228_power_w(uint32_t raw24, float current_lsb);
float ina228_die_temp_c(uint16_t raw16);

#ifdef __cplusplus
}
#endif

#endif

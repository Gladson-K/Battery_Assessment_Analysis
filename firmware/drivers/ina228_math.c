#include "ina228_math.h"

#include <math.h>

bool ina228_calibrate(float max_current_a, float shunt_ohm, float *current_lsb,
                      uint16_t *shunt_cal)
{
    if (current_lsb == 0 || shunt_cal == 0 || !isfinite(max_current_a) ||
        !isfinite(shunt_ohm) || max_current_a <= 0.0f || shunt_ohm <= 0.0f) {
        return false;
    }

    float lsb = max_current_a / INA228_CURRENT_LSB_DIVISOR;
    float calibration = INA228_SHUNT_CAL_FACTOR * lsb * shunt_ohm;
    if (!isfinite(lsb) || !isfinite(calibration)) {
        return false;
    }

    long rounded = lrintf(calibration);
    if (rounded < 1 || rounded > 32767) {
        return false;
    }

    *current_lsb = lsb;
    *shunt_cal = (uint16_t)rounded;
    return true;
}

float ina228_vbus_voltage(uint32_t raw24)
{
    uint32_t counts = (raw24 >> 4) & 0xFFFFFu;
    return (float)counts * INA228_VBUS_LSB_V;
}

float ina228_current_a(uint32_t raw24, float current_lsb)
{
    int32_t counts = (int32_t)((raw24 & 0xFFFFF0u) << 8) >> 12;
    return (float)counts * current_lsb;
}

float ina228_power_w(uint32_t raw24, float current_lsb)
{
    uint32_t counts = raw24 & 0xFFFFFFu;
    return (float)counts * INA228_POWER_LSB_FACTOR * current_lsb;
}

float ina228_die_temp_c(uint16_t raw16)
{
    return (float)(int16_t)raw16 * INA228_TEMP_LSB_C;
}

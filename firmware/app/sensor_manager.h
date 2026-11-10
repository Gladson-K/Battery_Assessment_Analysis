#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "acs712.h"
#include "battery_types.h"
#include "feature_builder.h"
#include "ina226.h"
#include "ina228.h"
#include "lcd_pcf8574.h"
#include "lm35.h"
#include "max31865.h"
#include "mlx90614.h"
#include "ntc.h"
#include "sht31.h"
#include "tmp117.h"
#include "voltage_sensor.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TEMP_SOURCE_NONE = 0,
    TEMP_SOURCE_LM35_MUX,
    TEMP_SOURCE_NTC_ADC,
    TEMP_SOURCE_MAX31865,
    TEMP_SOURCE_TMP117,
    TEMP_SOURCE_MLX90614
} temp_source_t;

typedef struct {
    temp_source_t source;
    uint8_t channel;
    uint8_t i2c_address;
} sm_temp_channel_t;

typedef struct {
    sm_temp_channel_t temp[BAT_MAX_TEMP_SENSORS];
    uint8_t temp_count;
    uint8_t use_voltage_divider;
    uint8_t use_acs712;
    uint8_t use_ina226;
    uint8_t use_ina228;
    uint8_t use_rain;
    uint8_t use_sht31;
    uint8_t use_lcd;
    uint8_t use_fan;
    float ina226_shunt_ohm;
    float ina226_max_current_a;
    float ina228_shunt_ohm;
    float ina228_max_current_a;
} sm_config_t;

typedef struct {
    sm_config_t config;
    lm35_t lm35;
    ntc_t ntc;
    voltage_sensor_t voltage;
    acs712_t acs712;
    max31865_t max31865;
    sht31_t sht31;
    ina226_t ina226;
    ina228_t ina228;
    lcd_pcf8574_t lcd;
    fb_t builder;
    uint8_t lcd_ok;
    uint8_t ina226_ok;
    uint8_t ina228_ok;
    uint8_t sht31_ok;
    uint8_t max31865_ok;
    uint8_t acs712_calibrated;
} sm_t;

bool sm_config_for_profile(sm_config_t *config, bat_profile_t profile);
bool sm_init(sm_t *manager, const sm_config_t *config, uint32_t now_ms);
bool sm_self_test(const sm_t *manager);
bool sm_calibrate_current(sm_t *manager);
void sm_read(sm_t *manager, bat_raw_sample_t *raw, bat_features_t *features, uint32_t now_ms);
lcd_pcf8574_t *sm_lcd(sm_t *manager);

#ifdef __cplusplus
}
#endif

#endif

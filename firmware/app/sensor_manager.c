#include "sensor_manager.h"

#include "app_config.h"
#include "board.h"
#include "board_config.h"
#include "rain_sensor.h"
#include "sensor_common.h"

#include <math.h>
#include <string.h>

static const uint32_t NTC_ADC_CHANNELS[] = BOARD_ADC_NTC_CHANNELS;

static bool temp_status_is_usable(sensor_status_t status)
{
    return status == SENSOR_STATUS_OK || status == SENSOR_STATUS_STUCK ||
           status == SENSOR_STATUS_NOISY;
}

static void set_temp(sm_temp_channel_t *channel, temp_source_t source, uint8_t mux_channel,
                     uint8_t address)
{
    channel->source = source;
    channel->channel = mux_channel;
    channel->i2c_address = address;
}

bool sm_config_for_profile(sm_config_t *config, bat_profile_t profile)
{
    if (config == 0) {
        return false;
    }
    memset(config, 0, sizeof(*config));
    switch (profile) {
    case BAT_PROFILE_BENCH_6V:
        config->temp_count = 8;
        for (uint8_t i = 0; i < 8; ++i) {
            set_temp(&config->temp[i], TEMP_SOURCE_LM35_MUX, i, 0);
        }
        config->use_voltage_divider = 1;
        config->use_acs712 = 1;
        config->use_rain = 1;
        config->use_lcd = 1;
        return true;
    case BAT_PROFILE_LFP_48V:
        config->temp_count = 10;
        for (uint8_t i = 0; i < 8; ++i) {
            set_temp(&config->temp[i], TEMP_SOURCE_MAX31865, i, 0);
        }
        set_temp(&config->temp[8], TEMP_SOURCE_TMP117, 0, BOARD_I2C_ADDR_TMP117);
        set_temp(&config->temp[9], TEMP_SOURCE_TMP117, 0, BOARD_I2C_ADDR_TMP117 + 1u);
        config->use_ina226 = 1;
        config->ina226_shunt_ohm = 0.0005f;
        config->ina226_max_current_a = 200.0f;
        config->use_fan = 1;
        return true;
    case BAT_PROFILE_LFP_24V:
        config->temp_count = 5;
        for (uint8_t i = 0; i < 4; ++i) {
            set_temp(&config->temp[i], TEMP_SOURCE_TMP117, 0,
                     (uint8_t)(BOARD_I2C_ADDR_TMP117 + i));
        }
        set_temp(&config->temp[4], TEMP_SOURCE_MLX90614, 0, BOARD_I2C_ADDR_MLX90614);
        config->use_ina226 = 1;
        config->ina226_shunt_ohm = 0.001f;
        config->ina226_max_current_a = 50.0f;
        config->use_sht31 = 1;
        config->use_lcd = 1;
        config->use_fan = 1;
        return true;
    case BAT_PROFILE_COLD_CHAIN:
        config->temp_count = 2;
        set_temp(&config->temp[0], TEMP_SOURCE_TMP117, 0, BOARD_I2C_ADDR_TMP117);
        set_temp(&config->temp[1], TEMP_SOURCE_TMP117, 0, BOARD_I2C_ADDR_TMP117 + 1u);
        config->use_voltage_divider = 1;
        config->use_acs712 = 1;
        config->use_sht31 = 1;
        config->use_lcd = 1;
        config->use_fan = 1;
        return true;
    case BAT_PROFILE_LEAD_12V:
    case BAT_PROFILE_CUSTOM:
    default:
        config->temp_count = 4;
        for (uint8_t i = 0; i < 4; ++i) {
            set_temp(&config->temp[i], TEMP_SOURCE_NTC_ADC, i, 0);
        }
        config->use_voltage_divider = 1;
        config->use_ina226 = 1;
        config->ina226_shunt_ohm = 0.001f;
        config->ina226_max_current_a = 50.0f;
        config->use_rain = 1;
        config->use_sht31 = 1;
        config->use_lcd = 1;
        config->use_fan = 1;
        return true;
    }
}

bool sm_init(sm_t *manager, const sm_config_t *config, uint32_t now_ms)
{
    if (manager == 0 || config == 0 || config->temp_count == 0 ||
        config->temp_count > BAT_MAX_TEMP_SENSORS) {
        return false;
    }
    memset(manager, 0, sizeof(*manager));
    manager->config = *config;
    manager->lm35 = lm35_default();
    manager->ntc = ntc_default();
    manager->voltage = voltage_sensor_default();
    manager->acs712 = acs712_default();
    manager->sht31.i2c = &hi2c1;
    manager->sht31.address = BOARD_I2C_ADDR_SHT31;
    manager->ina226.i2c = &hi2c1;
    manager->ina226.address = BOARD_I2C_ADDR_INA226;
    manager->ina226.shunt_resistance = config->ina226_shunt_ohm;
    manager->ina226.max_current = config->ina226_max_current_a;
    manager->lcd.i2c = &hi2c1;
    manager->lcd.address = BOARD_I2C_ADDR_LCD;
    manager->lcd.backlight = 1;

    manager->max31865.spi = &hspi2;
    manager->max31865.cs_port = BOARD_SPI_CS_PORT;
    manager->max31865.cs_pin = BOARD_SPI_CS_PIN;
    manager->max31865.reference_resistance = BOARD_MAX31865_RREF;
    manager->max31865.nominal_resistance = BOARD_MAX31865_RNOMINAL;
    manager->max31865.three_wire = 1;

    manager->max31865_ok = 1;
    for (uint8_t i = 0; i < config->temp_count; ++i) {
        if (config->temp[i].source == TEMP_SOURCE_MAX31865) {
            manager->max31865_ok = max31865_init(&manager->max31865) ? 1u : 0u;
            break;
        }
    }
    manager->ina226_ok = config->use_ina226 ? (ina226_init(&manager->ina226) ? 1u : 0u) : 1u;
    manager->sht31_ok = config->use_sht31 ? (sht31_init(&manager->sht31) ? 1u : 0u) : 1u;
    manager->lcd_ok = config->use_lcd ? (lcd_pcf8574_init(&manager->lcd) ? 1u : 0u) : 0u;

    if (config->use_acs712) {
        manager->acs712_calibrated = acs712_calibrate(&manager->acs712, 32) ? 1u : 0u;
    }
    return fb_init(&manager->builder, config->temp_count, 0, now_ms);
}

bool sm_self_test(const sm_t *manager)
{
    if (manager == 0) {
        return false;
    }
    if (manager->config.use_ina226 && !manager->ina226_ok) {
        return false;
    }
    if (manager->config.use_sht31 && !manager->sht31_ok) {
        return false;
    }
    for (uint8_t i = 0; i < manager->config.temp_count; ++i) {
        if (manager->config.temp[i].source == TEMP_SOURCE_MAX31865 &&
            !manager->max31865_ok) {
            return false;
        }
    }
    return true;
}

bool sm_calibrate_current(sm_t *manager)
{
    if (manager == 0 || !manager->config.use_acs712) {
        return false;
    }
    manager->acs712_calibrated =
        acs712_calibrate(&manager->acs712, APP_CURRENT_CAL_SAMPLES) ? 1u : 0u;
    return manager->acs712_calibrated != 0u;
}

lcd_pcf8574_t *sm_lcd(sm_t *manager)
{
    if (manager == 0 || !manager->lcd_ok) {
        return 0;
    }
    return &manager->lcd;
}

static sensor_value_t read_temp_channel(const sm_t *manager, const sm_temp_channel_t *channel)
{
    switch (channel->source) {
    case TEMP_SOURCE_LM35_MUX:
        return lm35_read(&board_mux, channel->channel, &manager->lm35);
    case TEMP_SOURCE_NTC_ADC:
        if (channel->channel >= (uint8_t)(sizeof(NTC_ADC_CHANNELS) / sizeof(NTC_ADC_CHANNELS[0]))) {
            return sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
        }
        return ntc_read(&manager->ntc, NTC_ADC_CHANNELS[channel->channel]);
    case TEMP_SOURCE_MAX31865:
        if (!manager->max31865_ok) {
            return sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
        }
        board_select_max31865(channel->channel);
        return max31865_read(&manager->max31865);
    case TEMP_SOURCE_TMP117: {
        tmp117_t sensor = {&hi2c1, channel->i2c_address};
        return tmp117_read(&sensor);
    }
    case TEMP_SOURCE_MLX90614: {
        mlx90614_t sensor = {&hi2c1, channel->i2c_address};
        return mlx90614_read_object(&sensor);
    }
    case TEMP_SOURCE_NONE:
    default:
        return sensor_value_make(0.0f, SENSOR_STATUS_ABSENT);
    }
}

void sm_read(sm_t *manager, bat_raw_sample_t *raw, bat_features_t *features, uint32_t now_ms)
{
    if (manager == 0 || raw == 0 || features == 0) {
        return;
    }
    memset(raw, 0, sizeof(*raw));
    raw->timestamp_ms = now_ms;
    raw->channel_count = manager->config.temp_count;
    raw->voltage_v = NAN;
    raw->current_a = NAN;
    raw->rain_analog = 0.0f;
    raw->rain_digital_dry = 1u;
    raw->humidity_pct = NAN;
    raw->humidity_valid = 0;
    raw->self_test_ok = sm_self_test(manager) ? 1u : 0u;
    for (uint8_t i = 0; i < BAT_MAX_TEMP_SENSORS; ++i) {
        raw->temp_c[i] = NAN;
        raw->temp_status[i] = SENSOR_STATUS_ABSENT;
    }

    for (uint8_t i = 0; i < manager->config.temp_count; ++i) {
        sensor_value_t value = read_temp_channel(manager, &manager->config.temp[i]);
        raw->temp_c[i] = value.value;
        raw->temp_status[i] = (uint8_t)value.status;
        if (temp_status_is_usable(value.status) && bat_is_finite(value.value)) {
            raw->temp_valid_mask |= (uint16_t)(1u << i);
        }
    }

    if (manager->config.use_ina226 && manager->ina226_ok) {
        ina226_reading_t reading = ina226_read(&manager->ina226);
        if (reading.valid) {
            raw->voltage_v = reading.bus_voltage.value;
            raw->current_a = reading.current.value;
        }
    } else if (manager->config.use_voltage_divider) {
        sensor_value_t voltage = voltage_sensor_read(&manager->voltage);
        if (voltage.status == SENSOR_STATUS_OK) {
            raw->voltage_v = voltage.value;
        }
    }

    if (manager->config.use_acs712) {
        sensor_value_t current = acs712_read(&manager->acs712);
        if (current.status == SENSOR_STATUS_OK) {
            raw->current_a = current.value;
        }
    }

    if (manager->config.use_rain) {
        rain_reading_t rain = rain_sensor_read();
        raw->rain_analog = (float)rain.raw_10bit;
        raw->rain_digital_dry = rain.digital_dry;
    }

    if (manager->config.use_sht31 && manager->sht31_ok) {
        sht31_reading_t environment = sht31_read(&manager->sht31);
        if (environment.valid && environment.humidity.status == SENSOR_STATUS_OK) {
            raw->humidity_pct = environment.humidity.value;
            raw->humidity_valid = 1u;
        }
    }

    fb_update(&manager->builder, raw, features, now_ms);
}

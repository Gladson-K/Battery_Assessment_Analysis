#include "app_config.h"
#include "battery_assessment.h"
#include "battery_profiles.h"
#include "board.h"
#include "command_handler.h"
#include "config_store.h"
#include "rf_infer.h"
#include "safety_controller.h"
#include "sensor_manager.h"
#include "serial_io.h"
#include "telemetry.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static sm_t sensor_manager;
static ba_t assessment;
static sc_t safety;
static ba_config_t config;
static bat_profile_t active_profile;
static bat_raw_sample_t raw;
static bat_features_t features;
static rf_result_t ml_result;
static bat_assessment_t result;
static sc_outputs_t outputs;

static uint32_t last_heartbeat_ms;
static uint8_t heartbeat_seen;
static uint32_t last_sample_ms;
static uint32_t last_lcd_ms;
static uint32_t last_button_ms;
static uint8_t button_previous;
static uint8_t boot_fault_pending = 1u;
static uint32_t boot_faults;
static uint16_t telemetry_sequence;

static char telemetry_payload[BAT_TELEMETRY_MAX_PAYLOAD];
static char telemetry_frame_buffer[BAT_TELEMETRY_MAX_PAYLOAD + 16];
static char command_line[APP_COMMAND_MAX_LEN];
static char command_response[BAT_TELEMETRY_MAX_PAYLOAD];
static char command_frame[BAT_TELEMETRY_MAX_PAYLOAD + 16];

static bat_profile_t profile_from_build(void)
{
#if APP_PROFILE_ID == 1
    return BAT_PROFILE_BENCH_6V;
#elif APP_PROFILE_ID == 2
    return BAT_PROFILE_LFP_48V;
#elif APP_PROFILE_ID == 3
    return BAT_PROFILE_LFP_24V;
#elif APP_PROFILE_ID == 4
    return BAT_PROFILE_LEAD_12V;
#else
    return BAT_PROFILE_COLD_CHAIN;
#endif
}

static const char *state_short(bat_state_t state)
{
    switch (state) {
    case BAT_STATE_NORMAL:
        return "NORM";
    case BAT_STATE_WARNING:
        return "WARN";
    case BAT_STATE_CRITICAL:
        return "CRIT";
    case BAT_STATE_TRIPPED:
        return "TRIP";
    default:
        return "INIT";
    }
}

static void format_fixed(char *buffer, size_t capacity, float value, uint8_t decimals)
{
    if (!bat_is_finite(value)) {
        (void)snprintf(buffer, capacity, "--");
        return;
    }
    uint32_t scale = decimals == 0u ? 1u : (decimals == 1u ? 10u : 100u);
    bool negative = value < 0.0f;
    float magnitude = fabsf(value);
    uint32_t scaled = (uint32_t)(magnitude * (float)scale + 0.5f);
    uint32_t whole = scaled / scale;
    uint32_t fraction = scaled % scale;
    if (decimals == 0u) {
        (void)snprintf(buffer, capacity, "%s%lu", negative ? "-" : "", (unsigned long)whole);
    } else if (decimals == 1u) {
        (void)snprintf(buffer, capacity, "%s%lu.%01lu", negative ? "-" : "",
                       (unsigned long)whole, (unsigned long)fraction);
    } else {
        (void)snprintf(buffer, capacity, "%s%lu.%02lu", negative ? "-" : "",
                       (unsigned long)whole, (unsigned long)fraction);
    }
}

static void request_reset(void *context)
{
    (void)context;
    ba_manual_reset(&assessment);
}

static uint8_t calibrate_current(void *context)
{
    (void)context;
    return sm_calibrate_current(&sensor_manager) ? 1u : 0u;
}

static uint8_t sensor_self_test(void *context)
{
    (void)context;
    return sm_self_test(&sensor_manager) ? 1u : 0u;
}

static void format_status(void *context, char *buffer, size_t capacity)
{
    (void)context;
    if (telemetry_build_json(&raw, &features, &result, telemetry_sequence, buffer, capacity) == 0u) {
        (void)snprintf(buffer, capacity, "{\"ok\":false,\"error\":\"no_data\"}");
    }
}

static void update_led(uint8_t mode, uint32_t now_ms)
{
    uint8_t on = 0;
    switch (mode) {
    case SC_LED_HEARTBEAT:
        on = (now_ms % 2000u) < 80u ? 1u : 0u;
        break;
    case SC_LED_WARNING:
        on = (now_ms % 2400u) < 200u ? 1u : 0u;
        break;
    case SC_LED_CRITICAL:
        on = (now_ms % 200u) < 100u ? 1u : 0u;
        break;
    case SC_LED_TRIPPED:
        on = 1u;
        break;
    default:
        on = 0u;
        break;
    }
    board_led_set(on);
}

static void update_lcd(uint32_t now_ms)
{
    lcd_pcf8574_t *lcd = sm_lcd(&sensor_manager);
    if (lcd == 0 || (uint32_t)(now_ms - last_lcd_ms) < APP_LCD_REFRESH_MS) {
        return;
    }
    last_lcd_ms = now_ms;

    char voltage[10];
    char current[10];
    char temperature[10];
    char risk[10];
    char line[20];
    format_fixed(voltage, sizeof(voltage), features.voltage_v, 1);
    format_fixed(current, sizeof(current), features.current_a, 1);
    format_fixed(temperature, sizeof(temperature), features.t_max, 1);
    format_fixed(risk, sizeof(risk), result.risk_score, 0);

    (void)snprintf(line, sizeof(line), "%-4.4s%5.5sV%5.5sA", state_short(result.state), voltage,
                   current);
    lcd_pcf8574_print_line(lcd, 0, line);
    (void)snprintf(line, sizeof(line), "T%5.5sC R%3.3s%%%s", temperature, risk,
                   result.active_faults != 0u ? "FLT" : "OK");
    lcd_pcf8574_print_line(lcd, 1, line);
}

static void build_ml_features(const bat_raw_sample_t *sample, const bat_features_t *feature_data,
                              float vector[BAT_ML_FEATURE_COUNT])
{
    float fallback = bat_is_finite(feature_data->t_avg) ? feature_data->t_avg : 25.0f;
    for (uint8_t i = 0; i < 8u; ++i) {
        float value = sample->temp_c[i];
        if ((sample->temp_valid_mask & (uint16_t)(1u << i)) != 0u && bat_is_finite(value)) {
            vector[i] = value;
        } else {
            vector[i] = fallback;
        }
    }
    vector[BAT_FEATURE_INDEX_VOLTAGE] = feature_data->voltage_v;
    vector[BAT_FEATURE_INDEX_CURRENT] = feature_data->current_a;
    vector[BAT_FEATURE_INDEX_RAIN_ANALOG] = sample->rain_analog;
    vector[BAT_FEATURE_INDEX_RAIN_DIGITAL] = (float)sample->rain_digital_dry;
}

static void sample_and_update(uint32_t now_ms)
{
    sm_read(&sensor_manager, &raw, &features, now_ms);
    raw.estop_active = board_estop_active();
    raw.comms_ok =
        (!heartbeat_seen ||
         (uint32_t)(now_ms - last_heartbeat_ms) <= APP_HEARTBEAT_TIMEOUT_MS)
            ? 1u
            : 0u;
    raw.external_faults = boot_fault_pending ? boot_faults : 0u;
    boot_fault_pending = 0u;

    memset(&ml_result, 0, sizeof(ml_result));
    ml_result.valid = 0u;
    if (APP_ENABLE_ML && config.ml_enabled != 0u && rf_model_self_test() &&
        bat_is_finite(features.voltage_v) && bat_is_finite(features.current_a)) {
        float vector[BAT_ML_FEATURE_COUNT];
        build_ml_features(&raw, &features, vector);
        rf_infer(vector, &ml_result);
    }

    ba_update(&assessment, &raw, &features, &ml_result, now_ms, &result);

    sc_inputs_t inputs;
    inputs.estop_active = raw.estop_active;
    inputs.self_test_ok = raw.self_test_ok;
    inputs.watchdog_ok = 1u;
    inputs.heartbeat_ok = raw.comms_ok;
    sc_update(&safety, &result, &inputs, now_ms, &outputs);

    board_relay_set(outputs.relay_closed);
    board_buzzer_set(outputs.buzzer_on);
    board_fan_set(sensor_manager.config.use_fan ? outputs.fan_on : 0u);
    update_led(outputs.led_mode, now_ms);
    update_lcd(now_ms);

    telemetry_sequence++;
    size_t payload_length =
        telemetry_build_json(&raw, &features, &result, telemetry_sequence, telemetry_payload,
                             sizeof(telemetry_payload));
    if (payload_length > 0u) {
        size_t frame_length = telemetry_frame(telemetry_payload, telemetry_frame_buffer,
                                              sizeof(telemetry_frame_buffer));
        if (frame_length > 0u) {
            serial_gateway_write(telemetry_frame_buffer);
            serial_debug_write(telemetry_frame_buffer);
        }
    }
}

static void process_command(void)
{
    size_t length = serial_read_line(command_line, sizeof(command_line));
    if (length == 0u) {
        return;
    }
    cmd_context_t context;
    context.request_reset = request_reset;
    context.calibrate_current = calibrate_current;
    context.self_test = sensor_self_test;
    context.format_status = format_status;
    context.context = 0;
    context.last_heartbeat_ms = &last_heartbeat_ms;
    context.heartbeat_seen = &heartbeat_seen;

    size_t response_length =
        cmd_process(&context, command_line, command_response, sizeof(command_response));
    if (response_length == 0u) {
        return;
    }
    size_t frame_length =
        telemetry_frame(command_response, command_frame, sizeof(command_frame));
    if (frame_length > 0u) {
        serial_gateway_write(command_frame);
        serial_debug_write(command_frame);
    }
}

static void process_reset_button(uint32_t now_ms)
{
    uint8_t pressed = board_reset_button_pressed();
    if (pressed != 0u && button_previous == 0u &&
        (uint32_t)(now_ms - last_button_ms) >= 250u) {
        ba_manual_reset(&assessment);
        last_button_ms = now_ms;
    }
    button_previous = pressed;
}

int main(void)
{
    if (!board_init()) {
        board_error_handler();
    }

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) {
        boot_faults |= BAT_FAULT_WATCHDOG_RESET;
    }
    __HAL_RCC_CLEAR_RESET_FLAGS();

    active_profile = profile_from_build();
    sm_config_t sm_config;
    bool have_stored_config = false;
#if APP_ENABLE_CONFIG_STORE
    have_stored_config = config_store_load(&config, &active_profile);
#endif
    if (!sm_config_for_profile(&sm_config, active_profile)) {
        active_profile = BAT_PROFILE_BENCH_6V;
        (void)sm_config_for_profile(&sm_config, active_profile);
    }
    if (!have_stored_config) {
        (void)ba_config_from_profile(&config, active_profile, sm_config.temp_count);
    } else {
        config.num_temp_sensors = sm_config.temp_count;
    }

    uint32_t now = board_millis();
    (void)sm_init(&sensor_manager, &sm_config, now);
    (void)ba_init(&assessment, &config, now);
    sc_init(&safety, now);

    last_sample_ms = now - APP_SAMPLE_PERIOD_MS;
    last_lcd_ms = now;
    last_button_ms = now;
    button_previous = board_reset_button_pressed();

    while (1) {
        now = board_millis();
        board_watchdog_refresh();
        process_command();
        process_reset_button(now);

        if ((uint32_t)(now - last_sample_ms) >= APP_SAMPLE_PERIOD_MS) {
            last_sample_ms = now;
            sample_and_update(now);
        }
        HAL_Delay(10);
    }
}

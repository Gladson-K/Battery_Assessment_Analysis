#ifndef BATTERY_SAFETY_CONTROLLER_H
#define BATTERY_SAFETY_CONTROLLER_H

#include "battery_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SC_LED_OFF = 0,
    SC_LED_HEARTBEAT = 1,
    SC_LED_WARNING = 2,
    SC_LED_CRITICAL = 3,
    SC_LED_TRIPPED = 4
} sc_led_mode_t;

typedef struct {
    uint8_t estop_active;
    uint8_t self_test_ok;
    uint8_t watchdog_ok;
    uint8_t heartbeat_ok;
} sc_inputs_t;

typedef struct {
    uint8_t relay_closed;
    uint8_t buzzer_on;
    uint8_t fan_on;
    uint8_t led_mode;
    uint8_t safe_state;
    uint8_t trip_latched;
} sc_outputs_t;

typedef struct {
    uint8_t initialized;
    uint8_t relay_closed;
    uint32_t relay_changed_ms;
    uint32_t close_dwell_ms;
} sc_t;

void sc_init(sc_t *controller, uint32_t now_ms);
void sc_update(sc_t *controller, const bat_assessment_t *assessment, const sc_inputs_t *inputs,
               uint32_t now_ms, sc_outputs_t *outputs);

#ifdef __cplusplus
}
#endif

#endif

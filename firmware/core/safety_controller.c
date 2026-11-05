#include "safety_controller.h"

#include <string.h>

void sc_init(sc_t *controller, uint32_t now_ms)
{
    if (controller == 0) {
        return;
    }
    memset(controller, 0, sizeof(*controller));
    controller->relay_closed = 0;
    controller->relay_changed_ms = now_ms;
    controller->close_dwell_ms = 1000u;
    controller->initialized = 1;
}

static uint8_t blink_pattern(bat_state_t state, uint32_t now_ms)
{
    uint32_t phase = now_ms % 2400u;
    switch (state) {
    case BAT_STATE_WARNING:
        return phase < 200u ? 1u : 0u;
    case BAT_STATE_CRITICAL:
        return (now_ms % 200u) < 100u ? 1u : 0u;
    case BAT_STATE_TRIPPED:
        return 1u;
    case BAT_STATE_NORMAL:
        return (now_ms % 2000u) < 80u ? 1u : 0u;
    default:
        return 0u;
    }
}

void sc_update(sc_t *controller, const bat_assessment_t *assessment, const sc_inputs_t *inputs,
               uint32_t now_ms, sc_outputs_t *outputs)
{
    if (outputs == 0) {
        return;
    }
    memset(outputs, 0, sizeof(*outputs));
    if (controller == 0 || assessment == 0 || inputs == 0 || !controller->initialized) {
        outputs->relay_closed = 0;
        outputs->buzzer_on = 1;
        outputs->safe_state = 1;
        outputs->led_mode = SC_LED_TRIPPED;
        return;
    }

    uint8_t allow_close = assessment->relay_allowed != 0u;
    allow_close = allow_close && inputs->self_test_ok != 0u;
    allow_close = allow_close && inputs->watchdog_ok != 0u;
    allow_close = allow_close && inputs->estop_active == 0u;
    allow_close = allow_close && inputs->heartbeat_ok != 0u;
    allow_close = allow_close && (assessment->state == BAT_STATE_NORMAL ||
                                  assessment->state == BAT_STATE_WARNING);

    if (!allow_close) {
        if (controller->relay_closed != 0u) {
            controller->relay_closed = 0;
            controller->relay_changed_ms = now_ms;
        }
    } else {
        if (controller->relay_closed == 0u &&
            (uint32_t)(now_ms - controller->relay_changed_ms) >= controller->close_dwell_ms) {
            controller->relay_closed = 1;
            controller->relay_changed_ms = now_ms;
        }
    }

    outputs->relay_closed = controller->relay_closed;
    outputs->trip_latched = assessment->trip_latched;
    outputs->safe_state = controller->relay_closed == 0u ? 1u : 0u;

    switch (assessment->state) {
    case BAT_STATE_NORMAL:
        outputs->led_mode = SC_LED_HEARTBEAT;
        outputs->buzzer_on = 0;
        outputs->fan_on = 0;
        break;
    case BAT_STATE_WARNING:
        outputs->led_mode = SC_LED_WARNING;
        outputs->buzzer_on = blink_pattern(assessment->state, now_ms);
        outputs->fan_on = assessment->risk_score >= 55.0f ? 1u : 0u;
        break;
    case BAT_STATE_CRITICAL:
        outputs->led_mode = SC_LED_CRITICAL;
        outputs->buzzer_on = blink_pattern(assessment->state, now_ms);
        outputs->fan_on = 1u;
        break;
    case BAT_STATE_TRIPPED:
        outputs->led_mode = SC_LED_TRIPPED;
        outputs->buzzer_on = 1u;
        outputs->fan_on = 1u;
        break;
    default:
        outputs->led_mode = SC_LED_OFF;
        outputs->buzzer_on = 0;
        outputs->fan_on = 0;
        break;
    }

    if (outputs->safe_state != 0u && assessment->state == BAT_STATE_TRIPPED) {
        outputs->buzzer_on = 1u;
    }
}

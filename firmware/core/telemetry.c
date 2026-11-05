#include "telemetry.h"

#include "battery_assessment.h"
#include "filters.h"

#include <math.h>
#include <string.h>

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
    uint8_t ok;
} sb_t;

static void sb_init(sb_t *sb, char *buffer, size_t capacity)
{
    sb->buffer = buffer;
    sb->capacity = capacity;
    sb->length = 0;
    sb->ok = (buffer != 0 && capacity > 0u) ? 1u : 0u;
    if (sb->ok != 0u) {
        buffer[0] = '\0';
    }
}

static void sb_char(sb_t *sb, char c)
{
    if (sb->ok == 0u) {
        return;
    }
    if (sb->length + 1u >= sb->capacity) {
        sb->ok = 0;
        sb->buffer[0] = '\0';
        return;
    }
    sb->buffer[sb->length++] = c;
    sb->buffer[sb->length] = '\0';
}

static void sb_str(sb_t *sb, const char *text)
{
    if (text == 0) {
        return;
    }
    while (*text != '\0') {
        sb_char(sb, *text++);
    }
}

static void sb_u32(sb_t *sb, uint32_t value)
{
    char digits[10];
    uint8_t count = 0;
    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && count < sizeof(digits));
    while (count > 0u) {
        sb_char(sb, digits[--count]);
    }
}

static void sb_i32(sb_t *sb, int32_t value)
{
    if (value < 0) {
        sb_char(sb, '-');
        uint32_t magnitude = (uint32_t)(-(value + 1)) + 1u;
        sb_u32(sb, magnitude);
    } else {
        sb_u32(sb, (uint32_t)value);
    }
}

static int32_t pow10_i32(uint8_t decimals)
{
    int32_t value = 1;
    for (uint8_t i = 0; i < decimals && i < 4u; i++) {
        value *= 10;
    }
    return value;
}

static void sb_float(sb_t *sb, float value, uint8_t decimals, const char *null_token)
{
    if (!bat_is_finite(value)) {
        sb_str(sb, null_token);
        return;
    }
    value = bat_clampf(value, -1000000.0f, 1000000.0f);
    int32_t scale = pow10_i32(decimals);
    int32_t scaled = (int32_t)lroundf(value * (float)scale);
    bool negative = scaled < 0;
    if (negative) {
        scaled = -scaled;
    }
    if (negative) {
        sb_char(sb, '-');
    }
    sb_i32(sb, scaled / scale);
    if (decimals > 0u) {
        sb_char(sb, '.');
        int32_t fraction = scaled % scale;
        int32_t divisor = scale / 10;
        while (divisor > 0) {
            sb_char(sb, (char)('0' + (fraction / divisor) % 10));
            divisor /= 10;
        }
    }
}

static void sb_hex32(sb_t *sb, uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    sb_str(sb, "0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        sb_char(sb, hex[(value >> shift) & 0x0Fu]);
    }
}

uint16_t telemetry_crc16_ccitt(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFu;
    if (data == 0) {
        return crc;
    }
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8u; bit++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

size_t telemetry_frame(const char *payload, char *buffer, size_t capacity)
{
    if (payload == 0 || buffer == 0 || capacity < 8u) {
        return 0;
    }
    size_t payload_length = strlen(payload);
    if (payload_length + 8u > capacity) {
        buffer[0] = '\0';
        return 0;
    }
    uint16_t crc = telemetry_crc16_ccitt((const uint8_t *)payload, payload_length);
    static const char hex[] = "0123456789ABCDEF";
    size_t index = 0;
    buffer[index++] = '$';
    memcpy(&buffer[index], payload, payload_length);
    index += payload_length;
    buffer[index++] = '*';
    buffer[index++] = hex[(crc >> 12) & 0x0Fu];
    buffer[index++] = hex[(crc >> 8) & 0x0Fu];
    buffer[index++] = hex[(crc >> 4) & 0x0Fu];
    buffer[index++] = hex[crc & 0x0Fu];
    buffer[index++] = '\n';
    buffer[index] = '\0';
    return index;
}

size_t telemetry_build_json(const bat_raw_sample_t *raw, const bat_features_t *features,
                            const bat_assessment_t *assessment, uint16_t sequence, char *buffer,
                            size_t capacity)
{
    if (raw == 0 || features == 0 || assessment == 0) {
        return 0;
    }
    sb_t sb;
    sb_init(&sb, buffer, capacity);
    sb_str(&sb, "{\"seq\":");
    sb_u32(&sb, sequence);
    sb_str(&sb, ",\"ms\":");
    sb_u32(&sb, raw->timestamp_ms);
    sb_str(&sb, ",\"state\":\"");
    sb_str(&sb, ba_state_name(assessment->state));
    sb_str(&sb, "\",\"risk\":");
    sb_float(&sb, assessment->risk_score, 2, "null");
    sb_str(&sb, ",\"rule\":");
    sb_float(&sb, assessment->rule_score, 2, "null");
    sb_str(&sb, ",\"ml\":{\"p\":");
    sb_float(&sb, assessment->ml_p_unsafe, 4, "null");
    sb_str(&sb, ",\"label\":");
    sb_u32(&sb, assessment->ml_label);
    sb_str(&sb, ",\"valid\":");
    sb_u32(&sb, assessment->ml_valid);
    sb_str(&sb, ",\"lowConf\":");
    sb_u32(&sb, assessment->ml_low_confidence);
    sb_str(&sb, "},\"v\":");
    sb_float(&sb, features->voltage_v, 3, "null");
    sb_str(&sb, ",\"i\":");
    sb_float(&sb, features->current_a, 3, "null");
    sb_str(&sb, ",\"p\":");
    sb_float(&sb, features->power_w, 3, "null");
    sb_str(&sb, ",\"rain\":");
    sb_float(&sb, features->rain_pct, 1, "null");
    sb_str(&sb, ",\"rainDry\":");
    sb_u32(&sb, raw->rain_digital_dry);
    sb_str(&sb, ",\"hum\":");
    sb_float(&sb, features->humidity_pct, 1, "null");
    sb_str(&sb, ",\"t\":[");
    for (uint8_t i = 0; i < features->num_temp; i++) {
        if (i != 0u) {
            sb_char(&sb, ',');
        }
        sb_float(&sb, features->temp_c[i], 2, "null");
    }
    sb_str(&sb, "],\"tmin\":");
    sb_float(&sb, features->t_min, 2, "null");
    sb_str(&sb, ",\"tmax\":");
    sb_float(&sb, features->t_max, 2, "null");
    sb_str(&sb, ",\"spread\":");
    sb_float(&sb, features->t_spread, 2, "null");
    sb_str(&sb, ",\"dt\":");
    sb_float(&sb, features->dt_dt_c_per_s, 3, "null");
    sb_str(&sb, ",\"faults\":\"");
    sb_hex32(&sb, assessment->active_faults);
    sb_str(&sb, "\",\"latched\":\"");
    sb_hex32(&sb, assessment->latched_faults);
    sb_str(&sb, "\",\"reason\":\"");
    sb_str(&sb, assessment->reason != 0 ? assessment->reason : "none");
    sb_str(&sb, "\",\"relay\":");
    sb_u32(&sb, assessment->relay_allowed);
    sb_str(&sb, ",\"buzzer\":");
    sb_u32(&sb, assessment->buzzer_on);
    sb_str(&sb, ",\"ver\":\"");
    sb_str(&sb, BAT_FIRMWARE_VERSION);
    sb_str(&sb, "\"}");
    return sb.ok != 0u ? sb.length : 0u;
}

size_t telemetry_build_csv(const bat_raw_sample_t *raw, const bat_features_t *features,
                           const bat_assessment_t *assessment, uint16_t sequence, char *buffer,
                           size_t capacity)
{
    if (raw == 0 || features == 0 || assessment == 0) {
        return 0;
    }
    sb_t sb;
    sb_init(&sb, buffer, capacity);
    sb_u32(&sb, sequence);
    sb_char(&sb, ',');
    sb_u32(&sb, raw->timestamp_ms);
    sb_char(&sb, ',');
    sb_str(&sb, ba_state_name(assessment->state));
    sb_char(&sb, ',');
    sb_float(&sb, assessment->risk_score, 2, "");
    sb_char(&sb, ',');
    sb_float(&sb, assessment->rule_score, 2, "");
    sb_char(&sb, ',');
    sb_float(&sb, assessment->ml_p_unsafe, 4, "");
    sb_char(&sb, ',');
    sb_u32(&sb, assessment->ml_label);
    sb_char(&sb, ',');
    sb_u32(&sb, assessment->ml_valid);
    sb_char(&sb, ',');
    sb_u32(&sb, assessment->relay_allowed);
    sb_char(&sb, ',');
    sb_u32(&sb, assessment->buzzer_on);
    sb_char(&sb, ',');
    sb_hex32(&sb, assessment->active_faults);
    sb_char(&sb, ',');
    sb_hex32(&sb, assessment->latched_faults);
    for (uint8_t i = 0; i < features->num_temp; i++) {
        sb_char(&sb, ',');
        sb_float(&sb, features->temp_c[i], 2, "");
    }
    sb_char(&sb, ',');
    sb_float(&sb, features->voltage_v, 3, "");
    sb_char(&sb, ',');
    sb_float(&sb, features->current_a, 3, "");
    sb_char(&sb, ',');
    sb_float(&sb, features->power_w, 3, "");
    sb_char(&sb, ',');
    sb_float(&sb, features->rain_pct, 1, "");
    sb_char(&sb, ',');
    sb_u32(&sb, raw->rain_digital_dry);
    sb_char(&sb, ',');
    sb_float(&sb, features->humidity_pct, 1, "");
    sb_char(&sb, ',');
    sb_str(&sb, assessment->reason != 0 ? assessment->reason : "none");
    return sb.ok != 0u ? sb.length : 0u;
}

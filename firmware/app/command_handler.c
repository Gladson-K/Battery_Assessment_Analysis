#include "command_handler.h"

#include "app_config.h"
#include "board.h"
#include "telemetry.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int hex_value(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static bool parse_frame(const char *line, char *payload, size_t capacity)
{
    if (line[0] != '$') {
        size_t length = strlen(line);
        if (length >= capacity) {
            return false;
        }
        memcpy(payload, line, length + 1u);
        return true;
    }
    const char *star = strrchr(line, '*');
    if (star == 0 || strlen(star + 1) < 4u) {
        return false;
    }
    size_t payload_length = (size_t)(star - line - 1);
    if (payload_length >= capacity) {
        return false;
    }
    memcpy(payload, line + 1, payload_length);
    payload[payload_length] = '\0';

    uint16_t expected = 0;
    for (uint8_t i = 0; i < 4u; ++i) {
        int value = hex_value(star[1 + i]);
        if (value < 0) {
            return false;
        }
        expected = (uint16_t)((expected << 4) | (uint16_t)value);
    }
    uint16_t actual = telemetry_crc16_ccitt((const uint8_t *)payload, payload_length);
    return expected == actual;
}

static void uppercase_token(char *token)
{
    while (*token != '\0' && !isspace((unsigned char)*token)) {
        *token = (char)toupper((unsigned char)*token);
        ++token;
    }
}

size_t cmd_process(cmd_context_t *context, const char *line, char *response, size_t capacity)
{
    if (context == 0 || line == 0 || response == 0 || capacity == 0) {
        return 0;
    }
    response[0] = '\0';

    char payload[APP_COMMAND_MAX_LEN];
    if (!parse_frame(line, payload, sizeof(payload))) {
        return (size_t)snprintf(response, capacity, "{\"ok\":false,\"error\":\"bad_frame\"}");
    }

    char command[APP_COMMAND_MAX_LEN];
    strncpy(command, payload, sizeof(command) - 1u);
    command[sizeof(command) - 1u] = '\0';
    char *token = command;
    while (*token == ' ') {
        ++token;
    }
    uppercase_token(token);
    if (*token == '\0') {
        return 0;
    }

    if (strcmp(token, "PING") == 0) {
        return (size_t)snprintf(response, capacity,
                                "{\"ok\":true,\"cmd\":\"PING\",\"fw\":\"%s\",\"version\":\"%s\"}",
                                APP_FIRMWARE_NAME, BAT_FIRMWARE_VERSION);
    }
    if (strcmp(token, "HEARTBEAT") == 0) {
        if (context->last_heartbeat_ms != 0) {
            *context->last_heartbeat_ms = board_millis();
        }
        if (context->heartbeat_seen != 0) {
            *context->heartbeat_seen = 1;
        }
        return (size_t)snprintf(response, capacity, "{\"ok\":true,\"cmd\":\"HEARTBEAT\"}");
    }
    if (strcmp(token, "STATUS") == 0) {
        if (context->format_status != 0) {
            context->format_status(context->context, response, capacity);
        } else {
            (void)snprintf(response, capacity, "{\"ok\":true,\"cmd\":\"STATUS\"}");
        }
        return strlen(response);
    }
    if (strcmp(token, "RESET") == 0) {
        if (context->request_reset != 0) {
            context->request_reset(context->context);
        }
        return (size_t)snprintf(response, capacity, "{\"ok\":true,\"cmd\":\"RESET\"}");
    }
    if (strcmp(token, "CAL") == 0) {
        bool ok = context->calibrate_current != 0 && context->calibrate_current(context->context);
        return (size_t)snprintf(response, capacity, "{\"ok\":%s,\"cmd\":\"CAL\"}",
                                ok ? "true" : "false");
    }
    if (strcmp(token, "SELFTEST") == 0) {
        bool ok = context->self_test != 0 && context->self_test(context->context);
        return (size_t)snprintf(response, capacity, "{\"ok\":%s,\"cmd\":\"SELFTEST\"}",
                                ok ? "true" : "false");
    }
    if (strcmp(token, "HELP") == 0) {
        return (size_t)snprintf(
            response, capacity,
            "{\"ok\":true,\"commands\":[\"PING\",\"HEARTBEAT\",\"STATUS\",\"RESET\",\"CAL\","
            "\"SELFTEST\"]}");
    }
    return (size_t)snprintf(response, capacity, "{\"ok\":false,\"error\":\"unknown_command\"}");
}

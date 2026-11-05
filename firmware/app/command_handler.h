#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*request_reset)(void *context);
    uint8_t (*calibrate_current)(void *context);
    uint8_t (*self_test)(void *context);
    void (*format_status)(void *context, char *buffer, size_t capacity);
    void *context;
    uint32_t *last_heartbeat_ms;
    uint8_t *heartbeat_seen;
} cmd_context_t;

size_t cmd_process(cmd_context_t *context, const char *line, char *response, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif

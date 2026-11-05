#ifndef SERIAL_IO_H
#define SERIAL_IO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void serial_debug_write(const char *text);
void serial_gateway_write(const char *text);
size_t serial_read_line(char *buffer, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif

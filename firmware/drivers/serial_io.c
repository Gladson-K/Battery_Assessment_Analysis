#include "serial_io.h"

#include "board.h"

#include <string.h>

#define SERIAL_LINE_CAPACITY 160u

static char line_buffer[SERIAL_LINE_CAPACITY];
static size_t line_length;

void serial_debug_write(const char *text)
{
    if (text == 0) {
        return;
    }
    HAL_UART_Transmit(&huart1, (const uint8_t *)text, (uint16_t)strlen(text), 100);
}

void serial_gateway_write(const char *text)
{
    if (text == 0) {
        return;
    }
    HAL_UART_Transmit(&huart2, (const uint8_t *)text, (uint16_t)strlen(text), 100);
}

size_t serial_read_line(char *buffer, size_t capacity)
{
    uint8_t byte = 0;
    while (HAL_UART_Receive(&huart2, &byte, 1, 0) == HAL_OK) {
        if (byte == '\r' || byte == '\n') {
            if (line_length == 0u) {
                continue;
            }
            if (buffer != 0 && capacity > 0u) {
                size_t copy = line_length < capacity - 1u ? line_length : capacity - 1u;
                memcpy(buffer, line_buffer, copy);
                buffer[copy] = '\0';
            }
            size_t result = line_length;
            line_length = 0;
            return result;
        }
        if (line_length < SERIAL_LINE_CAPACITY - 1u) {
            line_buffer[line_length++] = (char)byte;
        } else {
            line_length = 0;
        }
    }
    return 0;
}

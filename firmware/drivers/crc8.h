#ifndef CRC8_H
#define CRC8_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t crc8_maxim(const uint8_t *data, size_t length);
uint8_t crc8_smbus(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif

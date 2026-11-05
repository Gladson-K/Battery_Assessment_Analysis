#ifndef BATTERY_TELEMETRY_H
#define BATTERY_TELEMETRY_H

#include "battery_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BAT_TELEMETRY_MAX_PAYLOAD 640
#define BAT_FIRMWARE_VERSION "1.0.0"

size_t telemetry_build_json(const bat_raw_sample_t *raw, const bat_features_t *features,
                            const bat_assessment_t *assessment, uint16_t sequence, char *buffer,
                            size_t capacity);
size_t telemetry_build_csv(const bat_raw_sample_t *raw, const bat_features_t *features,
                           const bat_assessment_t *assessment, uint16_t sequence, char *buffer,
                           size_t capacity);
uint16_t telemetry_crc16_ccitt(const uint8_t *data, size_t length);
size_t telemetry_frame(const char *payload, char *buffer, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif

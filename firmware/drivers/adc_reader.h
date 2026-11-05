#ifndef ADC_READER_H
#define ADC_READER_H

#include "board.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool adc_read_channel(ADC_HandleTypeDef *adc, uint32_t channel, uint16_t *raw);

#ifdef __cplusplus
}
#endif

#endif

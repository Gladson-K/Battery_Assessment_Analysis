#ifndef MUX74HC4067_H
#define MUX74HC4067_H

#include "board.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    GPIO_TypeDef *s0_port;
    uint16_t s0_pin;
    GPIO_TypeDef *s1_port;
    uint16_t s1_pin;
    GPIO_TypeDef *s2_port;
    uint16_t s2_pin;
    GPIO_TypeDef *s3_port;
    uint16_t s3_pin;
} mux74hc4067_t;

extern const mux74hc4067_t board_mux;

void mux74hc4067_select(const mux74hc4067_t *mux, uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif

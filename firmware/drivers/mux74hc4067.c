#include "mux74hc4067.h"

const mux74hc4067_t board_mux = {
    BOARD_MUX_S0_PORT, BOARD_MUX_S0_PIN,
    BOARD_MUX_S1_PORT, BOARD_MUX_S1_PIN,
    BOARD_MUX_S2_PORT, BOARD_MUX_S2_PIN,
    BOARD_MUX_S3_PORT, BOARD_MUX_S3_PIN,
};

void mux74hc4067_select(const mux74hc4067_t *mux, uint8_t channel)
{
    if (mux == 0) {
        return;
    }
    HAL_GPIO_WritePin(mux->s0_port, mux->s0_pin,
                      (channel & 0x01u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(mux->s1_port, mux->s1_pin,
                      (channel & 0x02u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(mux->s2_port, mux->s2_pin,
                      (channel & 0x04u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(mux->s3_port, mux->s3_pin,
                      (channel & 0x08u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    board_delay_ms(1);
}

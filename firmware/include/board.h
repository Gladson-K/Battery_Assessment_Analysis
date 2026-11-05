#ifndef BOARD_H
#define BOARD_H

#include "board_config.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern IWDG_HandleTypeDef hiwdg;

bool board_init(void);
void board_watchdog_refresh(void);
uint32_t board_millis(void);
void board_delay_ms(uint32_t milliseconds);
void board_error_handler(void);

void board_led_set(uint8_t on);
void board_relay_set(uint8_t closed);
void board_buzzer_set(uint8_t on);
void board_fan_set(uint8_t on);
uint8_t board_estop_active(void);
uint8_t board_reset_button_pressed(void);
uint8_t board_rain_digital_dry(void);
void board_cs_set(uint8_t asserted);
void board_select_max31865(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif

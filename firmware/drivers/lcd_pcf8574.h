#ifndef LCD_PCF8574_H
#define LCD_PCF8574_H

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    I2C_HandleTypeDef *i2c;
    uint8_t address;
    uint8_t backlight;
} lcd_pcf8574_t;

bool lcd_pcf8574_init(lcd_pcf8574_t *lcd);
void lcd_pcf8574_clear(lcd_pcf8574_t *lcd);
void lcd_pcf8574_set_cursor(lcd_pcf8574_t *lcd, uint8_t row, uint8_t column);
void lcd_pcf8574_print(lcd_pcf8574_t *lcd, const char *text);
void lcd_pcf8574_print_line(lcd_pcf8574_t *lcd, uint8_t row, const char *text);

#ifdef __cplusplus
}
#endif

#endif

#include "lcd_pcf8574.h"

#include "board.h"

#define LCD_BACKLIGHT_BIT 0x08u
#define LCD_ENABLE_BIT 0x04u
#define LCD_RS_BIT 0x01u

static bool lcd_write_expander(const lcd_pcf8574_t *lcd, uint8_t value)
{
    uint8_t data = (uint8_t)(value | (lcd->backlight ? LCD_BACKLIGHT_BIT : 0u));
    return HAL_I2C_Master_Transmit(lcd->i2c, (uint16_t)(lcd->address << 1), &data, 1, 10) ==
           HAL_OK;
}

static bool lcd_write_nibble(const lcd_pcf8574_t *lcd, uint8_t nibble, uint8_t rs)
{
    uint8_t value = (uint8_t)((nibble << 4) | (rs ? LCD_RS_BIT : 0u));
    if (!lcd_write_expander(lcd, (uint8_t)(value | LCD_ENABLE_BIT))) {
        return false;
    }
    board_delay_ms(1);
    return lcd_write_expander(lcd, (uint8_t)(value & (uint8_t)~LCD_ENABLE_BIT));
}

static bool lcd_write_byte(const lcd_pcf8574_t *lcd, uint8_t value, uint8_t rs)
{
    if (!lcd_write_nibble(lcd, (uint8_t)(value >> 4), rs)) {
        return false;
    }
    return lcd_write_nibble(lcd, (uint8_t)(value & 0x0Fu), rs);
}

static bool lcd_command(const lcd_pcf8574_t *lcd, uint8_t command)
{
    return lcd_write_byte(lcd, command, 0);
}

bool lcd_pcf8574_init(lcd_pcf8574_t *lcd)
{
    if (lcd == 0 || lcd->i2c == 0) {
        return false;
    }
    board_delay_ms(50);
    if (!lcd_write_nibble(lcd, 0x03u, 0)) {
        return false;
    }
    board_delay_ms(5);
    if (!lcd_write_nibble(lcd, 0x03u, 0)) {
        return false;
    }
    board_delay_ms(1);
    if (!lcd_write_nibble(lcd, 0x03u, 0)) {
        return false;
    }
    if (!lcd_write_nibble(lcd, 0x02u, 0)) {
        return false;
    }
    if (!lcd_command(lcd, 0x28u)) {
        return false;
    }
    if (!lcd_command(lcd, 0x0Cu)) {
        return false;
    }
    if (!lcd_command(lcd, 0x06u)) {
        return false;
    }
    lcd_pcf8574_clear(lcd);
    return true;
}

void lcd_pcf8574_clear(lcd_pcf8574_t *lcd)
{
    (void)lcd_command(lcd, 0x01u);
    board_delay_ms(2);
}

void lcd_pcf8574_set_cursor(lcd_pcf8574_t *lcd, uint8_t row, uint8_t column)
{
    uint8_t address = (uint8_t)((row != 0u ? 0x40u : 0x00u) + column);
    (void)lcd_command(lcd, (uint8_t)(0x80u | address));
}

void lcd_pcf8574_print(lcd_pcf8574_t *lcd, const char *text)
{
    if (lcd == 0 || text == 0) {
        return;
    }
    while (*text != '\0') {
        (void)lcd_write_byte(lcd, (uint8_t)*text, 1);
        ++text;
    }
}

void lcd_pcf8574_print_line(lcd_pcf8574_t *lcd, uint8_t row, const char *text)
{
    lcd_pcf8574_set_cursor(lcd, row, 0);
    lcd_pcf8574_print(lcd, "                ");
    lcd_pcf8574_set_cursor(lcd, row, 0);
    lcd_pcf8574_print(lcd, text);
}

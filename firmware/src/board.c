#include "board.h"

#include "stm32f4xx_hal_pwr_ex.h"

ADC_HandleTypeDef hadc1;
I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi2;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
IWDG_HandleTypeDef hiwdg;

static void clock_init(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clocks = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscillator.HSEState = RCC_HSE_ON;
    oscillator.PLL.PLLState = RCC_PLL_ON;
    oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    oscillator.PLL.PLLM = 25;
    oscillator.PLL.PLLN = 200;
    oscillator.PLL.PLLP = RCC_PLLP_DIV2;
    oscillator.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        board_error_handler();
    }

    clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                       RCC_CLOCKTYPE_PCLK2;
    clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clocks.APB1CLKDivider = RCC_HCLK_DIV2;
    clocks.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_3) != HAL_OK) {
        board_error_handler();
    }
}

static void gpio_clock_enable(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
}

static void output_init(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState initial)
{
    GPIO_InitTypeDef gpio = {0};
    HAL_GPIO_WritePin(port, pin, initial);
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &gpio);
}

static void input_pullup_init(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(port, &gpio);
}

static void analog_init(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(port, &gpio);
}

static void alt_init(GPIO_TypeDef *port, uint16_t pin, uint32_t alternate, uint32_t mode,
                     uint32_t pull)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = mode;
    gpio.Pull = pull;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = alternate;
    HAL_GPIO_Init(port, &gpio);
}

static void gpio_init(void)
{
    output_init(BOARD_LED_PORT, BOARD_LED_PIN, GPIO_PIN_SET);
    output_init(BOARD_RELAY_PORT, BOARD_RELAY_PIN, GPIO_PIN_RESET);
    output_init(BOARD_BUZZER_PORT, BOARD_BUZZER_PIN, GPIO_PIN_RESET);
    output_init(BOARD_FAN_PORT, BOARD_FAN_PIN, GPIO_PIN_RESET);
    output_init(BOARD_SPI_CS_PORT, BOARD_SPI_CS_PIN, GPIO_PIN_SET);

    input_pullup_init(BOARD_ESTOP_PORT, BOARD_ESTOP_PIN);
    input_pullup_init(BOARD_RESET_BUTTON_PORT, BOARD_RESET_BUTTON_PIN);
    input_pullup_init(BOARD_RAIN_DIGITAL_PORT, BOARD_RAIN_DIGITAL_PIN);

    alt_init(BOARD_I2C_SCL_PORT, BOARD_I2C_SCL_PIN, GPIO_AF4_I2C1, GPIO_MODE_AF_OD,
             GPIO_PULLUP);
    alt_init(BOARD_I2C_SDA_PORT, BOARD_I2C_SDA_PIN, GPIO_AF4_I2C1, GPIO_MODE_AF_OD,
             GPIO_PULLUP);
    alt_init(BOARD_SPI_SCK_PORT, BOARD_SPI_SCK_PIN, GPIO_AF5_SPI2, GPIO_MODE_AF_PP,
             GPIO_NOPULL);
    alt_init(BOARD_SPI_MISO_PORT, BOARD_SPI_MISO_PIN, GPIO_AF5_SPI2, GPIO_MODE_AF_PP,
             GPIO_PULLUP);
    alt_init(BOARD_SPI_MOSI_PORT, BOARD_SPI_MOSI_PIN, GPIO_AF5_SPI2, GPIO_MODE_AF_PP,
             GPIO_NOPULL);
    alt_init(BOARD_UART_DEBUG_TX_PORT, BOARD_UART_DEBUG_TX_PIN, GPIO_AF7_USART1,
             GPIO_MODE_AF_PP, GPIO_NOPULL);
    alt_init(BOARD_UART_DEBUG_RX_PORT, BOARD_UART_DEBUG_RX_PIN, GPIO_AF7_USART1,
             GPIO_MODE_AF_PP, GPIO_PULLUP);
    alt_init(BOARD_UART_GATEWAY_TX_PORT, BOARD_UART_GATEWAY_TX_PIN, GPIO_AF7_USART2,
             GPIO_MODE_AF_PP, GPIO_NOPULL);
    alt_init(BOARD_UART_GATEWAY_RX_PORT, BOARD_UART_GATEWAY_RX_PIN, GPIO_AF7_USART2,
             GPIO_MODE_AF_PP, GPIO_PULLUP);

    analog_init(GPIOA, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5);
#if APP_PROFILE_ID == 2
    output_init(BOARD_MUX_S0_PORT, BOARD_MUX_S0_PIN, GPIO_PIN_RESET);
    output_init(BOARD_MUX_S1_PORT, BOARD_MUX_S1_PIN, GPIO_PIN_RESET);
    output_init(BOARD_MUX_S2_PORT, BOARD_MUX_S2_PIN, GPIO_PIN_RESET);
    output_init(BOARD_MUX_S3_PORT, BOARD_MUX_S3_PIN, GPIO_PIN_RESET);
#elif APP_PROFILE_ID == 4
    analog_init(GPIOA, GPIO_PIN_6 | GPIO_PIN_7);
    analog_init(GPIOB, GPIO_PIN_0 | GPIO_PIN_1);
#elif APP_PROFILE_ID == 1
    output_init(BOARD_MUX_S0_PORT, BOARD_MUX_S0_PIN, GPIO_PIN_RESET);
    output_init(BOARD_MUX_S1_PORT, BOARD_MUX_S1_PIN, GPIO_PIN_RESET);
    output_init(BOARD_MUX_S2_PORT, BOARD_MUX_S2_PIN, GPIO_PIN_RESET);
    output_init(BOARD_MUX_S3_PORT, BOARD_MUX_S3_PIN, GPIO_PIN_RESET);
#endif
}

static void adc_init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        board_error_handler();
    }
}

static void i2c_init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        board_error_handler();
    }
}

static void spi_init(void)
{
    __HAL_RCC_SPI2_CLK_ENABLE();
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi2) != HAL_OK) {
        board_error_handler();
    }
}

static void uart_init(UART_HandleTypeDef *uart, USART_TypeDef *instance)
{
    uart->Instance = instance;
    uart->Init.BaudRate = 115200;
    uart->Init.WordLength = UART_WORDLENGTH_8B;
    uart->Init.StopBits = UART_STOPBITS_1;
    uart->Init.Parity = UART_PARITY_NONE;
    uart->Init.Mode = UART_MODE_TX_RX;
    uart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart->Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(uart) != HAL_OK) {
        board_error_handler();
    }
}

static void watchdog_init(void)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg.Init.Reload = 1000;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        board_error_handler();
    }
}

bool board_init(void)
{
    if (HAL_Init() != HAL_OK) {
        return false;
    }
    clock_init();
    gpio_clock_enable();
    gpio_init();
    adc_init();
    i2c_init();
    spi_init();
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    uart_init(&huart1, USART1);
    uart_init(&huart2, USART2);
    watchdog_init();
    board_relay_set(0);
    board_buzzer_set(0);
    board_fan_set(0);
    board_led_set(0);
    return true;
}

void board_watchdog_refresh(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}

uint32_t board_millis(void)
{
    return HAL_GetTick();
}

void board_delay_ms(uint32_t milliseconds)
{
    HAL_Delay(milliseconds);
}

void board_led_set(uint8_t on)
{
#if BOARD_LED_ACTIVE_LOW
    HAL_GPIO_WritePin(BOARD_LED_PORT, BOARD_LED_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(BOARD_LED_PORT, BOARD_LED_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
}

void board_relay_set(uint8_t closed)
{
#if BOARD_RELAY_ACTIVE_HIGH
    HAL_GPIO_WritePin(BOARD_RELAY_PORT, BOARD_RELAY_PIN,
                      closed ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(BOARD_RELAY_PORT, BOARD_RELAY_PIN,
                      closed ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
}

void board_buzzer_set(uint8_t on)
{
    HAL_GPIO_WritePin(BOARD_BUZZER_PORT, BOARD_BUZZER_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_fan_set(uint8_t on)
{
    HAL_GPIO_WritePin(BOARD_FAN_PORT, BOARD_FAN_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint8_t board_estop_active(void)
{
#if BOARD_ESTOP_ACTIVE_LOW
    return HAL_GPIO_ReadPin(BOARD_ESTOP_PORT, BOARD_ESTOP_PIN) == GPIO_PIN_RESET ? 1u : 0u;
#else
    return HAL_GPIO_ReadPin(BOARD_ESTOP_PORT, BOARD_ESTOP_PIN) == GPIO_PIN_SET ? 1u : 0u;
#endif
}

uint8_t board_reset_button_pressed(void)
{
#if BOARD_RESET_BUTTON_ACTIVE_LOW
    return HAL_GPIO_ReadPin(BOARD_RESET_BUTTON_PORT, BOARD_RESET_BUTTON_PIN) == GPIO_PIN_RESET
               ? 1u
               : 0u;
#else
    return HAL_GPIO_ReadPin(BOARD_RESET_BUTTON_PORT, BOARD_RESET_BUTTON_PIN) == GPIO_PIN_SET
               ? 1u
               : 0u;
#endif
}

uint8_t board_rain_digital_dry(void)
{
    return HAL_GPIO_ReadPin(BOARD_RAIN_DIGITAL_PORT, BOARD_RAIN_DIGITAL_PIN) == GPIO_PIN_SET ? 1u
                                                                                            : 0u;
}

void board_cs_set(uint8_t asserted)
{
    HAL_GPIO_WritePin(BOARD_SPI_CS_PORT, BOARD_SPI_CS_PIN,
                      asserted ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void board_select_max31865(uint8_t channel)
{
#if BOARD_MAX31865_DECODER
    HAL_GPIO_WritePin(BOARD_MUX_S0_PORT, BOARD_MUX_S0_PIN,
                      (channel & 0x01u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BOARD_MUX_S1_PORT, BOARD_MUX_S1_PIN,
                      (channel & 0x02u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BOARD_MUX_S2_PORT, BOARD_MUX_S2_PIN,
                      (channel & 0x04u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    (void)channel;
#endif
}

void board_error_handler(void)
{
    __disable_irq();
    board_relay_set(0);
    board_buzzer_set(1);
    board_led_set(1);
    while (1) {
    }
}

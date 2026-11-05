#include "adc_reader.h"

bool adc_read_channel(ADC_HandleTypeDef *adc, uint32_t channel, uint16_t *raw)
{
    if (adc == 0 || raw == 0) {
        return false;
    }
    ADC_ChannelConfTypeDef config = {0};
    config.Channel = channel;
    config.Rank = 1;
    config.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    config.Offset = 0;
    if (HAL_ADC_ConfigChannel(adc, &config) != HAL_OK) {
        return false;
    }
    if (HAL_ADC_Start(adc) != HAL_OK) {
        return false;
    }
    if (HAL_ADC_PollForConversion(adc, 10) != HAL_OK) {
        HAL_ADC_Stop(adc);
        return false;
    }
    *raw = (uint16_t)HAL_ADC_GetValue(adc);
    HAL_ADC_Stop(adc);
    return true;
}

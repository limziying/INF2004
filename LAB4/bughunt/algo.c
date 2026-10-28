#include "algo.h"

uint16_t adc_to_mv(uint16_t raw)
{
    /* Bug 1: retain the full product before converting to millivolts. */
    uint32_t scaled = (uint32_t)raw * VREF_MV;
    return scaled / ADC_MAX;
}

uint16_t duty_from_adc(uint16_t raw, uint16_t wrap)
{
    /* Bug 2: multiply before dividing so fractional ADC levels are preserved. */
    return ((uint32_t)raw * wrap) / ADC_MAX;
}

/* Bug 4: keep independent filter state for each input channel. */
static int32_t y[NUM_CHANNELS] = {0};

void iir_reset(void)
{
    for (unsigned ch = 0; ch < NUM_CHANNELS; ch++)
        y[ch] = 0;
}

uint32_t iir_step(unsigned ch, uint32_t x)
{
    /* Bug 3: signed subtraction handles downward steps without unsigned wrap. */
    y[ch] += ((int32_t)x - y[ch]) / (1 << ALPHA_SHIFT);
    return (uint32_t)y[ch];
}

uint16_t pwm_wrap_for_hz(uint32_t hz, float clk_div)
{
    /* Bug 5: the PWM counter includes both zero and wrap. */
    return (uint16_t)(ALGO_SYS_CLK_HZ / clk_div / hz - 1);
}

/*
 * BUG HUNT #4 - the signal chain on real hardware.
 * INF2004 LAB 4
 *
 * Generates a 20 Hz / 50% PWM square wave on GP0, samples it back through the
 * ADC on GP26 every 25 ms, filters both ADC channels, and nudges the PWM duty
 * toward a target voltage.
 *
 * Wiring:  GP0 -> GP26   (jumper wire, exactly as in the lab exercise)
 *          GP27 left floating or on the IR line sensor
 *
 * Three of the eight planted defects live in THIS file. Five live in algo.c and
 * can be found on your laptop in one second. Do those first.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "algo.h"

#define PWM_PIN     0
#define ADC_PIN_A  26
#define ADC_PIN_B  27
#define SAMPLE_MS  25
#define PWM_HZ     20
#define CLK_DIV   250.0f
#define TARGET_MV 1650u

static uint pwm_slice;
static uint16_t wrap;
static uint16_t duty;

static void sample_once(void)
{
    /* Bug 6: select the ADC input before converting that channel. */
    adc_select_input(0);
    uint16_t raw_a = adc_read();

    adc_select_input(1);
    uint16_t raw_b = adc_read();

    uint32_t filt_a = iir_step(0, raw_a);
    uint32_t filt_b = iir_step(1, raw_b);

    uint16_t mv = adc_to_mv((uint16_t)filt_a);

    /* Bug 8: signed difference supports both control directions. */
    int32_t delta = (int32_t)TARGET_MV - (int32_t)mv;

    if (delta < 0) {
        if (duty > 0) duty--;
    } else if (delta > 0) {
        if (duty < wrap) duty++;
    }
    pwm_set_gpio_level(PWM_PIN, duty);

    /* Bug 7: print voltage using integer units, avoiding a float/%d mismatch. */
    printf("A raw=%4u filt=%4u  %4u mV (%u.%03u V)   B raw=%4u filt=%4u   duty=%u/%u\n",
           (unsigned)raw_a, (unsigned)filt_a, (unsigned)mv,
           (unsigned)(mv / 1000u), (unsigned)(mv % 1000u),
           (unsigned)raw_b, (unsigned)filt_b, (unsigned)duty, (unsigned)wrap);
}

int main(void)
{
    stdio_init_all();
    sleep_ms(3000);

    /* ---- PWM out on GP0 ---- */
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    pwm_slice = pwm_gpio_to_slice_num(PWM_PIN);

    wrap = pwm_wrap_for_hz(PWM_HZ, CLK_DIV);
    duty = wrap / 2;

    pwm_set_clkdiv(pwm_slice, CLK_DIV);
    pwm_set_wrap(pwm_slice, wrap);
    pwm_set_gpio_level(PWM_PIN, duty);
    pwm_set_enabled(pwm_slice, true);

    /* ---- ADC in on GP26 / GP27 ---- */
    adc_init();
    adc_gpio_init(ADC_PIN_A);
    adc_gpio_init(ADC_PIN_B);

    printf("BUG HUNT #4 - PWM %d Hz, wrap=%u, sampling every %d ms\n",
           PWM_HZ, (unsigned)wrap, SAMPLE_MS);
    printf("measure GP0 with a scope or a second Pico and check the frequency\n\n");

    while (true) {
        sample_once();
        sleep_ms(SAMPLE_MS);
    }
}

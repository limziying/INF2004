/*
 * BUG HUNT #3 - Works in Debug, hangs in Release
 * INF2004 LAB 3
 *
 * An IR wheel-encoder driver. It should:
 *   - count one pulse per slot of the encoder wheel
 *   - measure the width of each slot in microseconds
 *   - debounce the optical noise on each edge
 *   - stop and report once TARGET_SLOTS slots have gone past
 *
 * Wiring: HC020K OUT -> GP2,  VCC -> 3V3,  GND -> GND
 *
 * Eight defects are planted. This file COMPILES (with warnings - read them).
 * Building it is not the exercise; making it behave is.
 *
 * You must build this at BOTH -O0 (Debug) and -O3 (Release) before you
 * change anything.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

#define ENCODER_PIN   2
#define TARGET_SLOTS  20
/* Bug 4: keep debounce below legitimate pulse intervals. */
#define DEBOUNCE_US   1000u
#define SLOTS_PER_REV 20

/* ---- state shared between the ISR and main ---- */
/* Bug 1: volatile makes ISR updates visible to the polling main loop. */
static volatile uint32_t pulse_count = 0;
static volatile uint64_t last_edge_us = 0;
static volatile uint32_t slot_width_us = 0;
static volatile uint32_t slot_period_us = 0;

static uint32_t last_debounce_us = 0;
static uint32_t slot_start_us = 0;
static uint32_t previous_start_us = 0;
static bool have_previous_start = false;

typedef enum {
    ENC_IDLE,
    ENC_SLOT,
} enc_state_t;

static enc_state_t state = ENC_IDLE;

/* ------------------------------------------------------------------
 * Interrupt service routine - runs on every configured edge on GP2.
 * ------------------------------------------------------------------ */
void encoder_isr(uint gpio, uint32_t events)
{
    if (gpio != ENCODER_PIN || pulse_count >= TARGET_SLOTS)
        return;

    uint32_t now = time_us_32();

    /* Bug 3: elapsed-time subtraction remains valid across timer rollover. */
    if ((uint32_t)(now - last_debounce_us) <= DEBOUNCE_US)
        return;

    if ((events & GPIO_IRQ_EDGE_FALL) && (events & GPIO_IRQ_EDGE_RISE)) {
        state = ENC_IDLE;
        have_previous_start = false;
        return;
    }

    last_debounce_us = now;
    last_edge_us = time_us_64();

    if (events & GPIO_IRQ_EDGE_FALL) {
        if (have_previous_start)
            slot_period_us = now - previous_start_us;
        previous_start_us = now;
        have_previous_start = true;
        slot_start_us = now;
        state = ENC_SLOT;
    } else if (events & GPIO_IRQ_EDGE_RISE) {
        if (state == ENC_SLOT) {
            /* Bug 5: only a later rising edge completes the slot. */
            slot_width_us = now - slot_start_us;
            pulse_count++;
            state = ENC_IDLE;
        }
    }
}

/* ------------------------------------------------------------------
 * Speed uses the interval between slot starts, including the gap.
 * A slot's pulse width alone does not determine revolutions per minute.
 * ------------------------------------------------------------------ */
static uint32_t rpm_from_period(uint32_t period_us)
{
    if (period_us == 0) return 0;
    return (uint32_t)(60000000ull / ((uint64_t)period_us * SLOTS_PER_REV));
}

int main(void)
{
    stdio_init_all();
    sleep_ms(3000);

    gpio_init(ENCODER_PIN);
    gpio_set_dir(ENCODER_PIN, GPIO_IN);
    gpio_pull_up(ENCODER_PIN);

    /* Bug 7: width needs both the start and end edges of a pulse. */
    gpio_set_irq_enabled_with_callback(ENCODER_PIN,
                                       GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
                                       true,
                                       &encoder_isr);

    printf("BUG HUNT #3 - turn the wheel, waiting for %d slots\n", TARGET_SLOTS);

    while (pulse_count < TARGET_SLOTS)
        tight_loop_contents();

    /* Bug 2: snapshot the multiword ISR state atomically on Cortex-M0+. */
    uint32_t irq_state = save_and_disable_interrupts();
    uint32_t count = pulse_count;
    uint32_t width = slot_width_us;
    uint32_t period = slot_period_us;
    uint64_t finished_at = last_edge_us;
    restore_interrupts(irq_state);

    printf("\ndone.\n");
    /* Bug 6: keep potentially blocking USB output outside the ISR. */
    printf("  slots counted : %lu\n", (unsigned long)count);
    printf("  last width    : %lu us\n", (unsigned long)width);
    printf("  speed         : %lu rpm\n", (unsigned long)rpm_from_period(period));
    /* Bug 8: use a format specifier and argument type matching uint64_t. */
    printf("  last edge at  : %llu us since boot\n",
           (unsigned long long)finished_at);

    while (true)
        tight_loop_contents();
}

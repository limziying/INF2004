/*
 * BUG HUNT #1 - Bits that lie about themselves
 * INF2004 LAB 1
 *
 * A small library of bit-manipulation helpers, of the kind that appears in
 * every GPIO driver ever written. Six defects have been planted.
 *
 * Build on your laptop:   gcc -Wall -Wextra -o bughunt1 bughunt1.c && ./bughunt1
 * Build for the Pico:     see CMakeLists.txt in this folder
 *
 * DO NOT change main() or the expected values. Fix the functions.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h> // Bughunt #1

#ifdef PICO_ON_DEVICE
#include "pico/stdlib.h"
#endif

/* A GPIO mask with GP2 and GP5 set: 0b0010_0100 */
#define LED_MASK 0x00000024u

/* ------------------------------------------------------------------
 * 1. Count how many bits are set in a value.
 *    count_bits(0xFF) must be 8.  count_bits(-1) must be 32.
 * ------------------------------------------------------------------ */
uint8_t count_bits(int value)
{
    uint8_t count = 0; // Bughunt #2
    uint32_t bits = (uint32_t)value;

    while (bits) {
        count += bits & 1u;
        bits >>= 1; // Bughunt #5
    }
    return count;
}

/* ------------------------------------------------------------------
 * 2. True when the value has an even number of bits set.
 *    Used for parity checking on a serial link.
 * ------------------------------------------------------------------ */
bool even_parity(uint32_t value)
{
    uint8_t bits = count_bits(value);

    if (bits % 2 == 0) {
        return true;
    } // Bughunt #3
    else {
        return false;
    }
}

/* ------------------------------------------------------------------
 * 3. True when the given pin's bit is CLEAR in the mask.
 *    pin_is_clear(0x24, 2) is false - bit 2 is set.
 *    pin_is_clear(0x24, 3) is true  - bit 3 is clear.
 * ------------------------------------------------------------------ */
bool pin_is_clear(uint32_t mask, unsigned pin)
{
    return ((mask & (1u << pin)) == 0); // Bughunt #4
}

/* ------------------------------------------------------------------
 * 4. Reverse the bit order of a 32-bit word.
 *    Bit 0 becomes bit 31, bit 1 becomes bit 30, and so on.
 * ------------------------------------------------------------------ */
uint32_t reverse_bits(uint32_t v)
{
    uint32_t r = 0;

    for (int i = 0; i < 32; i++) // Bughunt #6
        r |= ((v >> i) & 1) << (31 - i);

    return r;
}

/* ------------------------------------------------------------------
 * 5. Swap the two nibbles inside every byte of a word.
 *    0x12345678 becomes 0x21436587.
 *    (This one may or may not be broken. Check it.)
 * ------------------------------------------------------------------ */
uint32_t swap_nibbles(uint32_t v)
{
    return ((v & 0x0F0F0F0Fu) << 4) | ((v & 0xF0F0F0F0u) >> 4);
}

/* ------------------------------------------------------------------
 * Test harness - do not modify.
 * ------------------------------------------------------------------ */
static int failures = 0;

static void check_u32(const char *what, uint32_t got, uint32_t expect)
{
    bool ok = (got == expect);
    if (!ok) failures++;
    printf("  %-34s got 0x%08lX  expect 0x%08lX  %s\n",
           what, (unsigned long)got, (unsigned long)expect, ok ? "ok" : "FAIL");
}

static void check_int(const char *what, int got, int expect)
{
    bool ok = (got == expect);
    if (!ok) failures++;
    printf("  %-34s got %-10d expect %-10d %s\n",
           what, got, expect, ok ? "ok" : "FAIL");
}

int main(void)
{
#ifdef PICO_ON_DEVICE
    stdio_init_all();
    sleep_ms(3000);
#endif

    printf("BUG HUNT #1 - bit manipulation\n\n");

    printf("count_bits\n");
    check_int("count_bits(0x000000FF)", count_bits(0x000000FF), 8);
    check_int("count_bits(0x00000024)", count_bits(0x00000024), 2);
    check_int("count_bits(-1)",         count_bits(-1),         32);

    printf("even_parity\n");
    check_int("even_parity(0x000000FF)", even_parity(0x000000FFu), 1);
    check_int("even_parity(0x00000007)", even_parity(0x00000007u), 0);

    printf("pin_is_clear (mask 0x24)\n");
    check_int("pin_is_clear(LED_MASK, 2)", pin_is_clear(LED_MASK, 2), 0);
    check_int("pin_is_clear(LED_MASK, 3)", pin_is_clear(LED_MASK, 3), 1);
    check_int("pin_is_clear(LED_MASK, 5)", pin_is_clear(LED_MASK, 5), 0);

    printf("reverse_bits\n");
    check_u32("reverse_bits(0x00000001)", reverse_bits(0x00000001u), 0x80000000u);
    check_u32("reverse_bits(0x12345678)", reverse_bits(0x12345678u), 0x1E6A2C48u);
    check_u32("reverse_bits(0xFFFFFFFF)", reverse_bits(0xFFFFFFFFu), 0xFFFFFFFFu);

    printf("swap_nibbles\n");
    check_u32("swap_nibbles(0x12345678)", swap_nibbles(0x12345678u), 0x21436587u);

    printf("\n%s  (%d failure%s)\n",
           failures ? "HUNT INCOMPLETE" : "ALL TESTS PASS",
           failures, failures == 1 ? "" : "s");

#ifdef PICO_ON_DEVICE
    while (true) tight_loop_contents();
#endif
    return failures ? 1 : 0;
}

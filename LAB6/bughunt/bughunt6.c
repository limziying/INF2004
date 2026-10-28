/*
 * BUG HUNT #6 - Only the disassembly tells the truth
 * INF2004 LAB 6
 *
 * A telemetry packet layer: CRC-8 integrity, an LFSR for test-pattern
 * generation, and a parser for incoming frames.
 *
 * SPECIFICATION
 * -------------
 *   crc8(data, len)
 *       CRC-8/ATM, polynomial 0x07, init 0x00, no reflection, no final XOR.
 *       Reference vector: crc8("123456789", 9) == 0xF4
 *
 *   lfsr_next()
 *       16-bit right-shifting Galois LFSR, feedback mask 0xB400.
 *       Seeded with 0xACE1. Its period must be 65535 - it must visit every
 *       non-zero 16-bit value exactly once before repeating, and must never
 *       reach zero.
 *
 *   parse_frame(frame, len, out)
 *       Frame layout:  [0xA5][len][payload...][crc8 over payload]
 *       Payload:       [0..3] timestamp, big-endian
 *                      [4..5] value, big-endian
 *       len must be exactly 6; reject truncated or differently sized frames.
 *
 *   format_reading(v)
 *       Returns a printable string for a reading.
 *
 * TWELVE defects are planted. Then, when you are done, there is pid.c.
 *
 * There are no hints in this file and none in the brief.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef PICO_ON_DEVICE
#include "pico/stdlib.h"
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <time.h>
#endif

#define FRAME_SOF   0xA5u
#define POLY        0x07u
#define LFSR_SEED   0xACE1u
/* Bug 4: this right-shifting Galois mask gives the required maximal period. */
#define LFSR_TAPS   0xB400u

/* ------------------------------------------------------------------ */
/* Bug 1: the CRC table needs an entry for every possible byte value. */
static uint8_t  crc_table[256];
static uint16_t lfsr_state = LFSR_SEED;
static bool     crc_ready  = false;

void crc_init(void)
{
    for (unsigned i = 0; i < sizeof crc_table; i++) {
        uint8_t c = (uint8_t)i;
        for (int b = 0; b < 8; b++)
            c = (c & 0x80u) ? (uint8_t)((c << 1) ^ POLY) : (uint8_t)(c << 1);
        crc_table[i] = c;
    }
    crc_ready = true;
}

uint8_t crc8(const uint8_t *data, uint8_t len)
{
    /* Bug 3: make the API safe when called before explicit initialization. */
    if (!crc_ready)
        crc_init();

    /* Bug 2: CRC-8/ATM starts from zero. */
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++)
        crc = crc_table[crc ^ data[i]];

    return crc;
}

/* ------------------------------------------------------------------ */
uint16_t lfsr_next(void)
{
    uint16_t lsb = lfsr_state & 1u;
    lfsr_state >>= 1;
    if (lsb)
        lfsr_state ^= LFSR_TAPS;
    return lfsr_state;
}

uint32_t lfsr_period(void)
{
    uint16_t start = lfsr_state;
    uint32_t n = 0;

    do {
        lfsr_next();
        n++;
    /* Bug 5: allow the full non-zero 16-bit sequence to complete. */
    } while (lfsr_state != start && n < 65536u);

    return n;
}

/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t timestamp;
    uint16_t value;
} reading_t;

uint32_t parse_ts(const uint8_t *frame)
{
    /* Bug 6: assemble big-endian bytes without unaligned native loads. */
    return ((uint32_t)frame[2] << 24) | ((uint32_t)frame[3] << 16)
         | ((uint32_t)frame[4] << 8) | frame[5];
}

bool parse_frame(const uint8_t *frame, uint8_t frame_len, reading_t *out)
{
    if (frame_len < 3)         return false;
    if (frame[0] != FRAME_SOF) return false;

    uint8_t len = frame[1];

    /* Bug 7: require the exact payload and full frame before indexing. */
    if (len != 6 || frame_len != (uint8_t)(len + 3u))
        return false;

    if (crc8(&frame[2], len) != frame[2 + len])
        return false;

    out->timestamp = parse_ts(frame);
    out->value     = (uint16_t)((frame[6] << 8) | frame[7]);

    return true;
}

/* ------------------------------------------------------------------ */
char *format_reading(const reading_t *r)
{
    /* Bug 8: static storage remains valid after this function returns. */
    static char buf[32];
    snprintf(buf, sizeof buf, "t=%lu v=%u",
             (unsigned long)r->timestamp, (unsigned)r->value);
    return buf;
}

char *label_for(const char *name)
{
    /* Bug 9: reserve space for the terminating NUL and check allocation. */
    char *out = malloc(strlen(name) + 1u);
    if (out == NULL)
        return NULL;
    strcpy(out, name);
    return out;
}

/* ------------------------------------------------------------------
 * A calibration delay the sensor datasheet requires: at least 500 us
 * between powering the part and reading it.
 * ------------------------------------------------------------------ */
void calibration_delay(void)
{
#ifdef PICO_ON_DEVICE
    /* Bug 10: an empty loop is removable and is not a time delay. */
    sleep_us(500);
#elif defined(_WIN32)
    Sleep(1);
#else
    struct timespec delay = { .tv_sec = 0, .tv_nsec = 500000 };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
#endif
}

/* ------------------------------------------------------------------
 * Wait for the acquisition ISR to signal that a conversion finished.
 * ------------------------------------------------------------------ */
/* Bug 11: the foreground wait must reload the ISR-updated flag. */
static volatile bool conversion_done = false;

void acquisition_isr(void)
{
    conversion_done = true;
}

void wait_for_conversion(void)
{
    while (!conversion_done)
        ;
    conversion_done = false;
}

/* ------------------------------------------------------------------
 * Sum every byte of a buffer. Used for a quick integrity check.
 * ------------------------------------------------------------------ */
uint32_t checksum_all(const uint8_t *data, uint16_t len)
{
    uint32_t sum = 0;
    /* Bug 12: use an index wide enough to reach all uint16_t lengths. */
    for (uint16_t i = 0; i < len; i++)
        sum += data[i];
    return sum;
}

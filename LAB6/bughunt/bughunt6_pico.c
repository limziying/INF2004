/*
 * BUG HUNT #6 - on the target, with a debugger attached.
 * INF2004 LAB 6
 *
 * Exercises the telemetry layer on real hardware. Some of the twelve defects
 * do not exist on your laptop at all - your laptop tolerates things a
 * Cortex-M0+ does not.
 *
 * Build this one with the Debug Probe attached and RUN IT UNDER THE DEBUGGER.
 * If it faults, do not power-cycle it. Stop, and read the registers.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

typedef struct { uint32_t timestamp; uint16_t value; } reading_t;

void      crc_init(void);
uint8_t   crc8(const uint8_t *data, uint8_t len);
uint16_t  lfsr_next(void);
uint32_t  lfsr_period(void);
uint32_t  parse_ts(const uint8_t *frame);
bool      parse_frame(const uint8_t *frame, uint8_t frame_len, reading_t *out);
char     *format_reading(const reading_t *r);
void      calibration_delay(void);
void      acquisition_isr(void);
void      wait_for_conversion(void);
uint32_t  checksum_all(const uint8_t *data, uint16_t len);

static int64_t conversion_cb(alarm_id_t id, void *user_data)
{
    (void)id;
    (void)user_data;
    acquisition_isr();
    return 0;
}

int main(void)
{
    stdio_init_all();
    sleep_ms(3000);

    printf("BUG HUNT #6 - target build\n\n");

    crc_init();

    printf("[1] crc8 reference vector\n");
    printf("    crc8(\"123456789\") = 0x%02X   (must be 0xF4)\n",
           crc8((const uint8_t *)"123456789", 9));

    printf("[2] lfsr period\n");
    printf("    measured %lu   (must be 65535)\n", (unsigned long)lfsr_period());

    printf("[3] calibration delay - datasheet requires >= 500 us\n");
    {
        uint32_t t0 = time_us_32();
        for (int i = 0; i < 100; i++) calibration_delay();
        uint32_t t1 = time_us_32();
        printf("    measured %lu us per call\n", (unsigned long)((t1 - t0) / 100));
        printf("    now rebuild at the other optimisation level and compare\n");
    }

    printf("[4] parse_frame on a well-formed frame\n");
    {
        uint8_t payload[6] = { 0x11, 0x22, 0x33, 0x44, 0x07, 0x08 };
        uint8_t frame[9];
        reading_t r = { 0, 0 };

        frame[0] = 0xA5;
        frame[1] = 6;
        memcpy(&frame[2], payload, 6);
        frame[8] = crc8(payload, 6);

        printf("    calling parse_frame...\n");
        stdio_flush();

        if (parse_frame(frame, sizeof frame, &r))
            printf("    timestamp = 0x%08lX (must be 0x11223344), value = 0x%04X\n",
                   (unsigned long)r.timestamp, (unsigned)r.value);
        else
            printf("    rejected\n");
    }

    printf("[5] format_reading\n");
    {
        reading_t r = { 42, 7 };
        char *s = format_reading(&r);
        printf("    \"%s\"   (must be \"t=42 v=7\")\n", s ? s : "(null)");
    }

    printf("[6] wait for an alarm ISR to finish a conversion\n");
    alarm_id_t alarm = add_alarm_in_ms(10, conversion_cb, NULL, true);
    if (alarm < 0) panic("Unable to schedule conversion alarm");
    wait_for_conversion();
    printf("    conversion complete\n");

    printf("\ndone.\n");

    while (true)
        tight_loop_contents();
}

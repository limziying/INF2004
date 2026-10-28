/*
 * BUG HUNT #2 - the link itself.
 * INF2004 LAB 2
 *
 * Flash this SAME program onto BOTH Picos. Wire them as in the lab's paired
 * UART task:
 *
 *      Pico A GP8 (TX1) ----> Pico B GP9 (RX1)
 *      Pico A GP9 (RX1) <---- Pico B GP8 (TX1)
 *      Pico A GND       <---> Pico B GND
 *
 * Press the button on GP20 to transmit one reading. Both boards print every
 * frame they send and every frame they receive, as hex.
 *
 * Two of the seven planted defects live in THIS file and cannot be reproduced
 * on your laptop. Fix frame.c first.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "frame.h"

#define LINK_UART uart1
#define LINK_TX_PIN 8
#define LINK_RX_PIN 9
#define LINK_BAUD 9600
#define BUTTON_PIN 20

static void link_init(void)
{
    uart_init(LINK_UART, LINK_BAUD);
    gpio_set_function(LINK_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(LINK_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(LINK_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(LINK_UART, true);

    /* Bug 6: keep CR/LF translation off so binary bytes stay unchanged. */
    uart_set_translate_crlf(LINK_UART, false);
}

static void send_reading(const reading_t *r)
{
    uint8_t frame[FRAME_MAX];
    uint8_t n = frame_encode(r, frame);

    hexdump("tx", frame, n);

    for (uint8_t i = 0; i < n; i++)
        uart_putc(LINK_UART, frame[i]);
}

static void rx_poll(void)
{
    static uint8_t buf[FRAME_MAX];
    static uint8_t n = 0;

    /* Bug 7: hunt for SOF, validate length, then receive one complete frame. */
    while (uart_is_readable(LINK_UART))
    {
        uint8_t byte = (uint8_t)uart_getc(LINK_UART);

        if (n == 0)
        {
            if (byte != FRAME_SOF)
                continue;
            buf[n++] = byte;
            continue;
        }

        buf[n++] = byte;

        if (n == 2 && buf[1] != FRAME_PAYLOAD)
        {
            n = 0;
            continue;
        }

        if (n == 2 + FRAME_PAYLOAD + 1)
        {
            reading_t r;

            hexdump("rx", buf, n);

            if (frame_decode(buf, n, &r))
                printf("           id=0x%04X status=%u temp=%s%d.%d C t=%lu ms\n",
                       r.sensor_id, r.status,
                       r.temp_c_x10 < 0 ? "-" : "",
                       (r.temp_c_x10 < 0 ? -r.temp_c_x10 : r.temp_c_x10) / 10,
                       (r.temp_c_x10 < 0 ? -r.temp_c_x10 : r.temp_c_x10) % 10,
                       (unsigned long)r.timestamp_ms);
            else
                printf("           BAD FRAME (checksum or header rejected)\n");

            n = 0;
        }
    }
}

int main(void)
{
    stdio_init_all();
    sleep_ms(3000);

    link_init();

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    printf("BUG HUNT #2 - press GP20 to send a reading\n");
    printf("sizeof(reading_t) = %u bytes\n\n", (unsigned)sizeof(reading_t));

    bool was_down = false;
    uint16_t seq = 0;

    while (true)
    {
        bool is_down = !gpio_get(BUTTON_PIN);

        if (is_down && !was_down)
        {
            reading_t r = {
                .sensor_id = 0x1234,
                .status = 0x01,
                .temp_c_x10 = 253,
                .timestamp_ms = 0x0A0B0C0D + seq++,
            };
            send_reading(&r);
        }
        was_down = is_down;

        rx_poll();
        sleep_ms(10);
    }
}

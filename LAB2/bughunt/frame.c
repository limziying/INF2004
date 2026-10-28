#include <stdio.h>
#include <string.h>
#include "frame.h"

uint8_t frame_encode(const reading_t *r, uint8_t *out)
{
    /* Bug 3: use the protocol's 9-byte payload length. */
    uint8_t len = FRAME_PAYLOAD;
    uint8_t sum = 0;
    uint8_t n = 0;

    out[n++] = FRAME_SOF;
    out[n++] = len;

    /* Bug 1: serialize fields explicitly, not the padded struct. */
    /* Bug 2: write multi-byte values in big-endian order. */
    out[n++] = (uint8_t)(r->sensor_id >> 8);
    out[n++] = (uint8_t)r->sensor_id;
    out[n++] = r->status;

    out[n++] = (uint8_t)((uint16_t)r->temp_c_x10 >> 8);
    out[n++] = (uint8_t)r->temp_c_x10;
    out[n++] = (uint8_t)(r->timestamp_ms >> 24);
    out[n++] = (uint8_t)(r->timestamp_ms >> 16);
    out[n++] = (uint8_t)(r->timestamp_ms >> 8);
    out[n++] = (uint8_t)r->timestamp_ms;

    /* Bug 3: checksum exactly the payload bytes. */
    for (uint8_t i = 2; i < n; i++)
        sum += out[i];

    out[n++] = sum;
    return n;
}

bool frame_decode(const uint8_t *in, uint8_t in_len, reading_t *r)
{
    uint8_t payload[FRAME_MAX];
    uint8_t sum = 0;
    uint8_t len;

    if (in_len < 3)
        return false;
    if (in[0] != FRAME_SOF)
        return false;

    len = in[1];

    /* Bug 5: reject wrong and truncated lengths before copying. */
    if (len != FRAME_PAYLOAD)
        return false;
    if (in_len < (uint8_t)(len + 3))
        return false;

    for (uint8_t i = 0; i < len; i++)
        payload[i] = in[2 + i];

    for (uint8_t i = 0; i < len; i++)
        sum += payload[i];

    if (sum != in[2 + len])
        return false;

    /* Bug 4: cast before shifting to keep the operations unsigned. */
    r->sensor_id = ((uint16_t)payload[0] << 8) | payload[1];
    r->status = payload[2];
    r->temp_c_x10 = (int16_t)(((uint16_t)payload[3] << 8) | payload[4]);
    r->timestamp_ms = ((uint32_t)payload[5] << 24) | ((uint32_t)payload[6] << 16) | ((uint32_t)payload[7] << 8) | payload[8];

    return true;
}

void hexdump(const char *label, const uint8_t *buf, uint8_t len)
{
    printf("%-10s [%2u] ", label, len);
    for (uint8_t i = 0; i < len; i++)
        printf("%02X ", buf[i]);
    printf("\n");
}

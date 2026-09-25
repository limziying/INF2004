# Debug Logbook — Bug Hunt #2

**Name(s): Lim Zi Ying** 
**Date: 25/9/2026** 

One row per hypothesis. **Include the hypotheses that turned out to be wrong** —
they are the evidence that you were reasoning rather than guessing.

| # | Symptom observed | Hypothesis | Experiment (ONE change) | Predicted | Result | Conclusion |
|---|---|---|---|---|---|---|
| 1 | | | | | | |
| 2 | | | | | | |
| 3 | | | | | | |
| 4 | | | | | | |
| 5 | | | | | | |
| 6 | | | | | | |
| 7 | | | | | | |
| 8 | | | | | | |

---

## Defects found

The five distinct defects (frame.c) are:

Encoder: 3 defects

1. Copied the raw struct into the frame

- reading_t contains compiler padding bytes.
- This produced the unexpected 00 00 00 bytes.
- Fix: serialize each field explicitly.

2. Used the machine’s byte order

- The machine stores values little-endian, but the protocol requires big-endian.
- For example, 0x1234 became 34 12 instead of 12 34.
- Fix: write the most-significant byte first using shifts.

3. Used the wrong payload length/checksum range

- sizeof(reading_t) was 12 instead of the required 9 payload bytes.
- The loop also copied len - 1 bytes, making the length and actual frame inconsistent.
- Fix:
uint8_t len = FRAME_PAYLOAD;

and checksum exactly the 9 payload bytes.

Decoder: 2 defects

4. Undefined behaviour from integer promotion

- Expressions such as:
    payload[5] << 24

    promote payload[5] to signed int before shifting.
- Shifting 128 left by 24 cannot be represented by int.
- Fix:
(uint32_t)payload[5] << 24

5. No validation of the received length

- The received len controls writes into:
uint8_t payload[FRAME_MAX];

- A corrupt length such as 200 causes a stack buffer overflow.
- It also accepted wrong or truncated frame lengths.
- Fix:
if (len != FRAME_PAYLOAD) return false;
if (in_len < len + 3) return false;

6. bughunt2_pico.c, line 42:
   uart_set_translate_crlf(..., true) treats binary frame data as text and
   converts 0x0A to 0x0D 0x0A, inserting an extra byte. Fix: use false.

7. bughunt2_pico.c, lines 59-69:
   receiver groups every 12 bytes without checking FRAME_SOF or resynchronising
   after an invalid frame. One lost/extra byte permanently shifts the frame
   window. Fix: implement start-byte hunting/state-machine reception.



| # | File & line | Class (syntax / logical / heisenbug / UB) | The defect | The fix | Why the fix is correct |
|---|---|---|---|---|---|
| 1 | | | | | |
| 2 | | | | | |
| 3 | | | | | |
| 4 | | | | | |
| 5 | | | | | |
| 6 | | | | | |
| 7 | | | | | |

---

## Reflection

**Which defect took longest, and what finally cracked it?**
Encoder

Previously it copied the raw reading_t struct:
const uint8_t *p = (const uint8_t *)r;

That caused:

padding bytes to be copied
little-endian byte order
incorrect payload length
I changed it to write exactly the 9 protocol bytes explicitly:
out[n++] = (uint8_t)(r->sensor_id >> 8);
out[n++] = (uint8_t)r->sensor_id;

This sends multi-byte values in big-endian order. The encoder now uses:
uint8_t len = FRAME_PAYLOAD;

and calculates the checksum over exactly those 9 payload bytes.

Decoder

I added validation before using the received length:
if (len != FRAME_PAYLOAD) return false;
if (in_len < (uint8_t)(len + 3)) return false;

This rejects invalid or truncated frames before copying data into the local buffer.

I also added explicit unsigned casts before shifting:
r->timestamp_ms =
    ((uint32_t)payload[5] << 24)


Without the cast, payload[5] is promoted to int, and shifting a value such as 128 left by 24 can cause undefined behaviour.

The sanitizer test was cancelled before running, so validate with:
gcc -Wall -Wextra -fsanitize=address,undefined -g -o bughunt2_check bughunt2_host.c frame.c
./bughunt2_check

Expected result:
CODEC MATCHES THE SPECIFICATION  (0 failures)

OG code frame.c
#include <stdio.h>
#include <string.h>
#include "frame.h"

uint8_t frame_encode(const reading_t *r, uint8_t *out)
{
    uint8_t len = sizeof(reading_t);
    uint8_t sum = 0;
    uint8_t n   = 0;

    const uint8_t *p = (const uint8_t *)r；

    for (uint8_ti = 0; i < len - 1; i++){
        out[n++] = p[i];
        sum += p[i];
    }

    out[n++] = sum;
    return n;
}

bool frame_decode(const uint8_t *in, uint8_t in_len, reading_t *r)
{
    uint8_t payload[FRAME_MAX];
    uint8_t sum = 0;
    uint8_t len;

    if (in_len < 3)         return false;
    if (in[0] != FRAME_SOF) return false;

    len = in[1];

    for (uint8_t i = 0; i < len; i++)
        payload[i] = in[2 + i];

    for (uint8_t i = 0; i < len; i++)
        sum += payload[i];

    if (sum != in[2 + len])
        return false;

    r->sensor_id    =  (payload[0] << 8) | payload[1];
    r->status       =   payload[2];
    r->temp_cx10 = (payload[3] << 8) | payload[4];
    r->timestamp_ms = (payload[5] << 24) | (payload[6] << 16) | (payload[7] << 8) | payload[8];


    return true;
}

void hexdump(const char *label, const uint8_t *buf, uint8_t len)
{
    printf("%-10s [%2u] ", label, len);
    for (uint8_t i = 0; i < len; i++)
        printf("%02X ", buf[i]);
    printf("\n");
}

The two Pico-only defects are in bughunt2_pico.c.

6. UART text translation corrupts binary data
Current code:
uart_set_translate_crlf(LINK_UART, true);

This is intended for text output. It converts line-feed byte 0x0A into carriage-return plus line-feed:
0A -> 0D 0A

But 0x0A is a valid byte in the binary frame, specifically part of the timestamp. Therefore the transmitter adds an extra 0x0D byte that frame_encode() did not produce. The receiver then sees the wrong frame.

Fix:
uart_set_translate_crlf(LINK_UART, false);

Class: logical/protocol defect
Why correct: binary UART frames must transmit every byte unchanged.

7. Receiver does not resynchronise on the start byte
Current code collects bytes until it has 12:
buf[n++] = (uint8_t)uart_getc(LINK_UART);

if (n == 2 + FRAME_PAYLOAD + 1) {
    ...
    n = 0;
}

It never checks that:
buf[0] == FRAME_SOF

If one byte is added, lost, or corrupted, the receiver’s 12-byte window becomes shifted. It then keeps grouping bytes incorrectly forever, because it resets after every 12 bytes without searching for the next valid 0xAA start byte.

Fix the receiver so it behaves like a state machine:

Ignore bytes until 0xAA is received.
Read the length byte.
Read exactly that many payload bytes and the checksum.
Decode the frame.
If anything is invalid, return to searching for 0xAA.
At minimum, the existing code should reject a buffer whose first byte is not FRAME_SOF, but proper resynchronisation requires hunting for 0xAA again after an invalid frame.

Class: logical/state-machine defect
Why correct: a UART receiver must recover from dropped, inserted, or corrupted bytes instead of staying permanently out of step.


**Which instrument was most useful, and which one misled you?**



**What would have caught this defect automatically — a compiler flag, an
assertion, a test, a code review rule?**

- step 1:
gcc -Wall -Wextra -o bughunt2_host bughunt2_host.c frame.c
bughunt2_host

- step 2:
analyse Vector A normal
1. Wrong length: actual length: 0C, expected length: 09 （encoder calculating payload length wrongly)
2. Wrong byte order: actual: 34 12, expected: 12 34 (written in little endian)
3. Unexpected zero bytes: actual： 00 00 00 （padding/unused struct bytes, specs expects fiels to be packed directly, code may be copying c struct directly into frame)
4. Another field in the wrong order; actual: 0D 0B..., expected: 0A 0B 0C 0D (fields may be copied or indexed wrongly)
5. Checksum differs; actual:68, expected: 72 (calculated over wrong bytes, frame contents/length alr wrong)

one main reason: 
memcpy(frame, &packet, sizeof(packet));

Using memcpy on a C struct can copy compiler padding and use the machine’s native byte order. A robust encoder writes each field explicitly in the required order.

 
- step 3: undefined-behaviour sanitiser
gcc -Wall -Wextra -fsanitize=undefined -o bughunt2_ub bughunt2_host.c frame.c
./bughunt2_ub

use this if gcc doesn't work
- run the command in ubuntu
- prints file name, line and exact operation

frame.c:48:36: runtime error: left shift of 128 by 24 places
               cannot be represented in type 'int'

- step 4: hardware
gcc -Wall -Wextra -fsanitize=address,undefined -g -o bughunt2_check bughunt2_host.c frame.c
bughunt2_check

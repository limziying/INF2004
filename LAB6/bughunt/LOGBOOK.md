# Debug Logbook — Bug Hunt #6

**Name(s):** 
**Date:** 

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

| # | File & line | Class (syntax / logical / heisenbug / UB) | The defect | The fix | Why the fix is correct |
|---|---|---|---|---|---|
| 1 | bughunt6.c:57-65 | UB | The CRC table had 255 entries, but initialization wrote 256. | Allocate 256 entries. | Every possible byte index is within the table. |
| 2 | bughunt6.c:77-81 | UB | crc8 used an uninitialized accumulator. | Initialize CRC-8/ATM to zero. | The calculation now starts at its specified initial value. |
| 3 | bughunt6.c:72-75 | logical | crc8 relied on callers to initialize the lookup table first. | Initialize lazily on the first crc8 call. | The public operation is valid before an explicit setup call. |
| 4 | bughunt6.c:50-54, 91 | logical | The right-shifting LFSR used the wrong feedback mask. | Use 0xB400. | This mask produces the required maximal non-zero sequence. |
| 5 | bughunt6.c:98-106 | logical | The period measurement stopped after 200 transitions. | Permit the full 65535-state cycle. | The measured period can reach the specified value. |
| 6 | bughunt6.c:118-122 | UB | Timestamp parsing used a possibly unaligned native-endian word load. | Assemble four big-endian bytes explicitly. | The result is portable and valid on Cortex-M0+. |
| 7 | bughunt6.c:129-137 | UB | The parser trusted the supplied length before validating available bytes. | Require payload length 6 and total frame length 9. | CRC and field reads stay within a valid complete frame. |
| 8 | bughunt6.c:145-151 | UB | format_reading returned a pointer to a local stack array. | Return static storage. | The returned string remains alive after the function returns. |
| 9 | bughunt6.c:154-161 | UB | label_for allocated no byte for the NUL terminator. | Allocate strlen(name)+1 and check allocation. | strcpy has room for the complete string and failure is reported. |
| 10 | bughunt6.c:166-178 | heisenbug | The empty calibration loop could be removed by optimization. | Use a timed platform delay. | The calibration wait remains an observable timed operation. |
| 11 | bughunt6.c:185-194 | heisenbug | The foreground wait could reuse a stale ISR flag value. | Mark the shared completion flag volatile. | Each poll reloads the ISR-updated flag. |
| 12 | bughunt6.c:201-207 | logical | An 8-bit checksum index wrapped before a 300-byte buffer ended. | Use a 16-bit index. | The loop can visit every position in a uint16_t-sized buffer. |

---

## Reflection

**Which defect took longest, and what finally cracked it?**

The unaligned timestamp load is the target-only trap: a native word cast also
uses the wrong byte order, and Cortex-M0+ does not guarantee unaligned word
loads. Reading the four bytes explicitly removes both assumptions.


**Which instrument was most useful, and which one misled you?**

The host harness at -O0, -O2, and -O3 was the most useful available check; all
three builds now match the CRC, LFSR, parser, string, and checksum expectations.
The host cannot establish target alignment faults or the Pico delay, so it would
be misleading to treat those as hardware-verified.


**What would have caught this defect automatically — a compiler flag, an
assertion, a test, a code review rule?**

Boundary tests and sanitizers catch the table, parser, allocation, and index
errors. A format/type review, endian-safe parsing rule, volatile ISR-state review,
and comparing optimized disassembly catch the rest.

Validation: the host harness passes at -O0, -O2, and -O3 with zero failures and
no warnings under -Wall -Wextra -Werror; the Pico target also builds. Address/
undefined sanitizers are not available in the installed Windows GCC. Pico fault,
timing, and debugger checks remain to be done on hardware. The separate pid.c
exercise is not part of these twelve bughunt defects.

# Debug Logbook — Bug Hunt #4

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
| 1 | algo.c:5-7 | logical | The millivolt product overflowed a 16-bit intermediate. | Multiply in uint32_t. | The full 4095 x 3300 product fits before division. |
| 2 | algo.c:11-13 | logical | Integer division happened before scaling, making most duty values zero. | Multiply by wrap before dividing by ADC_MAX. | Fractional ADC levels are retained through the product. |
| 3 | algo.c:26-29 | logical | Unsigned x - y wrapped on a downward filter step. | Use signed filter state and subtraction. | The filter approaches the input from either direction. |
| 4 | algo.c:16-23 | logical | Both ADC channels shared one filter accumulator. | Store one signed state per channel and reset both. | Updating one channel no longer changes the other. |
| 5 | algo.c:32-35 | logical | PWM wrap omitted the inclusive counter endpoint. | Subtract one from the period count. | A 0..wrap counter takes wrap+1 ticks. |
| 6 | bughunt4.c:36-43 | logical | ADC channel A was read before selecting channel A. | Select each input before its read. | Each sample now corresponds to the requested GPIO channel. |
| 7 | bughunt4.c:58-61 | UB | A float argument was printed with %d. | Format voltage as integer volts and millivolts. | The variadic arguments match their format specifiers. |
| 8 | bughunt4.c:48-54 | logical | Unsigned TARGET_MV - mv was tested against zero. | Use a signed delta and leave duty unchanged at zero. | Both control directions and the equal case are represented. |

---

## Reflection

**Which defect took longest, and what finally cracked it?**

The downward-filter failure was the clearest clue: unsigned subtraction turned
a small negative difference into a very large positive value. The host step
response then verified a smooth fall after using signed arithmetic.


**Which instrument was most useful, and which one misled you?**

The synthetic host harness was most useful because it produced repeatable
endpoint, step-response, and channel-independence checks. No hardware frequency
measurement was taken here.


**What would have caught this defect automatically — a compiler flag, an
assertion, a test, a code review rule?**

The existing -Wall -Wextra build and host harness catch the format and arithmetic
issues. Tests for both rising and falling filter steps, plus a review of integer
width and signedness, catch the remaining signal-chain mistakes.

Validation: the Lab 4 host harness passes with zero failures and no warnings under
-Wall -Wextra -Werror; the Pico target also builds. PWM frequency and ADC channel
identity still need hardware checks.

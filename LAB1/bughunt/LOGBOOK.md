# Debug Logbook — Bug Hunt #1

**Name(s): Lim Zi Ying** 
**Date: 20/9/2026** 

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
1. syntax: missing #include <stdint.h> to use uint8_t 
2. syntax: missing semicolon after uint8_t count = 0; line 30
3. syntax: missing  closing } before else; line 49
4. logical: operator precedence in pin_is_clear; line 61
5. logical: value >>= 1; line 36 
6. undefined: for(int i = 0; i <= 32; i++);line 75


| # | File & line | Class (syntax / logical / heisenbug / UB) | The defect | The fix | Why the fix is correct |
|---|---|---|---|---|---|
| 1 | | | | | |
| 2 | | | | | |
| 3 | | | | | |
| 4 | | | | | |
| 5 | | | | | |
| 6 | | | | | |

---

## Reflection

**Which defect took longest, and what finally cracked it?**
Explanation for (4.) :
- return (mask & 1u << pin == 0); line 62; not sure what was wrong with this
- return mask & ((1u << pin) == 0); was interpreted as; comparison ==0 happens before bitwise &, shifted bit is not being tested against mask
- return ((mask & (1u << pin)) == 0); intended should be
- 1u means unsigned int 1
- reason why only (1u << pin) was evaluated against ==0 in the initial code, precedence order is: 1. << 2. == 3. &
- therefore need use parentheses properly.

Explanation for (5.) :
- infinite loop for value = -1
- while (value) repeats while value is not zero.
- value & 1 checks the rightmost bit:
    - result 1: the bit is set, so increment count
    - result 0: the bit is clear
- value >>= 1 shifts all bits one position to the right, allowing the next bit to be checked.

OG code:
while (value) {
    count += value & 1;
    value >>= 1;
}

Example for value = 5:
5 = 0101
5 & 1 = 1
shift: 0101 -> 0010

2 & 1 = 0
shift: 0010 -> 0001

1 & 1 = 1
shift: 0001 -> 0000

The result is count == 2.

The problem is count_bits(-1). On your compiler, shifting signed -1 right usually keeps the sign bits:
11111111 >> 1 = 11111111

So value remains -1, the loop never ends, and the program appears frozen.

Use an unsigned copy of value so right-shifting fills with zeroes:
uint8_t count_bits(int value)
{
    uint8_t count = 0;
    uint32_t bits = (uint32_t)value;

    while (bits) {
        count += bits & 1u;
        bits >>= 1;
    }

    return count;
}

Why this works:

-1 converts to 0xFFFFFFFF
The loop checks all 32 bits
Unsigned right-shift inserts zeroes
After 32 shifts, bits becomes 0
count_bits(-1) returns 32 instead of looping forever

Explanation for (6.) :
- a 32 bit value has indexes 0 through 32. At i ==32, the code performs invalid shifts.
- instead of condition i <= 32, use < 32

This line copies one bit from v into its reversed position in r:
r |= ((v >> i) & 1) << (31 - i);

Breakdown:

v >> i
Moves bit i of v to the rightmost position.

(v >> i) & 1
Extracts only that bit. The result is either 0 or 1.

... << (31 - i)
Moves that bit to its reversed position.

r |= ...
Adds the bit to r without clearing bits already added. This is shorthand for: r = r | (...);


Example when i == 0:
(v >> 0) & 1       // get bit 0 of v
bit << 31          // put it into bit 31 of r

Example when i == 31:
(v >> 31) & 1      // get bit 31 of v
bit << 0           // put it into bit 0 of r

That is why the function reverses the bits. A slightly clearer version is:
r |= ((v >> i) & 1u) << (31 - i);

**Which instrument was most useful, and which one misled you?**

**What would have caught this defect automatically — a compiler flag, an
assertion, a test, a code review rule?**

- use ninja -C build -v -j1
- fix 1 error then cmake --build build
- once fix all, clean build cmake --build build --clean-first --parallel 1
- flash the uf2 file onto pico
- open serial monitor, see same output


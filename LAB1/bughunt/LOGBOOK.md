# Debug Logbook — Bug Hunt #1

**Name(s): Lim Zi Ying** 
**Date: 20/9/2026** 

One row per hypothesis. **Include the hypotheses that turned out to be wrong** —
they are the evidence that you were reasoning rather than guessing.

| # | Symptom observed | Hypothesis | Experiment (ONE change) | Predicted | Result | Conclusion |
|---|---|---|---|---|---|---|
| 1 | Compiler does not recognize `uint8_t`. | The fixed-width integer type is declared in a missing standard header. | Add `#include <stdint.h>`. | The unknown-type diagnostics should disappear. | The type is available; compilation proceeds to later errors. | Include diagnostics point to missing declarations before changing function logic. |
| 2 | Compiler reports an error at the `while` statement in `count_bits`. | The preceding initialization statement is incomplete. | Add the missing semicolon after `uint8_t count = 0`. | The parser should accept the loop and report any remaining syntax issue. | The `while`-line error is resolved. | The reported line can be after the actual syntax defect. |
| 3 | Compiler reports an error around `else` in `even_parity`. | The `if` block is not closed before `else`. | Add the missing `}` before `else`. | The function should parse, exposing any runtime failures. | The syntax error is resolved. | Read the preceding control-flow block when an `else` is reported as invalid. |
| 4 | `pin_is_clear` does not test the requested mask bit correctly. | Operator precedence groups the equality test before the bitwise mask operation. | Parenthesize `(mask & (1u << pin))` before comparing it with zero. | Pins 2 and 5 in mask `0x24` should be reported set, and pin 3 clear. | The expression matches all three specified mask cases. | Parentheses make the intended mask-then-compare order explicit. |
| 5 | `count_bits(-1)` does not finish. | Right-shifting a negative signed value can preserve its sign bit. | Copy the input to `uint32_t` and shift that unsigned value. | All 32 one-bits should be counted, then the loop should terminate. | The unsigned copy reaches zero after 32 shifts and returns 32. | Use unsigned arithmetic for a finite bit scan of the input's 32-bit representation. |
| 6 | `reverse_bits` performs an invalid shift at the final loop iteration. | The loop includes index 32, outside the valid bit indices 0 through 31. | Change the loop condition from `i <= 32` to `i < 32`. | Only shifts from bit positions 0 through 31 should occur. | The 32-bit reversal cases match their expected values without an out-of-range shift. | A 32-bit word has 32 positions, indexed 0 through 31. |
| 7 | Considered whether the even-parity rule was inverted. | The implementation might be checking for an odd number of set bits. | Compare the specified `0xFF` and `0x07` cases against the parity rule. | Eight set bits should return true; three should return false. | The intended even-parity condition is consistent with those cases. | The parity logic itself was not a planted defect; the parse error was. |
| 8 | Considered whether `swap_nibbles` also needed a fix. | The nibble masks or shift directions might be reversed. | Check `0x12345678` against the stated result `0x21436587`. | Each byte's high and low nibbles should exchange places. | The existing expression produces the specified transformation. | This helper was a check, not an additional defect. |

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
| 1 | bughunt1.c:15 | syntax | `uint8_t` was used without including its declaration. | Include `<stdint.h>`. | The standard header declares the fixed-width integer type. |
| 2 | bughunt1.c:30 | syntax | The `count` initialization lacked a terminating semicolon. | Add `;` after `uint8_t count = 0`. | The declaration becomes a complete C statement. |
| 3 | bughunt1.c:49 | syntax | The `if` block in `even_parity` was not closed before `else`. | Add the missing closing brace. | The `else` is then paired with the intended `if`. |
| 4 | bughunt1.c:61-62 | logical | Operator precedence caused the pin test to compare the shifted mask expression to zero before applying `mask`. | Use `((mask & (1u << pin)) == 0)`. | The requested bit is first isolated from the mask, then tested for clear. |
| 5 | bughunt1.c:36 | logical | Right-shifting signed `-1` can preserve the sign bit, so the loop may not terminate. | Convert the input to `uint32_t` and shift the unsigned copy. | Unsigned right shifts insert zeroes and the 32-bit scan terminates. |
| 6 | bughunt1.c:75 | UB | The loop included `i == 32`, causing shifts by an amount equal to the word width. | Iterate while `i < 32`. | The loop only shifts valid bit positions 0 through 31. |

---

## Reflection

**Which defect took longest, and what finally cracked it?**

The `pin_is_clear` precedence issue took the most reasoning. Writing out the
operator grouping showed that the comparison was applied to the shifted pin
mask before the intended bitwise-AND with `mask`. Explicit parentheses fixed
the order.

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

The compiler diagnostics were most useful for the three syntax defects, and the
test harness exposed the incorrect bit-manipulation results. The source line
reported by the compiler could be misleading when the missing punctuation was
on the preceding line, so I checked the surrounding lines instead of changing
the reported line blindly.

**What would have caught this defect automatically — a compiler flag, an
assertion, a test, a code review rule?**

`-Wall -Wextra` plus compiling after each syntax fix catches the declaration and
punctuation problems. The existing test cases catch the parity, mask, and
reversal behavior. A negative-input test for `count_bits` and a sanitizer run
would draw attention to nontermination and the out-of-range shift.

- use ninja -C build -v -j1
- fix 1 error then cmake --build build
- once fix all, clean build cmake --build build --clean-first --parallel 1
- flash the uf2 file onto pico
- open serial monitor, see same output

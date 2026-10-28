# Debug Logbook — Bug Hunt #3

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
| 1 | bughunt3.c:33-36 | heisenbug | ISR-updated values were not volatile; Release could cache pulse_count. | Mark ISR/main shared values volatile. | Each poll reloads the updated count. |
| 2 | bughunt3.c:120-125 | heisenbug | A 64-bit timestamp can be torn on the 32-bit Cortex-M0+. | Disable interrupts while taking the summary snapshot. | Main reads all ISR values from one consistent state. |
| 3 | bughunt3.c:60-61 | logical | Adding the debounce interval can overflow at timer rollover. | Compare unsigned elapsed time, now - last. | Unsigned subtraction gives the elapsed interval across wraparound. |
| 4 | bughunt3.c:27-28 | logical | A 50 ms debounce rejects legitimate wheel edges. | Reduce it to 1 ms for the slow manual-wheel exercise. | The threshold no longer suppresses ordinary pulse edges. |
| 5 | bughunt3.c:73-85 | logical | The ENC_IDLE case fell through and completed a slot on its starting edge. | Use distinct falling-edge start and rising-edge finish transitions. | A pulse is counted only after both edges. |
| 6 | bughunt3.c:55-87 | heisenbug | printf in the ISR can block long enough to lose edges. | Remove ISR printing; report from main after sampling. | Slow USB output no longer delays edge handling. |
| 7 | bughunt3.c:110-113 | logical | Only falling edges were enabled, so the pulse end was never measured. | Enable rising and falling edge interrupts. | The ISR sees both edges needed to measure pulse width. |
| 8 | bughunt3.c:134-135 | UB | The 64-bit timestamp was passed to printf with %u. | Use %llu and cast to unsigned long long. | The format and variadic argument types now match. |

---

## Reflection

**Which defect took longest, and what finally cracked it?**

The Release-only hang was explained by the wait-loop disassembly: without volatile,
the compiler read pulse_count once and turned the loop into a permanent branch.
After the fix, the Release build reloads the count inside the loop.


**Which instrument was most useful, and which one misled you?**

Comparing the Debug and Release disassembly was the most useful way to expose the
optimization-dependent hang. Source-level inspection alone did not show why the
same loop behaved differently.


**What would have caught this defect automatically — a compiler flag, an
assertion, a test, a code review rule?**

Format warnings would catch the printf mismatch. A review checklist for ISR-shared
state, rollover-safe timing, and ISR work would catch the other defects; a release
build and disassembly comparison would catch the optimization-dependent failure.

Validation: Debug and Release Pico builds succeed, and the Release wait loop reloads
pulse_count. Hardware pulse-width and speed measurements still need to be checked
on the Pico.
- step 1: cd LAB3\bughunt
- step 2: separate build folder for debug, so can compare binary side by side without rebuilding back and forth.
    - mkdir build-debug
- step 3: In powershell, copy pico_sdk into lab folder
    - cd C:\Users\limzi\OneDrive\Documents\GitHub\INF2004\LAB3\bughunt
    - Copy-Item "$env:PICO_SDK_PATH\external\pico_sdk_import.cmake" ".\"
    - Test-Path ".\pico_sdk_import.cmake"
- step 4: configure the debug build at -O0
    -  cd build-debug
    - cmake -G Ninja -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Debug -DPICO_DEOPTIMIZED_DEBUG=1 ..
- step 5: build and flash the debug version
    - ninja (produce uf2)
    - flash to pico
    - Debug (-O0): turn the wheel, and you should see live serial output — edge: count=1 width=... us, counting up — and once it hits TARGET_SLOTS (20), it prints the done. summary with final stats and stops.
- step 6: create separate build folder for release
    - cd .. (back to the bughunt project folder)
    - mkdir build-release, cd build-release
- step 7: configure the release build at -O3
    - cmake -G Ninja -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Release ..
- step 8: build & flash the release version
    - ninja
    - flash pico
    - Release (-O3): per the exercise title, this build is expected to hang — meaning you’d turn the wheel, maybe see some edges printed (or maybe none at all, worth noting which), but it never reaches done. — the while (pulse_count < TARGET_SLOTS) loop in main() never exits, even though the wheel is clearly moving and the ISR appears to still be counting.

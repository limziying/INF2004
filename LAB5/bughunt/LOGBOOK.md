# Debug Logbook — Bug Hunt #5

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
| 1 | bughunt5.c:100-101 | logical | sizeof(&sample) sent pointer bytes instead of the two-byte sample. | Send sizeof sample. | The receiver gets exactly the published value. |
| 2 | bughunt5.c:37-38, 101-105 | logical | Two tasks read one message buffer, which is not a broadcast channel. | Give the moving-average and running-mean tasks separate input buffers. | Both consumers receive every sample independently. |
| 3 | bughunt5.c:123-134 | heisenbug | Two consumers modified the same ring without synchronization. | Make the ring local to avg_task. | A single task owns every ring update and sum. |
| 4 | bughunt5.c:135-137 | logical | The moving average divided by ten before ten samples existed. | Divide by the current fill count. | The warm-up average uses only received samples. |
| 5 | bughunt5.c:149-157 | logical | A static running-mean accumulator was shared by both callers. | Keep sum and count in mean_task alone. | The running mean is based only on its own complete sample stream. |
| 6 | bughunt5.c:180-184, 211-214; FreeRTOSConfig.h:45-47 | heisenbug | The print task had little stack margin and overflow checking was disabled. | Increase its stack, enable overflow hooks, and report high-water marks. | Stack use can be measured and overflow is detected rather than hidden. |
| 7 | bughunt5.c:75-77, 101-106 | logical | Failed queue/message-buffer sends were ignored. | Block print sends and assert that sample sends succeed. | Backpressure or lost samples become visible instead of silent. |
| 8 | bughunt5.c:92-109 | logical | vTaskDelay(1) delayed one tick; the 100 ms setting was unused. | Use vTaskDelayUntil with SAMPLE_PERIOD_MS. | Sampling follows the specified cadence without processing-time drift. |
| 9 | FreeRTOSConfig.h:89 | logical | configASSERT was empty, suppressing kernel checks. | Enable assertions; also turn on allocation and stack hooks. | Kernel precondition and resource failures are no longer silently discarded. |

---

## Reflection

**Which defect took longest, and what finally cracked it?**

The message-buffer design required the biggest correction: a message buffer has
one reader, so two consumers cannot each receive every sample from it. Separate
buffers make the fan-out explicit and keep each calculation independent.


**Which instrument was most useful, and which one misled you?**

Checking each send size against the corresponding receive size exposed the first
failure immediately. Stack high-water marks and the enabled FreeRTOS hooks are
the right hardware instruments for the stack question; they were not measured
in this environment.


**What would have caught this defect automatically — a compiler flag, an
assertion, a test, a code review rule?**

Assertions on every task/resource creation and send/receive result catch failures.
A deterministic sample stream (10, 20, ...), ownership review, and a test for
warm-up averages and independent consumers catch the logic defects.

Validation: code and logbook updated, but no Lab 5 build or board run was possible
here because pico_sdk_import.cmake and FREERTOS_KERNEL_PATH are not configured.
Verify compilation, stack high-water marks, and the sample stream on the target.

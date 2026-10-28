/*
 * BUG HUNT #5 - It failed once, an hour in
 * INF2004 LAB 5
 *
 * The architecture from the Lab 5 exercise, built out properly:
 *
 *      sensor_task   reads the RP2040 temperature sensor every
 *                    SAMPLE_PERIOD_MS and publishes each sample
 *      avg_task      moving average over the last RING_LEN samples
 *      mean_task     simple running mean since boot
 *      print_task    the ONLY task allowed to call printf
 *
 * Requirement, unchanged from the lab: no printf outside print_task.
 *
 * NINE defects are planted, one of them in FreeRTOSConfig.h. It compiles, it
 * links, and it will run for a while. That is the problem.
 */

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "message_buffer.h"

#define RING_LEN          10u
#define MBUF_BYTES       256u
#define PRINT_Q_LEN        8u
#define SAMPLE_PERIOD_MS 100u

/* ---- shared state ---- */
static MessageBufferHandle_t avg_mbuf;
static MessageBufferHandle_t mean_mbuf;
static QueueHandle_t         print_q;
static TaskHandle_t sensor_handle;
static TaskHandle_t avg_handle;
static TaskHandle_t mean_handle;
static TaskHandle_t print_handle;

typedef enum {
    MSG_MOVING_AVG,
    MSG_RUNNING_MEAN,
} msg_kind_t;

typedef struct {
    msg_kind_t kind;
    float      value;
} print_msg_t;

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    (void)task_name;
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

/* ------------------------------------------------------------------
 * Hand a message to print_task. No task except print_task may printf.
 * ------------------------------------------------------------------ */
static void say(msg_kind_t kind, float value)
{
    print_msg_t m = { .kind = kind, .value = value };
    BaseType_t sent = xQueueSend(print_q, &m, portMAX_DELAY);
    configASSERT(sent == pdPASS);
}

/* ------------------------------------------------------------------
 * Tasks
 * ------------------------------------------------------------------ */
static void sensor_task(void *params)
{
    (void)params;

    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    /* Bug 8: maintain the specified cadence without accumulating drift. */
    TickType_t next_sample = xTaskGetTickCount();
    while (true) {
        uint16_t raw = adc_read();

        /* RP2040 datasheet transfer function; x10 to keep one decimal */
        float volts = raw * 3.3f / 4096.0f;
        float degc  = 27.0f - (volts - 0.706f) / 0.001721f;
        int16_t sample = (int16_t)(degc * 10.0f);

        /* Bug 1: send the sample bytes, not the pointer's bytes. */
        size_t sent = xMessageBufferSend(avg_mbuf, &sample, sizeof sample, 0);
        /* Bug 2: separate single-reader buffers deliver each sample twice. */
        configASSERT(sent == sizeof sample);
        sent = xMessageBufferSend(mean_mbuf, &sample, sizeof sample, 0);
        /* Bug 7: detect backpressure instead of silently losing a sample. */
        configASSERT(sent == sizeof sample);

        vTaskDelayUntil(&next_sample, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

static void avg_task(void *params)
{
    (void)params;

    while (true) {
        int16_t sample;
        size_t received = xMessageBufferReceive(avg_mbuf, &sample, sizeof sample, portMAX_DELAY);
        configASSERT(received == sizeof sample);

        /* Bug 3: the moving-average ring belongs only to this consumer. */
        static int16_t ring[RING_LEN] = {0};
        static unsigned head = 0;
        static unsigned fill = 0;
        static int32_t sum = 0;
        sum -= ring[head];
        ring[head] = sample;
        sum += sample;
        head = (head + 1u) % RING_LEN;
        if (fill < RING_LEN)
            fill++;

        /* Bug 4: divide by valid samples, not the full window during warm-up. */
        say(MSG_MOVING_AVG, (float)sum / (float)fill / 10.0f);
    }
}

static void mean_task(void *params)
{
    (void)params;

    while (true) {
        int16_t sample;
        size_t received = xMessageBufferReceive(mean_mbuf, &sample, sizeof sample, portMAX_DELAY);
        configASSERT(received == sizeof sample);

        /* Bug 5: only this task updates the running-mean history. */
        static int64_t sum = 0;
        static uint64_t count = 0;
        configASSERT(count < UINT64_MAX);
        configASSERT(sample <= 0 || sum <= INT64_MAX - sample);
        configASSERT(sample >= 0 || sum >= INT64_MIN - sample);
        sum += sample;
        count++;
        say(MSG_RUNNING_MEAN, (float)((double)sum / (double)count / 10.0));
    }
}

static void print_task(void *params)
{
    (void)params;

    TickType_t last_health = xTaskGetTickCount();
    while (true) {
        print_msg_t m;
        if (xQueueReceive(print_q, &m, pdMS_TO_TICKS(100)) == pdPASS) {
            switch (m.kind) {
                case MSG_MOVING_AVG:
                    printf("moving avg (last %u): %.1f C\n",
                           (unsigned)RING_LEN, (double)m.value);
                    break;
                case MSG_RUNNING_MEAN:
                    printf("running mean since boot: %.1f C\n", (double)m.value);
                    break;
            }
        }

        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - last_health) >= pdMS_TO_TICKS(1000)) {
            printf("stack minimum free (words): sensor=%lu avg=%lu mean=%lu print=%lu; heap=%lu bytes\n",
                   (unsigned long)uxTaskGetStackHighWaterMark(sensor_handle),
                   (unsigned long)uxTaskGetStackHighWaterMark(avg_handle),
                   (unsigned long)uxTaskGetStackHighWaterMark(mean_handle),
                   (unsigned long)uxTaskGetStackHighWaterMark(print_handle),
                   (unsigned long)xPortGetFreeHeapSize());
            last_health = now;
        }
    }
}

int main(void)
{
    stdio_init_all();
    sleep_ms(3000);

    avg_mbuf = xMessageBufferCreate(MBUF_BYTES);
    mean_mbuf = xMessageBufferCreate(MBUF_BYTES);
    print_q = xQueueCreate(PRINT_Q_LEN, sizeof(print_msg_t));
    configASSERT(avg_mbuf != NULL && mean_mbuf != NULL && print_q != NULL);

    BaseType_t created;
    created = xTaskCreate(sensor_task, "sensor", configMINIMAL_STACK_SIZE * 2u,
                          NULL, 5, &sensor_handle);
    configASSERT(created == pdPASS);
    created = xTaskCreate(avg_task, "avg", configMINIMAL_STACK_SIZE * 2u,
                          NULL, 3, &avg_handle);
    configASSERT(created == pdPASS);
    created = xTaskCreate(mean_task, "mean", configMINIMAL_STACK_SIZE * 2u,
                          NULL, 3, &mean_handle);
    configASSERT(created == pdPASS);
    /* Bug 6: allocate a monitored stack margin for formatted output. */
    created = xTaskCreate(print_task, "print", configMINIMAL_STACK_SIZE * 4u,
                          NULL, 2, &print_handle);
    configASSERT(created == pdPASS);

    vTaskStartScheduler();
    panic("Scheduler did not start");
}

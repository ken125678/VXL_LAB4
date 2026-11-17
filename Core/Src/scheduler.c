#include "scheduler.h"
#include <stddef.h> /* NULL */

typedef struct {
    void (*pTask)(void);   /* Callback executed in main, not in ISR. */
    uint32_t NextRunTick;  /* Absolute tick at which callback becomes due. */
    uint32_t PeriodTicks;  /* 0 => one-shot. */
    uint32_t TaskID;       /* ID independent of the array index. */
} sTask;

static sTask SCH_tasks_G[SCH_MAX_TASKS];
static volatile uint32_t sch_tick = 0u;
static uint32_t next_task_id = 0u;

/* Valid for intervals strictly shorter than 2^31 ticks. */
static uint8_t tick_reached(uint32_t now, uint32_t deadline) {
    return ((int32_t)(now - deadline) >= 0) ? 1u : 0u;
}

static void clear_task(uint32_t i) {
    SCH_tasks_G[i].pTask = NULL;
    SCH_tasks_G[i].NextRunTick = 0u;
    SCH_tasks_G[i].PeriodTicks = 0u;
    SCH_tasks_G[i].TaskID = 0u;
}

void SCH_Init(void) {
    sch_tick = 0u;
    next_task_id = 0u;
    for (uint32_t i = 0u; i < SCH_MAX_TASKS; ++i) {
        clear_task(i);
    }
}

void SCH_Update(void) {
    /* O(1): ISR does not search for tasks or execute callbacks. */
    ++sch_tick;
}

uint32_t SCH_GetTick(void) {
    /* Aligned 32-bit access is atomic on STM32F103 Cortex-M3. */
    return sch_tick;
}

uint32_t SCH_GetTimeMs(void) {
    return SCH_GetTick() * SCH_TICK_MS;
}

uint32_t SCH_Add_Task(void (*pFunction)(void), uint32_t DELAY, uint32_t PERIOD) {
    if (pFunction == NULL || DELAY >= 0x80000000u || PERIOD >= 0x80000000u) {
        return SCH_INVALID_TASK_ID;
    }

    /* This function is called in main context; only sch_tick changes in ISR. */
    for (uint32_t i = 0u; i < SCH_MAX_TASKS; ++i) {
        if (SCH_tasks_G[i].pTask == NULL) {
            /* Monotonic task IDs so a reused slot is not confused with its old task. */
            ++next_task_id;
            if (next_task_id == SCH_INVALID_TASK_ID) ++next_task_id;
            SCH_tasks_G[i].NextRunTick = SCH_GetTick() + DELAY;
            SCH_tasks_G[i].PeriodTicks = PERIOD;
            SCH_tasks_G[i].TaskID = next_task_id;
            SCH_tasks_G[i].pTask = pFunction;
            return next_task_id;
        }
    }
    return SCH_INVALID_TASK_ID; /* Task table full. */
}

uint8_t SCH_Delete_Task(uint32_t taskID) {
    if (taskID == SCH_INVALID_TASK_ID) return 0u;
    for (uint32_t i = 0u; i < SCH_MAX_TASKS; ++i) {
        if (SCH_tasks_G[i].pTask != NULL && SCH_tasks_G[i].TaskID == taskID) {
            clear_task(i);
            return 1u;
        }
    }
    return 0u;
}

void SCH_Dispatch_Tasks(void) {
    /* Capture one snapshot: stable ordering even if timer advances mid-dispatch. */
    const uint32_t now = SCH_GetTick();

    /* At most one execution per registered task in a dispatcher call. */
    for (uint32_t dispatched = 0u; dispatched < SCH_MAX_TASKS; ++dispatched) {
        int32_t best_index = -1;
        uint32_t best_lateness = 0u;

        /* Search for the earliest due deadline (the most overdue task). */
        for (uint32_t i = 0u; i < SCH_MAX_TASKS; ++i) {
            const sTask *task = &SCH_tasks_G[i];
            if (task->pTask == NULL || !tick_reached(now, task->NextRunTick)) {
                continue;
            }
            const uint32_t lateness = now - task->NextRunTick;
            if (best_index < 0 || lateness > best_lateness ||
                (lateness == best_lateness && task->TaskID <
                 SCH_tasks_G[(uint32_t)best_index].TaskID)) {
                best_index = (int32_t)i;
                best_lateness = lateness;
            }
        }

        if (best_index < 0) break;

        sTask *task = &SCH_tasks_G[(uint32_t)best_index];
        void (*callback)(void) = task->pTask;
        if (task->PeriodTicks == 0u) {
            /* Free slot BEFORE call, so callback can safely add a task. */
            clear_task((uint32_t)best_index);
        } else {
            /* Skip overdue occurrences; avoid an infinite catch-up loop. */
            const uint32_t missed = (now - task->NextRunTick) / task->PeriodTicks;
            task->NextRunTick += (missed + 1u) * task->PeriodTicks;
        }

        callback(); /* Runs in main context: keep each task short/non-blocking. */
    }
}

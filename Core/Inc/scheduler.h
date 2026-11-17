
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

// 1 tick = 10 milliseconds
#define SCH_TICK_MS          10u

// Maximum number of tasks
#define SCH_MAX_TASKS        12u

// Return 0 if task registration fails
#define SCH_INVALID_TASK_ID  0u

// Convert milliseconds to ticks (round up)
#define SCH_MS_TO_TICKS(ms) \
    (((uint32_t)(ms) + SCH_TICK_MS - 1u) / SCH_TICK_MS)

// Initialize scheduler and task array
void SCH_Init(void);

// Called every 10ms in TIM2 interrupt
void SCH_Update(void);

// Check and execute tasks in main loop
void SCH_Dispatch_Tasks(void);

// Register a new task
// DELAY: time before first execution (ticks)
// PERIOD: repeat interval (ticks)
// PERIOD = 0 means one-shot task
uint32_t SCH_Add_Task(
    void (*pFunction)(void),
    uint32_t DELAY,
    uint32_t PERIOD
);

// Remove a task using its ID
// Return 1 if successful, 0 otherwise
uint8_t SCH_Delete_Task(uint32_t taskID);

// Read scheduler time
uint32_t SCH_GetTick(void);
uint32_t SCH_GetTimeMs(void);

#endif

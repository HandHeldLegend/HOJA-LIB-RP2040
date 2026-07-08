#ifndef HOJA_TASKS_H
#define HOJA_TASKS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*task_fn_t)(uint64_t now_us);

typedef enum
{
    TASK_TYPE_REQUIRED  = 0b00000001, // Must run once per task period
    TASK_TYPE_RECURRING = 0b00000010, // Will iterate through during free time
    TASK_TYPE_OPTIONAL  = 0b00000100, // One optional task runs per task period
    TASK_TYPE_RAPID     = 0b00001000, // Runs every tasks_run() call
    TASK_TYPE_SHUTDOWN  = 0b00010000, // Task is enabled during shutdown lock
    TASK_TYPE_MOTION    = 0b00100000, // Guaranteed once per cycle, then evenly spaced by motion_interval_us
} task_type_t;

typedef struct
{
    const char *name;
    uint64_t max_runtime_us;
    uint64_t last_run_us; // Last time this operation was run
    uint64_t optional_interval_us; // Interval for optional tasks
    uint64_t motion_interval_us; // Even-spacing interval for motion tasks (extra reads within a cycle)
    uint8_t  type_mask; // task_type_t to form the mask
    uint32_t required_done_flag; // Bit set in tasks_sm when this required/motion task completes its guaranteed run for the cycle
    task_fn_t fn;
} task_s;

void tasks_mark_shutdown(void);

void tasks_reset(void);

void tasks_register(task_s *task);

// Call once at the start of each main-loop iteration.
void tasks_mark_sent_isr(void);
void tasks_mark_sent(void);

void tasks_run(void);

// True once every registered required task has run since the last tasks_mark_sent().
bool tasks_get_required_done(void);

#ifdef __cplusplus
}
#endif

#endif

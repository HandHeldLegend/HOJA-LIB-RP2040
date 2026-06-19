#ifndef HOJA_TASKS_H
#define HOJA_TASKS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*task_fn_t)(uint64_t now_us);

typedef struct
{
    const char *name;
    uint64_t last_us;
    uint64_t interval_us;
    uint64_t max_runtime_us;
    task_fn_t fn;
} task_s;

typedef struct
{
    uint64_t start_us;
    uint64_t max_us;
} task_benchmark_s;

static inline void tasks_bm_start(task_benchmark_s *bm, uint64_t now_us)
{
    bm->start_us = now_us;
}

static inline void tasks_bm_end(task_benchmark_s *bm, uint64_t now_us)
{
    uint64_t elapsed = now_us - bm->start_us;
    if (elapsed > bm->max_us)
    {
        bm->max_us = elapsed;
    }
}

void tasks_register_required(task_s *task);
void tasks_register_optional(task_s *task);
void tasks_register_idle(task_s *task);

// Call once at the start of each main-loop iteration.
void tasks_mark_start(void);

// Run at most one task callback per call.
void tasks_task(void);

#ifdef __cplusplus
}
#endif

#endif

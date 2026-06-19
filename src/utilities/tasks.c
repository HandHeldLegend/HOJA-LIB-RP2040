#include "utilities/tasks.h"
#include "hal/sys_hal.h"
#include <stddef.h>

#define TASKS_MAX_COUNT 32

// Estimated runtime for tasks that have not been measured yet.
#define TASKS_DEFAULT_RUNTIME_US 1

static uint8_t _required_task_count = 0;
static task_s *_required_tasks[TASKS_MAX_COUNT] = {0};

static uint8_t _optional_task_count = 0;
static task_s *_optional_tasks[TASKS_MAX_COUNT] = {0};
static uint8_t _optional_scan_index = 0;

static uint8_t _idle_task_count = 0;
static task_s *_idle_tasks[TASKS_MAX_COUNT] = {0};
static uint8_t _idle_index = 0;

static uint64_t _task_last_mark_us = 0;
static uint64_t _task_frame_deadline_us = 0;

static uint8_t _required_index = 0;

typedef enum
{
    TASKS_PHASE_REQUIRED, // Each required task runs once per loop.
    TASKS_PHASE_OPTIONAL, // At most one due optional task per loop.
    TASKS_PHASE_IDLE,       // Fill remaining frame time with idle tasks.
} tasks_phase_t;

static tasks_phase_t _phase = TASKS_PHASE_REQUIRED;

static inline bool _task_is_due(const task_s *task, uint64_t now_us)
{
    if (task->interval_us == 0)
    {
        return true;
    }

    return now_us >= (task->last_us + task->interval_us);
}

static inline uint64_t _task_budget_us(const task_s *task)
{
    return task->max_runtime_us ? task->max_runtime_us : TASKS_DEFAULT_RUNTIME_US;
}

static inline bool _task_can_run(uint64_t now_us, uint64_t required_runtime_us)
{
    return (now_us + required_runtime_us) <= _task_frame_deadline_us;
}

static void _task_run(task_s *task, uint64_t now_us, bool track_runtime)
{
    uint64_t start_us = now_us;

    task->fn(now_us);

    now_us = sys_hal_now_us();
    task->last_us = start_us;

    if (!track_runtime)
    {
        return;
    }

    uint64_t runtime_us = now_us - start_us;
    if (runtime_us > task->max_runtime_us)
    {
        task->max_runtime_us = runtime_us;
    }
}

static int8_t _optional_pick_due(uint64_t now_us)
{
    if (_optional_task_count == 0)
    {
        return -1;
    }

    for (uint8_t step = 0; step < _optional_task_count; step++)
    {
        uint8_t idx = (_optional_scan_index + step) % _optional_task_count;
        task_s *task = _optional_tasks[idx];

        if (!_task_is_due(task, now_us))
        {
            continue;
        }

        if (!_task_can_run(now_us, _task_budget_us(task)))
        {
            continue;
        }

        _optional_scan_index = (idx + 1) % _optional_task_count;
        return (int8_t)idx;
    }

    return -1;
}

void tasks_register_required(task_s *task)
{
    if (_required_task_count >= TASKS_MAX_COUNT)
    {
        return;
    }

    if (!task || !task->fn)
    {
        return;
    }

    _required_tasks[_required_task_count++] = task;
}

void tasks_register_optional(task_s *task)
{
    if (_optional_task_count >= TASKS_MAX_COUNT)
    {
        return;
    }

    if (!task || !task->fn)
    {
        return;
    }

    _optional_tasks[_optional_task_count++] = task;
}

void tasks_register_idle(task_s *task)
{
    if (_idle_task_count >= TASKS_MAX_COUNT)
    {
        return;
    }

    if (!task || !task->fn)
    {
        return;
    }

    _idle_tasks[_idle_task_count++] = task;
}

void tasks_mark_start(void)
{
    uint64_t now_us = sys_hal_now_us();
    uint64_t period_us = 0;

    if (_task_last_mark_us != 0)
    {
        period_us = now_us - _task_last_mark_us;
    }

    _task_last_mark_us = now_us;

    if (period_us == 0)
    {
        // First frame, or zero-length loop: allow slack work until measured.
        _task_frame_deadline_us = UINT64_MAX;
    }
    else
    {
        _task_frame_deadline_us = now_us + period_us;
    }

    _required_index = 0;
    _phase = TASKS_PHASE_REQUIRED;
}

void tasks_task(void)
{
    task_s *task = NULL;
    uint64_t now_us = 0;
    int8_t optional_idx = -1;

    switch (_phase)
    {
        default:
            return;

        case TASKS_PHASE_REQUIRED:
            if (_required_task_count == 0)
            {
                _phase = TASKS_PHASE_OPTIONAL;
                return;
            }

            task = _required_tasks[_required_index];
            now_us = sys_hal_now_us();
            _task_run(task, now_us, true);

            _required_index++;
            if (_required_index >= _required_task_count)
            {
                _required_index = 0;
                _phase = TASKS_PHASE_OPTIONAL;
            }
            return;

        case TASKS_PHASE_OPTIONAL:
            now_us = sys_hal_now_us();
            optional_idx = _optional_pick_due(now_us);

            if (optional_idx >= 0)
            {
                task = _optional_tasks[(uint8_t)optional_idx];
                _task_run(task, now_us, true);
            }

            _phase = TASKS_PHASE_IDLE;
            return;

        case TASKS_PHASE_IDLE:
            if (_idle_task_count == 0)
            {
                return;
            }

            task = _idle_tasks[_idle_index];
            now_us = sys_hal_now_us();

            if (!_task_can_run(now_us, _task_budget_us(task)))
            {
                return;
            }

            _task_run(task, now_us, true);

            _idle_index++;
            if (_idle_index >= _idle_task_count)
            {
                _idle_index = 0;
            }
            return;
    }
}

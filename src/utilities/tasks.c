#include "utilities/tasks.h"
#include "hal/sys_hal.h"
#include <stddef.h>
#include <string.h>

#define TASKS_MAX_COUNT 8

// Estimated runtime for tasks that have not been measured yet.
#define TASKS_DEFAULT_RUNTIME_US 1
#define TASK_FORCE_RESET_US (8 * 1000) // 8ms forceful reset
#define RUNTIME_MAX_DO_UPDATE true 
#define RUNTIME_MAX_NO_UPDATE false

volatile bool _tasks_sent_isr_signal = false; 
volatile bool _tasks_shutdown_lock = false;

typedef enum
{
    TASK_PHASE_REQUIRED,
    TASK_PHASE_RECURRING,
} task_phase_t;

typedef struct
{
    task_s   *required[TASKS_MAX_COUNT]; 
    uint8_t   required_count;
    uint8_t   required_completed;

    task_s   *recurring[TASKS_MAX_COUNT]; 
    uint8_t   recurring_count;
    uint8_t   recurring_completed;

    task_s   *optional[TASKS_MAX_COUNT]; 
    uint8_t   optional_count;
    // optional_completed removed: selection is now interval-aware, not round-robin

    task_s   *rapid[TASKS_MAX_COUNT];
    uint8_t   rapid_count;

    uint64_t cycle_last_reset_us;
    uint64_t cycle_deadline_us; 
    uint64_t cycle_reset_deadline_us;
} tasks_sm_s; 

static tasks_sm_s _tasks_sm;
static task_phase_t _tasks_phase = TASK_PHASE_REQUIRED;

void tasks_mark_shutdown(void)
{
    _tasks_shutdown_lock = true;
}

void tasks_reset(void)
{
    memset(&_tasks_sm, 0, sizeof(tasks_sm_s));
    _tasks_phase = TASK_PHASE_REQUIRED;
}

static void _tasks_reset_sm(void)
{
    uint64_t now_us = sys_hal_now_us();
    uint64_t period = (_tasks_sm.cycle_last_reset_us == 0)
    ? TASK_FORCE_RESET_US
    : now_us - _tasks_sm.cycle_last_reset_us;
    _tasks_sm.cycle_last_reset_us = now_us;

    _tasks_sm.cycle_deadline_us        = now_us + period;
    _tasks_sm.cycle_reset_deadline_us  = now_us + TASK_FORCE_RESET_US;

    _tasks_sm.required_completed = 0;

    _tasks_phase = TASK_PHASE_REQUIRED;
}

static bool _tasks_runnable(const task_s *task)
{
    return !(_tasks_shutdown_lock && !(task->type_mask & TASK_TYPE_SHUTDOWN));
}

static bool _tasks_interval_due(const task_s *task, uint64_t now_us)
{
    if (!(task->type_mask & TASK_TYPE_OPTIONAL) || task->optional_interval_us == 0)
    {
        return true;
    }

    return now_us > (task->last_run_us + task->optional_interval_us);
}

static bool _tasks_force_run(task_s *task, bool update_max_runtime)
{
    if(!_tasks_runnable(task)) return false;

    uint64_t start_us = sys_hal_now_us();
    task->fn(start_us);
    uint64_t end_us = sys_hal_now_us();
    task->last_run_us = start_us;

    if(!update_max_runtime) return true;

    uint64_t runtime_us = end_us - start_us;
    if(runtime_us > task->max_runtime_us)
    {
        task->max_runtime_us = runtime_us;
    }
    return true;
}

// Runs the single most-overdue optional task (if any are past their interval).
// Tasks that have never run (last_run_us == 0) are treated as maximally overdue.
static void _tasks_optional_run(task_s *optional_tasks[TASKS_MAX_COUNT], uint8_t count, uint64_t now_us)
{
    task_s *t = NULL;
    uint64_t max_delta = 0;
    for(int i = 0; i < count; i++)
    {
        task_s *candidate = optional_tasks[i];
        if(!_tasks_runnable(candidate))
        {
            continue;
        }

        bool due = _tasks_shutdown_lock && (candidate->type_mask & TASK_TYPE_SHUTDOWN)
                 ? true
                 : _tasks_interval_due(candidate, now_us);
        if(!due)
        {
            continue;
        }

        uint64_t d = _tasks_shutdown_lock && (candidate->type_mask & TASK_TYPE_SHUTDOWN)
                   ? (now_us - candidate->last_run_us)
                   : (now_us - (candidate->last_run_us + candidate->optional_interval_us));
        if(d > max_delta)
        {
            max_delta = d;
            t = candidate;
        }
    }

    if(t != NULL)
    {
        _tasks_force_run(t, RUNTIME_MAX_DO_UPDATE);
    }
}

static void _tasks_rapid_run(void)
{
    for(int i = 0; i < _tasks_sm.rapid_count; i++)
    {
        _tasks_force_run(_tasks_sm.rapid[i], RUNTIME_MAX_NO_UPDATE);
    }
}

static bool _tasks_try_run(task_s *task, bool update_max_runtime)
{
    if(!_tasks_runnable(task)) return false;

    uint64_t start_us = sys_hal_now_us();

    if(!(_tasks_shutdown_lock && (task->type_mask & TASK_TYPE_SHUTDOWN)))
    {
        if(!_tasks_interval_due(task, start_us)) return false;
    }

    // Confirm there's enough budget remaining
    // Return if not enough time remaining
    if(start_us + task->max_runtime_us >= _tasks_sm.cycle_deadline_us) return false;

    // Run task with the allocated time
    task->fn(start_us);
    uint64_t end_us = sys_hal_now_us();
    task->last_run_us = start_us;

    // Optionally update max runtime of the task
    if(!update_max_runtime) return true;

    uint64_t runtime_us = end_us - start_us;
    if(runtime_us > task->max_runtime_us)
    {
        task->max_runtime_us = runtime_us;
    }
    return true;
}

static void _tasks_check_deadline(void)
{
    uint64_t now_us = sys_hal_now_us();
    if (now_us >= _tasks_sm.cycle_reset_deadline_us)
    {
        _tasks_reset_sm();
    }
}

static void _task_try_add(task_s *task_list[TASKS_MAX_COUNT], uint8_t *task_count, task_s *task)
{
    if(*task_count >= TASKS_MAX_COUNT) return;

    task_list[*task_count] = task;
    task->max_runtime_us = TASKS_DEFAULT_RUNTIME_US;
    (*task_count)++;
}

void tasks_register(task_s *task)
{
    if (!task || !task->fn)
    {
        return;
    }

    if(task->type_mask & TASK_TYPE_REQUIRED)
    {
        _task_try_add(_tasks_sm.required, &_tasks_sm.required_count, task);
    }

    if(task->type_mask & TASK_TYPE_RECURRING)
    {
        _task_try_add(_tasks_sm.recurring, &_tasks_sm.recurring_count, task);
    }

    if(task->type_mask & TASK_TYPE_OPTIONAL)
    {
        _task_try_add(_tasks_sm.optional, &_tasks_sm.optional_count, task);
    }

    if(task->type_mask & TASK_TYPE_RAPID)
    {
        _task_try_add(_tasks_sm.rapid, &_tasks_sm.rapid_count, task);
    }
}

// Used where it's unsafe to call sys_hal_now_us
void tasks_mark_sent_isr(void)
{
    _tasks_sent_isr_signal = true;
}

// Used where it's not ISR context and safe to call sys_hal_now_us
void tasks_mark_sent(void)
{
    _tasks_reset_sm();
}

void tasks_run(void)
{
    task_s *t;

    _tasks_check_deadline();

    if(_tasks_sent_isr_signal)
    {
        _tasks_reset_sm();
        _tasks_sent_isr_signal = false;
    }

    _tasks_rapid_run();

    switch(_tasks_phase)
    {
        case TASK_PHASE_REQUIRED:
        {
            if(_tasks_sm.required_completed < _tasks_sm.required_count)
            {
                t = _tasks_sm.required[_tasks_sm.required_completed];
                _tasks_force_run(t, RUNTIME_MAX_DO_UPDATE);
                _tasks_sm.required_completed++;
            }
            else
            {
                // All required tasks done. Run the single most-overdue optional
                // task (if any have exceeded their interval) before moving on.
                if (_tasks_sm.optional_count > 0)
                {
                    uint64_t now_us = sys_hal_now_us();
                    _tasks_optional_run(_tasks_sm.optional, _tasks_sm.optional_count, now_us);
                }
                _tasks_phase = TASK_PHASE_RECURRING;
            }
        }
        break;

        case TASK_PHASE_RECURRING:
        {
            if (_tasks_sm.recurring_count == 0) break;

            t = _tasks_sm.recurring[_tasks_sm.recurring_completed];
            if(!_tasks_try_run(t, RUNTIME_MAX_NO_UPDATE)) return;
            _tasks_sm.recurring_completed = (_tasks_sm.recurring_completed + 1) % _tasks_sm.recurring_count;
        }
        break;
    }
}

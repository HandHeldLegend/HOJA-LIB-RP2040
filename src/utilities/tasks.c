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
    uint8_t   optional_completed;

    task_s   *motion; 
    uint8_t   motion_iterations; 
    uint8_t   motion_completed_count;
    uint64_t  motion_period_us;
    uint64_t  motion_last_us;

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

    _tasks_sm.motion_completed_count = 0;
    _tasks_sm.motion_last_us = 0;

    _tasks_sm.required_completed = 0;

    _tasks_phase = TASK_PHASE_REQUIRED;
}

static void _tasks_force_run(task_s *task, bool update_max_runtime)
{
    if(_tasks_shutdown_lock && !(task->type_mask & TASK_TYPE_SHUTDOWN)) return;

    uint64_t start_us = sys_hal_now_us();
    task->fn(start_us);
    uint64_t end_us = sys_hal_now_us();

    if(!update_max_runtime) return;

    uint64_t runtime_us = end_us - start_us;
    if(runtime_us > task->max_runtime_us)
    {
        task->max_runtime_us = runtime_us;
    }
}

static bool _tasks_try_run(task_s *task, bool update_max_runtime)
{
    if(_tasks_shutdown_lock && !(task->type_mask & TASK_TYPE_SHUTDOWN)) return false;

    // Confirm there's enough budget remaining
    uint64_t start_us = sys_hal_now_us();
    // Return if not enough time remaining
    if(start_us + task->max_runtime_us >= _tasks_sm.cycle_deadline_us) return false;

    // Run task with the allocated time
    task->fn(start_us);
    uint64_t end_us = sys_hal_now_us();

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

    if(task->type_mask & TASK_TYPE_MOTION)
    {
        _tasks_sm.motion = task;
        _tasks_sm.motion_iterations = 1;
    }
}

void tasks_set_motion_interval(uint64_t interval_us)
{
    if (!_tasks_sm.motion)
    {
        return;
    }

    if (interval_us <= 2000)
    {
        _tasks_sm.motion_iterations = 1;
        _tasks_sm.motion_period_us  = 1;
    }
    else if (interval_us <= 4000)
    {
        _tasks_sm.motion_iterations = 2;
        _tasks_sm.motion_period_us  = interval_us / 2;
    }
    else
    {
        // Three reads per poll window. The first runs immediately when the cycle
        // resets (see _task_try_motion); later reads are spaced by interval/4 so the
        // third finishes well before an 8000us report (0, ~2ms, ~4ms).
        _tasks_sm.motion_iterations = 3;
        _tasks_sm.motion_period_us  = interval_us / 4;
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

static bool _task_try_motion(task_s *task)
{
    uint64_t now_us = sys_hal_now_us();
    bool due = (_tasks_sm.motion_completed_count == 0 && _tasks_sm.motion_last_us == 0)
            || (now_us > _tasks_sm.motion_last_us + _tasks_sm.motion_period_us);

    if (due)
    {
        task->fn(now_us);
        _tasks_sm.motion_last_us = now_us;
        uint64_t end_us = sys_hal_now_us();

        uint64_t runtime_us = end_us - now_us;
        if(runtime_us > task->max_runtime_us)
        {
            task->max_runtime_us = runtime_us;
        }

        return true;
    }
    return false;
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
                // If we got here, run one optional task and move
                // to our recurring tasks
                if (_tasks_sm.optional_count > 0)
                {
                    t = _tasks_sm.optional[_tasks_sm.optional_completed];
                    _tasks_force_run(t, RUNTIME_MAX_DO_UPDATE);
                    _tasks_sm.optional_completed = (_tasks_sm.optional_completed + 1) % _tasks_sm.optional_count;
                }
                _tasks_phase = TASK_PHASE_RECURRING;
            }
        }
        break;

        case TASK_PHASE_RECURRING:
        {
            if(_tasks_sm.motion && (_tasks_sm.motion_completed_count < _tasks_sm.motion_iterations))
            {
                if(_task_try_motion(_tasks_sm.motion))
                {
                    _tasks_sm.motion_completed_count++;
                    return;
                }
            }

            if (_tasks_sm.recurring_count == 0) break;

            t = _tasks_sm.recurring[_tasks_sm.recurring_completed];
            if(!_tasks_try_run(t, RUNTIME_MAX_NO_UPDATE)) return;
            _tasks_sm.recurring_completed = (_tasks_sm.recurring_completed + 1) % _tasks_sm.recurring_count;
        }
        break;
    }
}

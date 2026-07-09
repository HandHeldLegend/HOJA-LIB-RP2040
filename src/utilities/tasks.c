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

    task_s   *motion[TASKS_MAX_COUNT];
    uint8_t   motion_count;

    uint64_t cycle_last_reset_us;
    uint64_t cycle_deadline_us; 
    uint64_t cycle_reset_deadline_us;

    uint8_t  flag_bit_count; // Shared bit allocator for required + motion guaranteed-run flags
    uint32_t required_flags_mask;
    uint32_t required_flags_done;
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
    _tasks_sm.required_flags_done = 0;

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

        if(!_tasks_interval_due(candidate, now_us))
        {
            continue;
        }

        uint64_t d = now_us - (candidate->last_run_us + candidate->optional_interval_us);
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

// Motion tasks are guaranteed exactly one read at the start of every cycle
// (this read also gates the outbound packet via the required-flag mechanism),
// then take additional reads on a fixed absolute-time grid so that any samples
// taken after the guaranteed one are evenly spaced. On fast cycles (e.g. 1kHz)
// the grid deadline lands past the cycle end, so only the guaranteed read runs;
// on slow cycles (e.g. 8ms) several evenly-spaced reads occur per cycle.
static void _tasks_motion_run(uint64_t now_us)
{
    for(int i = 0; i < _tasks_sm.motion_count; i++)
    {
        task_s *t = _tasks_sm.motion[i];

        // Guaranteed once-per-cycle read. The flag is set unconditionally (even
        // when a shutdown lock makes the task non-runnable) so the packet send
        // is never blocked waiting on a read that can't happen.
        if(!(_tasks_sm.required_flags_done & t->required_done_flag))
        {
            _tasks_force_run(t, RUNTIME_MAX_DO_UPDATE); // anchors last_run_us to the packet-aligned read
            _tasks_sm.required_flags_done |= t->required_done_flag;
            continue;
        }

        if(t->motion_interval_us == 0) continue;

        // Additional evenly-spaced reads on an absolute grid.
        uint64_t next_us = t->last_run_us + t->motion_interval_us;
        if(now_us >= next_us)
        {
            _tasks_force_run(t, RUNTIME_MAX_DO_UPDATE);
            // Advance the grid rather than re-anchoring to "now" so spacing does
            // not drift with call jitter. Resync if we fell more than one
            // interval behind (avoids a burst of catch-up reads).
            t->last_run_us = next_us;
            if(now_us - t->last_run_us >= t->motion_interval_us)
            {
                t->last_run_us = now_us;
            }
        }
    }
}

static bool _tasks_try_run(task_s *task, bool update_max_runtime)
{
    if(!_tasks_runnable(task)) return false;

    uint64_t start_us = sys_hal_now_us();

    if(!_tasks_interval_due(task, start_us)) return false;

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

static uint32_t _tasks_alloc_done_flag(task_s *task)
{
    uint32_t flag = (1u << _tasks_sm.flag_bit_count);
    _tasks_sm.flag_bit_count++;
    task->required_done_flag = flag;
    _tasks_sm.required_flags_mask |= flag;
    return flag;
}

void tasks_register(task_s *task)
{
    if (!task || !task->fn)
    {
        return;
    }

    if(task->type_mask & TASK_TYPE_REQUIRED)
    {
        _tasks_alloc_done_flag(task);
        _task_try_add(_tasks_sm.required, &_tasks_sm.required_count, task);
    }

    if(task->type_mask & TASK_TYPE_MOTION)
    {
        // Motion guaranteed reads participate in the packet-gating flag set so
        // the outbound report waits for at least one fresh sample per cycle.
        _tasks_alloc_done_flag(task);
        _task_try_add(_tasks_sm.motion, &_tasks_sm.motion_count, task);
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

bool tasks_get_required_done(void)
{
    if(_tasks_sm.required_flags_mask == 0)
    {
        return true;
    }

    return (_tasks_sm.required_flags_done & _tasks_sm.required_flags_mask)
        == _tasks_sm.required_flags_mask;
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

    // Motion runs every call: guaranteed read on the first call of a new cycle,
    // then evenly-spaced grid reads as time allows within the cycle.
    if(_tasks_sm.motion_count > 0)
    {
        _tasks_motion_run(sys_hal_now_us());
    }

    if(_tasks_phase == TASK_PHASE_REQUIRED)
    {
        if(_tasks_sm.required_completed < _tasks_sm.required_count)
        {
            t = _tasks_sm.required[_tasks_sm.required_completed];
            _tasks_force_run(t, RUNTIME_MAX_DO_UPDATE);
            _tasks_sm.required_flags_done |= t->required_done_flag;
            _tasks_sm.required_completed++;
        }
        else
        {
            if (_tasks_sm.optional_count > 0)
            {
                uint64_t now_us = sys_hal_now_us();
                _tasks_optional_run(_tasks_sm.optional, _tasks_sm.optional_count, now_us);
            }
            _tasks_phase = TASK_PHASE_RECURRING;
        }
    }

    _tasks_rapid_run();

    if(_tasks_phase == TASK_PHASE_RECURRING)
    {
        if (_tasks_sm.recurring_count == 0) return;

        t = _tasks_sm.recurring[_tasks_sm.recurring_completed];
        if(!_tasks_try_run(t, RUNTIME_MAX_NO_UPDATE)) return;
        _tasks_sm.recurring_completed = (_tasks_sm.recurring_completed + 1) % _tasks_sm.recurring_count;
    }
}

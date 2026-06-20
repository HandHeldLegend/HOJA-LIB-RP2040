#include "input/idle_manager.h"

#include "devices/battery.h"
#include "devices/rgb.h"

#include "hoja.h"

#define IDLE_ACTIVATION_TIME_SECONDS 5 * 60 // 5 mins
#define IDLE_ACTIVATION_TIME_US (IDLE_ACTIVATION_TIME_SECONDS * 1000 * 1000)

volatile bool _reset_state = false;
bool _idle_active = false;

// Call this for any function that should help keep
// the idle state from activating

void idle_manager_heartbeat()
{
    _reset_state = true;
}

void idle_manager_task(uint64_t timestamp)
{
    static uint64_t last_activity_us = 0;

    if (_reset_state)
    {
        last_activity_us = timestamp;

        if (_idle_active)
        {
            rgb_set_idle(false);
            _idle_active = false;
        }

        _reset_state = false;
        return;
    }

    if (last_activity_us == 0)
    {
        last_activity_us = timestamp;
        return;
    }

    if (!_idle_active && (timestamp - last_activity_us >= IDLE_ACTIVATION_TIME_US))
    {
        _idle_active = true;
        rgb_set_idle(true);
    }
}

#include "input/idle_manager.h"

#include "utilities/interval.h"
#include "devices/battery.h"
#include "devices/rgb.h"

#include "hoja.h"

#define IDLE_ACTIVATION_TIME_SECONDS 5 // 10 minutes
#define IDLE_ACTIVATION_TIME_US (IDLE_ACTIVATION_TIME_SECONDS * 1000 * 1000)

volatile bool _reset_state = false;
bool _idle_active = false;

// Call this for any function that should help keep
// the idle state from activating

void idle_manager_heartbeat()
{
    _reset_state = true;
}

void idle_manager_task(uint32_t timestamp)
{
    static interval_s interval = {0};

    if(!_idle_active)
    {
        if(interval_resettable_run(timestamp, IDLE_ACTIVATION_TIME_US, _reset_state, &interval))
        {
            _idle_active = true;

            hoja_set_notification_status(COLOR_BLUE);

            rgb_set_idle(true);
        }
    }
    else if(_reset_state && _idle_active)
    {
        hoja_set_notification_status(COLOR_BLACK);
        rgb_set_idle(false);
        _idle_active = false;
    }

    if(_reset_state) _reset_state = false;
}

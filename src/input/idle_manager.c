#include "input/idle_manager.h"

#include "utilities/interval.h"
#include "devices/battery.h"
#include "devices/rgb.h"

#include "hoja.h"

#define IDLE_ACTIVATION_TIME_SECONDS 10//5 * 60 // 5 mins
#define IDLE_ACTIVATION_TIME_US (IDLE_ACTIVATION_TIME_SECONDS * 1000 * 1000)

volatile bool _reset_state = false;
volatile bool _state_is_reset = true;
bool _idle_active = false;

// Call this for any function that should help keep
// the idle state from activating

void idle_manager_heartbeat()
{
    _reset_state = true;
    _state_is_reset = false;
}

void idle_manager_task(uint64_t timestamp)
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
        else if(_reset_state)
        {
            _state_is_reset = true;
        }

        if(_reset_state && _state_is_reset)
            _reset_state = false;
    }
    else if(_reset_state && _idle_active)
    {
        hoja_set_notification_status(COLOR_BLACK);
        rgb_set_idle(false);
        _idle_active = false;
    }
}

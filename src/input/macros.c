#include "macros.h"
#include "interval.h"

bool _safe_mode_on = false;

typedef enum
{
    MACRO_IDLE = 0,
    MACRO_HELD = 1,
} macro_press_t;

macro_press_t _safe_mode = MACRO_IDLE;
bool _safe_mode_state = false;
macro_press_t _ship_mode = MACRO_IDLE;

macro_press_t _sync_mode = MACRO_IDLE;

void macro_handler_task(uint32_t timestamp, button_data_s *in)
{

    // Tracker for Safe Mode
    if (interval_run(timestamp, 16000))
    {

        if (in->button_safemode && !_safe_mode)
        {
            _safe_mode = MACRO_HELD;
        }
        else if (!in->button_safemode && (_safe_mode == MACRO_HELD))
        {
            _safe_mode_state = !_safe_mode_state;
            _safe_mode = MACRO_IDLE;

            if (_safe_mode_state)
            {
                rgb_set_group(RGB_GROUP_PLUS, 0);
                rgb_set_group(RGB_GROUP_HOME, 0);
                rgb_set_group(RGB_GROUP_MINUS, 0);
                rgb_set_group(RGB_GROUP_CAPTURE, 0);
            }
            else
            {
                rgb_preset_reload();
            }

            rgb_set_dirty();
        }
    }

    // Will fire when Shipping button is held for 3 seconds
    // otherwise the state resets
    if(interval_resettable_run(timestamp, 3000000, !in->button_shipping))
    {
        // Sleeps the controller
        util_battery_enable_ship_mode();
    }
}

bool macro_safe_mode_check()
{
    return _safe_mode_on;
}
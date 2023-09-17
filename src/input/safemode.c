#include "safemode.h"
#include "interval.h"

bool _safe_mode_on = false;

void safe_mode_task(uint32_t timestamp, button_data_s *in)
{
    static bool _toggling = false;

    if (interval_run(timestamp, 16000))
    {

        if (in->button_safemode && !_toggling)
        {
            _toggling = 1;
        }
        else if (!in->button_safemode && _toggling > 0)
        {
            _safe_mode_on = !_safe_mode_on;
            _toggling = 0;

            if (_safe_mode_on)
            {
                rgb_set_group(RGB_GROUP_PLUS, 0);
                rgb_set_group(RGB_GROUP_HOME, 0);
                rgb_set_group(RGB_GROUP_MINUS, 0);
                rgb_set_group(RGB_GROUP_CAPTURE, 0);
            }
            else
            {
                rgb_load_preset();
            }

            rgb_set_dirty();
        }
    }
}

bool safe_mode_check()
{
    return _safe_mode_on;
}
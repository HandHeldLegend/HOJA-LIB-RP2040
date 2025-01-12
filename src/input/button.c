#include "input/button.h"
#include "input/remap.h"
#include "input/trigger.h"

#include <string.h>

#include "utilities/interval.h"
#include "utilities/pcm.h"
#include "utilities/settings.h"

#include "hal/mutex_hal.h"
#include "hal/sys_hal.h"

#include "hoja.h"

MUTEX_HAL_INIT(_button_mutex);

button_data_s _safe_raw_button_data;

button_data_s _boot_button_data;
button_data_s _raw_button_data;
volatile bool _first_run = false;

void _button_blocking_enter()
{
    MUTEX_HAL_ENTER_BLOCKING(&_button_mutex);
}

uint32_t _button_mutex_owner = 0;
bool _button_try_enter()
{
    if(MUTEX_HAL_ENTER_TRY(&_button_mutex, &_button_mutex_owner))
    {
        return true;
    }
    return false;
}

void _button_exit()
{
    MUTEX_HAL_EXIT(&_button_mutex);
}

// Access button data safely from any core :)
void button_access_blocking(button_data_s *out, button_access_t type)
{
    _button_blocking_enter();
    switch(type)
    {
        case BUTTON_ACCESS_RAW_DATA:
        memcpy(out, &_safe_raw_button_data, sizeof(button_data_s));
        break;

        case BUTTON_ACCESS_BOOT_DATA:
        memcpy(out, &_boot_button_data, sizeof(button_data_s));
        break;
    }
    _button_exit();
}

// Access button data (not guaranteed latest)
void button_access_safe(button_data_s *out, button_access_t type)
{
    if(_button_try_enter())
    {
        switch(type)
        {
            case BUTTON_ACCESS_RAW_DATA:
            memcpy(out, &_safe_raw_button_data, sizeof(button_data_s));
            break;

            case BUTTON_ACCESS_BOOT_DATA:
            memcpy(out, &_boot_button_data, sizeof(button_data_s));
            break;
        }
        _button_exit();
    }
}

bool button_init()
{
    // Init hardware buttons
    if(!cb_hoja_buttons_init())
    {
        // Reboot
        sys_hal_reboot();
    }

    // Store boot button state
    _button_blocking_enter();
    cb_hoja_read_buttons(&_boot_button_data);
    _button_exit();

    // Init remap
    remap_init();

    return true;
}

#define BUTTON_READ_RATE_US 500 // 500us read rate. Double the 1khz maximum USB input rate.

#include "utilities/erm_simulator.h"
#include "utilities/pcm_samples.h"

void button_task(uint32_t timestamp)
{
    static interval_s interval = {0};

    if(interval_run(timestamp, BUTTON_READ_RATE_US, &interval))
    {
        // Read raw buttons
        cb_hoja_read_buttons(&_raw_button_data);

        // Could break this out later...
        if(haptic_config->haptic_triggers==1)
        {
            static bool pressed_zl = false;
            if(_raw_button_data.trigger_zl && !pressed_zl)
            {
                pcm_play_sample(hapticPattern, sizeof(hapticPattern));
                pressed_zl = true;
            }
            else if (!_raw_button_data.trigger_zl && pressed_zl)
            {
                pcm_play_sample(offPattern, sizeof(offPattern));
                pressed_zl = false;
            }

            static bool pressed_zr = false;
            if(_raw_button_data.trigger_zr && !pressed_zr)
            {
                pcm_play_sample(hapticPattern, sizeof(hapticPattern));
                pressed_zr = true;
            }
            else if (!_raw_button_data.trigger_zr && pressed_zr)
            {
                pcm_play_sample(offPattern, sizeof(offPattern));
                pressed_zr = false;
            }
        }

        if(_button_try_enter())
        {
            memcpy(&_safe_raw_button_data, &_raw_button_data, sizeof(button_data_s));
            _button_exit();
        }

    }
}
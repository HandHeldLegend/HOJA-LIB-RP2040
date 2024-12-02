#include "input/button.h"

#include <string.h>

#include "utilities/interval.h"

#include "hal/mutex_hal.h"
#include "input/remap.h"
#include "hoja.h"

MUTEX_HAL_INIT(_button_mutex);

button_data_s _boot_button_data;
button_data_s _raw_button_data;
button_data_s _remapped_button_data;

void _button_blocking_enter()
{
    MUTEX_HAL_ENTER_BLOCKING(&_button_mutex);
}

bool _button_try_enter()
{
    if(MUTEX_HAL_ENTER_TIMEOUT_US(&_button_mutex, 10))
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
        memcpy(out, &_raw_button_data, sizeof(button_data_s));
        break;

        case BUTTON_ACCESS_REMAPPED_DATA:
        memcpy(out, &_remapped_button_data, sizeof(button_data_s));
        break;

        case BUTTON_ACCESS_BOOT_DATA:
        memcpy(out, &_boot_button_data, sizeof(button_data_s));
        break;
    }
    _button_exit();
}

// Access button data with fallback
bool button_access_try(button_data_s *out, button_access_t type)
{
    if(_button_try_enter())
    {
        switch(type)
        {
            case BUTTON_ACCESS_RAW_DATA:
            memcpy(out, &_raw_button_data, sizeof(button_data_s));
            break;

            case BUTTON_ACCESS_REMAPPED_DATA:
            memcpy(out, &_remapped_button_data, sizeof(button_data_s));
            break;

            case BUTTON_ACCESS_BOOT_DATA:
            memcpy(out, &_boot_button_data, sizeof(button_data_s));
            break;
        }
        _button_exit();
        return true;
    }
    return false;
}

bool button_init()
{
    // Store boot button state
    _button_blocking_enter();
    cb_hoja_read_buttons(&_boot_button_data);

    if(_boot_button_data.button_safemode && _boot_button_data.trigger_l)
    {
        //reset_usb_boot(0, 0);
    }
    
    _button_exit();

    return true;
}

#define BUTTON_READ_RATE_US 500 // 500us read rate. Double the 1khz maximum USB input rate.

void button_task(uint32_t timestamp)
{
    static interval_s interval = {0};

    if(interval_run(timestamp, BUTTON_READ_RATE_US, &interval))
    {
        _button_blocking_enter();
        // Read raw buttons
        cb_hoja_read_buttons(&_raw_button_data);
        // Process button remaps
        //--
        _button_exit();
    }
}
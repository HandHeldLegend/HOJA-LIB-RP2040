#include "input/button.h"

#include <string.h>

#include "hal/mutex_hal.h"
#include "input/remap.h"
#include "hoja.h"

MUTEX_HAL_INIT(_button_mutex);

button_data_s _boot_button_data;
button_data_s _raw_button_data;
button_data_s _remapped_button_data;

void _button_safe_enter()
{
    MUTEX_HAL_ENTER_BLOCKING(&_button_mutex);
}

void _button_exit()
{
    MUTEX_HAL_EXIT(&_button_mutex);
}

// Access button data safely from any core :)
void button_access(button_data_s *out, button_access_t type)
{
    _button_safe_enter();
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

bool button_init()
{
    // Store boot button state
    _button_safe_enter();
    cb_hoja_read_buttons(&_boot_button_data);

    if(_boot_button_data.button_safemode && _boot_button_data.trigger_l)
    {
        reset_usb_boot(0, 0);
    }
    
    _button_exit();
}

void button_task(uint32_t timestamp)
{
    _button_safe_enter();
    // Read raw buttons
    cb_hoja_read_buttons(&_raw_button_data);
    // Process button remaps
    //--
    _button_exit();
}
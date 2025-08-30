#include "input/button.h"
#include "input/remap.h"
#include "input/trigger.h"
#include "input/idle_manager.h"

#include <string.h>

#include "utilities/interval.h"
#include "utilities/settings.h"

#include "hal/mutex_hal.h"
#include "hal/sys_hal.h"

#include "hoja.h"

button_data_s _safe_raw_button_data;

button_data_s _boot_button_data;
button_data_s _raw_button_data;
volatile bool _first_run = false;
bool _tourney_lockout = false;

void button_tourney_lockout_enable(bool enable)
{
    _tourney_lockout = enable;
}

bool _are_buttons_different(button_data_s *current)
{
    static button_data_s last_buttons = {0};
    bool return_val = false;

    if(current->buttons_all != last_buttons.buttons_all) return_val = true;
    if(current->buttons_system != last_buttons.buttons_system) return_val = true;

    last_buttons.buttons_all = current->buttons_all;
    last_buttons.buttons_system = current->buttons_system;

    return return_val;
}

// Access button data
void button_access_safe(button_data_s *out, button_access_t type)
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
    cb_hoja_read_buttons(&_boot_button_data);

    // Init remap
    remap_init();

    return true;
}

#define BUTTON_READ_RATE_US 250 // read rate in micros. Double the 1khz maximum USB input rate.

void button_task(uint64_t timestamp)
{
    static interval_s interval = {0};

    // Read raw buttons
    cb_hoja_read_buttons(&_raw_button_data);

    if(_tourney_lockout)
    {
        _raw_button_data.dpad_down = 0;
        _raw_button_data.dpad_left = 0;
        _raw_button_data.dpad_right = 0;
        _raw_button_data.dpad_up = 0;

        _raw_button_data.button_home = 0;
        _raw_button_data.button_capture = 0;
        _raw_button_data.button_plus = 0;
        _raw_button_data.button_minus = 0;
    }

    memcpy(&_safe_raw_button_data, &_raw_button_data, sizeof(button_data_s));

    if(_are_buttons_different(&_raw_button_data)) 
    {
        idle_manager_heartbeat();
    }
}
#include "input/button.h"
#include "input/trigger.h"
#include "input/idle_manager.h"
#include "input/mapper.h"

#include <string.h>

#include "utilities/interval.h"
#include "utilities/settings.h"
#include "utilities/crosscore_snapshot.h"

#include "hal/mutex_hal.h"
#include "hal/sys_hal.h"

#include "hoja.h"

SNAPSHOT_TYPE(button, button_data_s);
snapshot_button_t _button_snap_raw;
snapshot_button_t _button_snap_boot;

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

    if (last_buttons.buttons != current->buttons)
    {
        last_buttons.buttons = current->buttons;
        return true;
    }

    return false;
}

// Access button data
void button_access_safe(button_data_s *out, button_access_t type)
{
    switch(type)
    {
        case BUTTON_ACCESS_RAW_DATA:
        snapshot_button_read(&_button_snap_raw, out);
        break;

        case BUTTON_ACCESS_BOOT_DATA:
        snapshot_button_read(&_button_snap_boot, out);
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
    snapshot_button_write(&_button_snap_boot, &_boot_button_data);

    // Init remap
    mapper_init();

    return true;
}

#define BUTTON_READ_RATE_US 250 // read rate in micros
#define TOURNEY_BUTTON_MASK ( (1<<MAPPER_CODE_UP) | (1<<MAPPER_CODE_DOWN) | (1<<MAPPER_CODE_LEFT) | (1<<MAPPER_CODE_RIGHT) | \
                            (1<<MAPPER_CODE_START) | (1<<MAPPER_CODE_SELECT) | (1<<MAPPER_CODE_HOME) | (1<<MAPPER_CODE_CAPTURE) )

void button_task(uint64_t timestamp)
{
    static interval_s interval = {0};

    // Read raw buttons
    cb_hoja_read_buttons(&_raw_button_data);

    if(_tourney_lockout)
    {
        _raw_button_data.buttons = ~TOURNEY_BUTTON_MASK & _raw_button_data.buttons;
    }

    snapshot_button_write(&_button_snap_raw, &_raw_button_data);

    if(_are_buttons_different(&_raw_button_data)) 
    {
        idle_manager_heartbeat();
    }
}
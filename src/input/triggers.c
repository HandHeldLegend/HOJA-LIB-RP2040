#include "input/triggers.h"
#include <stddef.h>

#include "settings_shared_types.h"

float _lt_scaler = 1;
float _rt_scaler = 1;
bool _trigger_calibration = false;
#define TRIGGER_DEADZONE 150

void triggers_config_cmd(trigger_cmd_t cmd, const uint8_t *data, setting_callback_t cb)
{
    const uint8_t cb_dat[3] = {CFG_BLOCK_TRIGGER, cmd, 1};

    switch(cmd)
    {
        default:
        break;

        case TRIGGER_CMD_SET_BOUNDS:
        break;

        case TRIGGER_CMD_SET_DEADZONE:
        break;

        case TRIGGER_CMD_SET_ENABLE:
        break;
    }

    if(cb!=NULL)
    {
        cb(cb_dat, 3);
    }
}

bool triggers_init()
{

}

void triggers_process(button_data_s *in, button_data_s *out)
{
    
}

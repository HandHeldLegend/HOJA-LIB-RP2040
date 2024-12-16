#include "input/triggers.h"
#include <stddef.h>

#include "settings_shared_types.h"

float _lt_scaler = 1;
float _rt_scaler = 1;
bool _trigger_calibration = false;
#define TRIGGER_DEADZONE 150

void triggers_config_cmd(trigger_cmd_t cmd, command_confirm_t cb)
{
    switch(cmd)
    {
        default:
        break;
    }
}

bool triggers_init()
{

}

void triggers_process(button_data_s *in, button_data_s *out)
{
    
}

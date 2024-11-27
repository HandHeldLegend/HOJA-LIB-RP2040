#include "wired/wired.h"
#include "utilities/callback.h"
#include <stddef.h>

time_callback_t _wired_task_cb = NULL;

void wired_mode_task(uint32_t timestamp)
{
    if(_wired_task_cb != NULL)
    {
        _wired_task_cb(timestamp);
    }
}

bool wired_mode_start(gamepad_mode_t mode)
{
    switch(mode)
    {
        case GAMEPAD_MODE_SNES:
        break;

        case GAMEPAD_MODE_N64:
        break;

        default:
        case GAMEPAD_MODE_GAMECUBE:
        break;
    }
}
#include "wired/wired.h"
#include "utilities/callback.h"
#include <stddef.h>

#include "hoja_bsp.h"
#include "board_config.h"

#include "wired/nesbus.h"
#include "wired/n64.h"
#include "wired/gamecube.h"

time_callback_t _wired_task_cb = NULL;

void wired_mode_task(uint32_t timestamp)
{
    if(_wired_task_cb)
        _wired_task_cb(timestamp);
}

bool wired_mode_start(gamepad_mode_t mode)
{
    switch(mode)
    {   
        #if defined(HOJA_NESBUS_DRIVER) && (HOJA_NESBUS_DRIVER > 0)
        case GAMEPAD_MODE_SNES:
            _wired_task_cb = nesbus_wired_task;
            nesbus_wired_start();
            return true;
        break;
        #endif

        #if defined(HOJA_JOYBUS_N64_DRIVER) && (HOJA_JOYBUS_N64_DRIVER > 0)
        case GAMEPAD_MODE_N64:
            _wired_task_cb = n64_wired_task;
            n64_wired_start();
            return true;
        break;
        #endif

        default:
        #if defined(HOJA_JOYBUS_GC_DRIVER) && (HOJA_JOYBUS_GC_DRIVER > 0)
        case GAMEPAD_MODE_GAMECUBE:
            _wired_task_cb = gamecube_wired_task;
            gamecube_wired_start();
            return true;
        break;
        #else 
            return false;
            break;
        #endif
    }

    return false;
}
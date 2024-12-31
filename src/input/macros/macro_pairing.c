#include "input/macros/macro_pairing.h"
#include "hal/sys_hal.h"
#include "devices/battery.h"
#include "utilities/interval.h"
#include "utilities/boot.h"
#include "hoja_shared_types.h"
#include "hoja.h"

#define PAIRING_HOLD_TIME 3 // Seconds
#define PAIRING_MACRO_INTERVAL_US 3000
#define PAIRING_HOLD_LOOPS ( (PAIRING_HOLD_TIME*1000*1000) / PAIRING_MACRO_INTERVAL_US )

void macro_pairing(uint32_t timestamp, button_data_s *buttons)
{
    static interval_s interval = {0};
    static bool holding = false;
    static uint32_t iterations = 0;
    static bool lockout = false;

    if(lockout)
    {
        return;
    }

    if(interval_run(timestamp, PAIRING_MACRO_INTERVAL_US, &interval))
    {
        if(!holding && buttons->button_sync)
        {
            holding = true;
        }
        else if(holding && !buttons->button_sync)
        {
            holding = false;
            iterations = 0;
        }

        if(holding)
        {
            iterations++;

            // Trigger macro
            if(iterations>=PAIRING_HOLD_LOOPS)
            {
                // Get our current input mode
                gamepad_mode_t mode = hoja_gamepad_mode_get();
                bool pair = false;

                // If it's a valid mode
                switch(mode)
                {
                    case GAMEPAD_MODE_SWPRO:
                        // Enable pairing
                        pair = true;
                    break;
                }

                if(pair)
                {
                    lockout = true;

                    boot_memory_s mem = {
                        .gamepad_method = GAMEPAD_METHOD_BLUETOOTH,
                        .gamepad_mode   = mode,
                        .gamepad_pair   = true
                        };

                    boot_set_memory(&mem);
                    
                    hoja_deinit(sys_hal_reboot);
                }
            }
        }
    }
}
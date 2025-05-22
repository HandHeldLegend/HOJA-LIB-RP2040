#include "input/macros/macro_tourney.h"
#include "input/button.h"
#include "hal/sys_hal.h"
#include "devices/battery.h"
#include "devices/rgb.h"
#include "utilities/interval.h"
#include "utilities/boot.h"
#include "hoja_shared_types.h"
#include "hoja.h"

#define PAIRING_TOURNEY_INTERVAL_US 3000

void macro_tourney(uint64_t timestamp, button_data_s *buttons)
{
    static interval_s interval = {0};
    static bool holding = false;
    static uint32_t iterations = 0;
    static bool lockout = false;
    static bool tourney_mode_enabled = false;

    if(interval_run(timestamp, PAIRING_TOURNEY_INTERVAL_US, &interval))
    {
        if(!holding && buttons->button_sync)
        {
            holding = true;
        }
        else if(holding && !buttons->button_sync)
        {
            holding = false;
            tourney_mode_enabled = !tourney_mode_enabled;
            button_tourney_lockout_enable(tourney_mode_enabled);
            hoja_set_ss_notif(tourney_mode_enabled ? COLOR_GREEN : COLOR_RED);
        }
    }
}
#include "input/macros/macro_shutdown.h"
#include "devices/battery.h"
#include "utilities/interval.h"
#include "hoja.h"

#define SHUTDOWN_HOLD_TIME 3 // Seconds
#define SHUTDOWN_MACRO_INTERVAL_US 3000
#define SHUTDOWN_HOLD_LOOPS ( (SHUTDOWN_HOLD_TIME*1000*1000) / SHUTDOWN_MACRO_INTERVAL_US )

volatile bool _shutdown_ready = false;

void _shutdown_finalize()
{
    _shutdown_ready = true;
}

void macro_shutdown(uint32_t timestamp, button_data_s *buttons)
{
    static interval_s interval = {0};
    static bool holding = false;
    static uint32_t iterations = 0;
    static bool lockout = false;

    if(lockout)
    {
        // Only shut down when we release button
        if(_shutdown_ready && !buttons->button_shipping)
        {
            _shutdown_ready = false;
            battery_set_ship_mode();
        }
        return;
    }

    if(interval_run(timestamp, SHUTDOWN_MACRO_INTERVAL_US, &interval))
    {
        if(!holding && buttons->button_shipping)
        {
            holding = true;
        }
        else if(holding && !buttons->button_shipping)
        {
            holding = false;
            iterations = 0;
        }

        if(holding)
        {
            iterations++;
            if(iterations>=SHUTDOWN_HOLD_LOOPS)
            {
                lockout = true;
                // Deinit, then shut down
                hoja_deinit(_shutdown_finalize);
            }
        }
    }
}
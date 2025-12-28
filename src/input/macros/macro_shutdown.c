#include "input/macros/macro_shutdown.h"
#include "devices/battery.h"
#include "utilities/interval.h"
#include "hoja.h"

#define SHUTDOWN_HOLD_TIME 3 // Seconds
#define SHUTDOWN_MACRO_INTERVAL_US 3000
#define SHUTDOWN_HOLD_LOOPS ( (SHUTDOWN_HOLD_TIME*1000*1000) / SHUTDOWN_MACRO_INTERVAL_US )

bool _shutdown_ready = false;

void _shutdown_finalize()
{
    _shutdown_ready = true;
}

void macro_shutdown(uint64_t timestamp, mapper_input_s *input)
{
    static interval_s interval = {0};
    static bool holding = false;
    static uint32_t iterations = 0;
    static bool lockout = false;
    static bool boot_wait = true;
    static bool interval_initialized = false;

    // Wait for shipping button to be released after boot
    if(boot_wait)
    {
        if(!input->button_shipping)
        {
            boot_wait = false;
        }
        return;
    }

    // Initialize interval on first run after boot
    if(!interval_initialized)
    {
        interval_resettable_run(timestamp, SHUTDOWN_MACRO_INTERVAL_US, true, &interval);
        interval_initialized = true;
        return;
    }

    if(lockout)
    {
        // Only shut down when we release button
        if(_shutdown_ready && !input->button_shipping)
        {
            _shutdown_ready = false;
            hoja_shutdown();
        }
        return;
    }

    if(interval_run(timestamp, SHUTDOWN_MACRO_INTERVAL_US, &interval))
    {
        if(!holding && input->button_shipping)
        {
            holding = true;
            iterations = 0; // Reset iterations when starting to hold
        }
        else if(holding && !input->button_shipping)
        {
            holding = false;
            iterations = 0;
        }

        if(holding)
        {
            iterations++;
            if(iterations >= SHUTDOWN_HOLD_LOOPS)
            {
                lockout = true;
                // Deinit, then shut down
                hoja_deinit(_shutdown_finalize);
            }
        }
    }
}
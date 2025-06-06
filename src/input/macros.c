#include "input/button.h"
#include "input/macros.h"
#include "utilities/interval.h"

#include "input/macros/macro_shutdown.h"
#include "input/macros/macro_pairing.h"
#include "input/macros/macro_tourney.h"

#include "devices/battery.h"

#include "board_config.h"

#if defined(HOJA_HAPTICS_DEBUG) && (HOJA_HAPTICS_DEBUG==1)
#include "input/macros/macro_pcmdebug.h"
#endif

void macros_task(uint64_t timestamp)
{
    static button_data_s buttons = {0};
    static interval_s interval = {0};
    static bool first_run = false;

    if(interval_run(timestamp, 1000, &interval))
    {
        button_access_safe(&buttons, BUTTON_ACCESS_RAW_DATA);
        first_run = true;
    }

    // Only run macros on successful button read
    if(!first_run) return;

    #if defined(HOJA_BATTERY_DRIVER) && (HOJA_BATTERY_DRIVER>0)
    macro_shutdown(timestamp, &buttons);
    #endif

    #if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER>0)
    macro_pairing(timestamp, &buttons);
    #endif

    #if defined(HOJA_HAPTICS_DEBUG) && (HOJA_HAPTICS_DEBUG==1)
    macro_pcmdebug(timestamp, &buttons);
    #endif

    #if defined(HOJA_DISABLE_TOURNEY_MACRO) && (HOJA_DISABLE_TOURNEY_MACRO==1)
    #else
    macro_tourney(timestamp, &buttons);
    #endif
}
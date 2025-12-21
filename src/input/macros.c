#include "input/macros.h"
#include "utilities/interval.h"
#include "input/mapper.h"

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
    static mapper_input_s input = {0};
    static interval_s interval = {0};
    static bool first_run = false;

    if(interval_run(timestamp, 1000, &interval))
    {
        input = mapper_get_translated_input();
        first_run = true;
    }

    // Only run macros on successful button read
    if(!first_run) return;

    #if defined(HOJA_BATTERY_DRIVER) && (HOJA_BATTERY_DRIVER>0)
    macro_shutdown(timestamp, &input);
    #endif

    #if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER>0)
    macro_pairing(timestamp, &input);
    #endif

    #if defined(HOJA_HAPTICS_DEBUG) && (HOJA_HAPTICS_DEBUG==1)
    macro_pcmdebug(timestamp, &input);
    #endif

    #if defined(HOJA_DISABLE_TOURNEY_MACRO) && (HOJA_DISABLE_TOURNEY_MACRO==1)
    #else
    macro_tourney(timestamp, &input);
    #endif
}
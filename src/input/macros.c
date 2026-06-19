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

    // Always run; battery_set_ship_mode() safely no-ops when no PMIC is present.
    macro_shutdown(timestamp, &input);

    #if defined(HOJA_TRANSPORT_BT_DRIVER) && (HOJA_TRANSPORT_BT_DRIVER > 0)
    macro_pairing(timestamp, &input);
    #endif

    #if defined(HOJA_HAPTICS_DEBUG) && (HOJA_HAPTICS_DEBUG==1)
    macro_pcmdebug(timestamp, &input);
    #endif

    // Disabled when hoja_config_s.tourney_macro_code is INPUT_CODE_UNUSED.
    macro_tourney(timestamp, &input);
}
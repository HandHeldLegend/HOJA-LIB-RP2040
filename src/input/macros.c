#include "input/macros.h"
#include "input/hover.h"

#include "input/macros/macro_shutdown.h"
#include "input/macros/macro_pairing.h"
#include "input/macros/macro_tourney.h"

#include "board_config.h"

#if defined(HOJA_HAPTICS_DEBUG) && (HOJA_HAPTICS_DEBUG==1)
#include "input/macros/macro_pcmdebug.h"
#endif

void macros_task(uint64_t timestamp)
{
    mapper_input_s input = {0};
    hover_access_safe(&input);

    // Disabled when hoja_config_s.shipping_macro_code[0] is INPUT_CODE_UNUSED.
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

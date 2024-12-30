#include "input/button.h"
#include "input/macros.h"

#include "input/macros/macro_shutdown.h"
#include "devices/battery.h"

#include "board_config.h"

void macros_task(uint32_t timestamp)
{
    static button_data_s buttons = {0};

    button_access_try(&buttons, BUTTON_ACCESS_RAW_DATA);

    #if defined(HOJA_BATTERY_DRIVER) && (HOJA_BATTERY_DRIVER>0)
    macro_shutdown(timestamp, &buttons);
    #endif
}
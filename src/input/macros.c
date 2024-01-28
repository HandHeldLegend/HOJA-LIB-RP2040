#include "macros.h"
#include "interval.h"

typedef enum
{
    MACRO_IDLE = 0,
    MACRO_HELD = 1,
} macro_press_t;

macro_press_t _safe_mode = MACRO_IDLE;
bool _safe_mode_state = false;
macro_press_t _ship_mode = MACRO_IDLE;

macro_press_t _sync_mode = MACRO_IDLE;

void macro_handler_task(uint32_t timestamp, button_data_s *in)
{
    static interval_s interval = {0};
    static interval_s interval_2 = {0};

    // Tracker for Safe Mode
    if (interval_run(timestamp, 16000, &interval))
    {

        if (in->button_safemode && !_safe_mode)
        {
            _safe_mode = MACRO_HELD;
        }
        else if (!in->button_safemode && (_safe_mode == MACRO_HELD))
        {
            _safe_mode_state = !_safe_mode_state;
            _safe_mode = MACRO_IDLE;

            if (_safe_mode_state)
            {
                rgb_indicate(COLOR_GREEN.color, 0);
            }
            else
            {
                rgb_indicate(COLOR_RED.color, 0);
            }
        }

    }

    // Will fire when Shipping button is held for 3 seconds
    // otherwise the state resets
    if(interval_resettable_run(timestamp, 3000000, !in->button_shipping, &interval_2))
    {
        input_method_t i = hoja_get_input_method();
        if( (i == INPUT_METHOD_USB))
        {
            #ifdef HOJA_CAPABILITY_BLUETOOTH
                #if (HOJA_CAPABILITY_BLUETOOTH == 1)
                    hoja_reboot_memory_u reboot_memory = {
                        .gamepad_mode = hoja_comms_current_mode(), 
                        .reboot_reason = ADAPTER_REBOOT_REASON_BTSTART,
                        };

                    reboot_with_memory(reboot_memory.value);
                #endif
            #endif

        }
        else if (i == INPUT_METHOD_BLUETOOTH)
        {
            #ifdef HOJA_CAPABILITY_BLUETOOTH
                #if (HOJA_CAPABILITY_BLUETOOTH == 1)
                    hoja_shutdown();
                #endif
            #endif
        }
        else if (i == INPUT_METHOD_WIRED)
        {
            #if(HOJA_CAPABILITY_BATTERY)
            if(!util_wire_connected())
            {
                hoja_shutdown();
            }   
            #endif
        }
        
    }
}

bool macro_safe_mode_check()
{
    return _safe_mode_state;
}
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

    // Tracker for Safe Mode
    if (interval_run(timestamp, 16000))
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
                rgb_set_group(RGB_GROUP_PLUS, 0, false);
                rgb_set_group(RGB_GROUP_HOME, 0, false);
                rgb_set_group(RGB_GROUP_MINUS, 0, false);
                rgb_set_group(RGB_GROUP_CAPTURE, 0, false);
            }
            else
            {
                rgb_init(global_loaded_settings.rgb_mode, -1);
            }
        }

    }

    // Will fire when Shipping button is held for 3 seconds
    // otherwise the state resets
    if(interval_resettable_run(timestamp, 3000000, !in->button_shipping))
    {
        input_method_t i = hoja_get_input_method();
        if( (i == INPUT_METHOD_USB) || (i == INPUT_METHOD_WIRED) )
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
        else
        {
            #ifdef HOJA_CAPABILITY_BLUETOOTH
                #if (HOJA_CAPABILITY_BLUETOOTH == 1)

                    // Check if we are connected 
                    if (util_wire_connected())
                    // Do nothing
                    {           }
                    // Sleeps the controller 
                    else {hoja_shutdown();}
                    
                #endif
            #endif 
        }
        
    }
}

bool macro_safe_mode_check()
{
    return _safe_mode_state;
}
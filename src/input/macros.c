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

callback_t macro_deinit_cb;
volatile bool macro_deinit_done = false;
void macro_deinit_complete()
{
    macro_deinit_done = true;
}

void macro_handler_task(uint32_t timestamp, button_data_s *in)
{
    static interval_s interval = {0};
    static interval_s interval_2 = {0};
    static interval_s interval_3 = {0};
    static interval_s interval_4 = {0};

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
                rgb_indicate(COLOR_GREEN.color, 24);
            }
            else
            {
                rgb_indicate(COLOR_RED.color, 24);
            }
        }

    }

    // Prevent shipping button from resetting before you do anything!
    static bool shipping_lockout = true;
    static bool shutdown_lockout = true;
    if(shipping_lockout && interval_resettable_run(timestamp, 100000, in->button_shipping, &interval_3))
    {
        shipping_lockout = false;
    }
    // Will fire when Shipping button is held for 3 seconds
    // otherwise the state resets

    if(!shipping_lockout)
    {
        if(shutdown_lockout)
        {
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
                        macro_deinit_cb = macro_deinit_complete;
                        hoja_deinit(macro_deinit_cb);
                        #endif
                    #endif
                }
                else if (i == INPUT_METHOD_WIRED)
                {
                    #if(HOJA_CAPABILITY_BATTERY)
                    int8_t plugged_status = battery_get_plugged_status();

                    if(plugged_status!=1)
                    {
                        shutdown_lockout = false;
                        macro_deinit_cb = macro_deinit_complete;
                        hoja_deinit(macro_deinit_cb);
                    }   
                    #endif
                }
                sleep_ms(300);
            }
        }
        
    }

    if(macro_deinit_done && !in->button_shipping)
    {
        hoja_shutdown();
    }
}

bool macro_safe_mode_check()
{
    return _safe_mode_state;
}
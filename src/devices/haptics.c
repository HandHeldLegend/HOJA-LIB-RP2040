#include "devices/haptics.h"
#include "utilities/interval.h"

#include <string.h>

#include "devices_shared_types.h"
#include "board_config.h"
#include "utilities/settings.h"
#include "switch/switch_haptics.h"

#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER==HAPTICS_DRIVER_LRA_HAL)
#include "hal/lra_hal.h"
#elif defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER==HAPTICS_DRIVER_ERM_HAL)
#include "hal/erm_hal.h"
#endif

#include "usb/webusb.h"

volatile uint32_t _haptic_test_remaining = 0;
webreport_cmd_confirm_t _test_cb = NULL;

void haptic_config_cmd(haptic_cmd_t cmd, webreport_cmd_confirm_t cb)
{
    switch(cmd)
    {
        default:
        break;

        case HAPTIC_CMD_TEST_STRENGTH:
        _haptic_test_remaining = (1000000) / 8000;
        _test_cb = cb;
        break;
    }
}

void haptics_set_hd(haptic_processed_s *input)
{
    #if defined(HOJA_HAPTICS_PUSH_AMFM)
    HOJA_HAPTICS_PUSH_AMFM(input);
    #endif
}

void haptics_set_std(uint8_t amplitude, bool brake)
{
    #if defined(HOJA_HAPTICS_SET_STD)
    HOJA_HAPTICS_SET_STD(amplitude, brake);
    #endif
}

bool haptics_init() 
{
    if(haptic_config->haptic_config_version != CFG_BLOCK_HAPTIC_VERSION)
    {
        haptic_config->haptic_config_version = CFG_BLOCK_HAPTIC_VERSION;
        haptic_config->haptic_strength = 191;
        haptic_config->haptic_triggers = 1;
    }

    // Initialize the haptics
    switch_haptics_init();

    #if defined(HOJA_HAPTICS_INIT)
    return HOJA_HAPTICS_INIT(haptic_config->haptic_strength);
    #endif
}

void haptics_stop() 
{
    #if defined(HOJA_HAPTICS_STOP)
    HOJA_HAPTICS_STOP();
    #endif
}

void haptics_task(uint64_t timestamp)
{
    #if defined(HOJA_HAPTICS_TASK)
    HOJA_HAPTICS_TASK(timestamp);
    #endif

    if(_haptic_test_remaining > 0)
    {
        static interval_s interval = {0};
        if(interval_run(timestamp, 8000, &interval))
        {
            #if defined(HOJA_HAPTICS_SET_STD)
            HOJA_HAPTICS_SET_STD(255, false);
            #endif
            _haptic_test_remaining--;
            if(!_haptic_test_remaining)
            {
                #if defined(HOJA_HAPTICS_SET_STD)
                HOJA_HAPTICS_SET_STD(0, false);
                #endif
                if(_test_cb)
                    _test_cb(CFG_BLOCK_HAPTIC, HAPTIC_CMD_TEST_STRENGTH, true, NULL, 0);
            }
        }
    }
}

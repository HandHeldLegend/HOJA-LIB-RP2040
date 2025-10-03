#include "utilities/boot.h"
#include "hal/sys_hal.h"
#include "input/button.h"

#include "board_config.h"
#include "devices/battery.h"
#include "devices/bluetooth.h"
#include "utilities/settings.h"
#include "utilities/transport.h"

#include "hoja.h"

void boot_get_memory(boot_memory_s *out)
{
    boot_memory_s tmp = {0};
    tmp.val = sys_hal_get_bootmemory();

    if(tmp.magic_num != BOOT_MEM_MAGIC)
        out->val = 0x00000000;
    else 
    {
        out->val = tmp.val;
    }
}

void boot_set_memory(boot_memory_s *in)
{
    in->magic_num = BOOT_MEM_MAGIC;
    sys_hal_set_bootmemory(in->val);
}

void boot_clear_memory()
{
    sys_hal_set_bootmemory(0);
}

void boot_get_mode_method(transport_profile_s *profile)
{
    // Access boot time button state
    button_data_s buttons = {0};
    button_access_safe(&buttons, BUTTON_ACCESS_BOOT_DATA);

    transport_mode_t thisMode = TRANSPORT_MODE_SWPRO;

    // Set default return states
    bool             thisPair   = false;

    switch(gamepad_config->gamepad_default_mode)
    {
        default: 
        case 0: 
            thisMode = TRANSPORT_MODE_SWPRO;
        break;

        case 1:
            thisMode = TRANSPORT_MODE_XINPUT;
        break;

        case 2:
            thisMode = TRANSPORT_MODE_GCUSB;
        break;

        case 3:
            thisMode = TRANSPORT_MODE_GAMECUBE;
        break;

        case 4:
            thisMode = TRANSPORT_MODE_N64;
        break;

        case 5:
            thisMode = TRANSPORT_MODE_SNES;
        break;

        case 6:
            thisMode = TRANSPORT_MODE_SINPUT;
        break;
    }

    // Button overrides
    {
        // Input modes in order
        if(buttons.dpad_left) 
        {
            thisMode    = TRANSPORT_MODE_SNES;
        }
        else if(buttons.dpad_down)
        {
            thisMode    = TRANSPORT_MODE_N64;
        }
        else if(buttons.dpad_right)
        {
            thisMode    = TRANSPORT_MODE_GAMECUBE;
        }
#if defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_BAYX)
        else if(buttons.button_east)
        {
            thisMode            = TRANSPORT_MODE_SWPRO;
        }
        else if(buttons.button_south)
        {
            thisMode            = TRANSPORT_MODE_SINPUT;
        }
        else if(buttons.button_north)
        {
            thisMode            = TRANSPORT_MODE_XINPUT;
        }
        else if(buttons.button_west)
        {
            thisMode    = TRANSPORT_MODE_GCUSB;
        }
#elif defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_AXBY)
        else if(buttons.button_south)
        {
            thisMode = TRANSPORT_MODE_SWPRO;
        }
        else if(buttons.button_west)
        {
            thisMode = TRANSPORT_MODE_SINPUT;
        }
        else if(buttons.button_east)
        {
            thisMode = TRANSPORT_MODE_XINPUT;
        }
        else if(buttons.button_north)
        {
            thisMode = TRANSPORT_MODE_GCUSB;
        }
#else 
        else if(buttons.button_south)
        {
            thisMode            = TRANSPORT_MODE_SWPRO;
        }
        else if(buttons.button_east)
        {
            thisMode            = TRANSPORT_MODE_SINPUT;
        }
        else if(buttons.button_west)
        {
            thisMode            = TRANSPORT_MODE_XINPUT;
        }
        else if(buttons.button_north)
        {
            thisMode    = TRANSPORT_MODE_GCUSB;
        }
#endif 
    }

    // Check reboot memory for any overrides for buttons and whatnot
    boot_memory_s   boot_memory = {0};
    boot_get_memory(&boot_memory);
    boot_clear_memory();

    // If we have reboot memory, use it
    if(boot_memory.val != 0)
    {
        thisMode    = boot_memory.gamepad_mode;
        thisPair    = boot_memory.gamepad_pair ? true : false;
    }
    #if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER>0)
    else 
    {
        // Choose gamepad boot mode here based on button inputs
        if(buttons.trigger_r && buttons.button_plus)
        {
            thisMode = TRANSPORT_MODE_LOAD;
            thisPair = false;
            return;
        }
        else if(buttons.button_plus)
        {
            thisPair = true;
        }
    }
    #endif

    if(buttons.trigger_l && buttons.button_plus)
    {
        sys_hal_bootloader();
        return;
    }

    profile->bluetooth_pair = thisPair;

    // Apply profile specifics based on 
    // transport mode
    switch(thisMode)
    {
        default: 
        case TRANSPORT_MODE_SWPRO: 
            profile->bluetooth_supported = true;
            profile->gyro_supported = true;
            profile->haptics_supported = true;
        break;

        case TRANSPORT_MODE_XINPUT:
            profile->bluetooth_supported = false;
            profile->gyro_supported = false;
            profile->haptics_supported = true;
        break;

        case TRANSPORT_MODE_GCUSB:
            profile->bluetooth_supported = false;
            profile->gyro_supported = false;
            profile->haptics_supported = true;
        break;

        case TRANSPORT_MODE_GAMECUBE:
            profile->bluetooth_supported = false;
            profile->gyro_supported = false;
            profile->haptics_supported = true;
        break;

        case TRANSPORT_MODE_N64:
            profile->bluetooth_supported = false;
            profile->gyro_supported = false;
            profile->haptics_supported = true;
        break;

        case TRANSPORT_MODE_SNES:
            profile->bluetooth_supported = false;
            profile->gyro_supported = false;
            profile->haptics_supported = false;
        break;

        case TRANSPORT_MODE_SINPUT:
            profile->bluetooth_supported = true;
            profile->gyro_supported = false;
            profile->haptics_supported = false;
        break;
    }
}
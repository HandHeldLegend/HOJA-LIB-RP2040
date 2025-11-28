#include "utilities/boot.h"
#include "hal/sys_hal.h"

#include "board_config.h"
#include "devices/battery.h"
#include "devices/bluetooth.h"
#include "utilities/settings.h"
#include "input/hover.h"

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

void boot_get_mode_method(gamepad_mode_t *mode, gamepad_method_t *method, bool *pair)
{
    // Access boot time button state
    mapper_input_s input = hover_access_boot();

    // Set default return states
    gamepad_method_t thisMethod = GAMEPAD_METHOD_USB;
    gamepad_mode_t   thisMode   = GAMEPAD_MODE_SWPRO;
    bool             thisPair   = false;

    switch(gamepad_config->gamepad_default_mode)
    {
        default: 
        case 0: 
            thisMode            = GAMEPAD_MODE_SWPRO;
            thisMethod          = GAMEPAD_METHOD_AUTO;
        break;

        case 1:
            thisMode            = GAMEPAD_MODE_XINPUT;
            thisMethod          = GAMEPAD_METHOD_AUTO;
        break;

        case 2:
            thisMode    = GAMEPAD_MODE_GCUSB; 
            thisMethod  = GAMEPAD_METHOD_USB; // Force USB for now
        break;

        case 3:
            thisMode    = GAMEPAD_MODE_GAMECUBE; 
            thisMethod  = GAMEPAD_METHOD_WIRED; 
        break;

        case 4:
            thisMode    = GAMEPAD_MODE_N64; 
            thisMethod  = GAMEPAD_METHOD_WIRED; 
        break;

        case 5:
            thisMode    = GAMEPAD_MODE_SNES; 
            thisMethod  = GAMEPAD_METHOD_WIRED; 
        break;

        case 6:
            thisMode            = GAMEPAD_MODE_SINPUT;
            thisMethod          = GAMEPAD_METHOD_AUTO; 
        break;
    }

    // Button overrides
    {
        // Input modes in order
        if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_LEFT)) 
        {
            thisMode    = GAMEPAD_MODE_SNES;
            thisMethod  = GAMEPAD_METHOD_WIRED;
        }
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_DOWN))
        {
            thisMode    = GAMEPAD_MODE_N64;
            thisMethod  = GAMEPAD_METHOD_WIRED;
        }
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_RIGHT))
        {
            thisMode    = GAMEPAD_MODE_GAMECUBE;
            thisMethod  = GAMEPAD_METHOD_WIRED;
        }
#if defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_BAYX)
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_EAST))
        {
            thisMode            = GAMEPAD_MODE_SWPRO;
            thisMethod          = GAMEPAD_METHOD_AUTO;
        }
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_SOUTH))
        {
            thisMode            = GAMEPAD_MODE_SINPUT;
            thisMethod          = GAMEPAD_METHOD_AUTO; 
        }
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_NORTH))
        {
            thisMode            = GAMEPAD_MODE_XINPUT;
            thisMethod          = GAMEPAD_METHOD_AUTO; 
        }
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_WEST))
        {
            thisMode    = GAMEPAD_MODE_GCUSB;
            thisMethod  = GAMEPAD_METHOD_USB; // Force USB for now
        }
#elif defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_AXBY)
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_SOUTH))
        {
            thisMode            = GAMEPAD_MODE_SWPRO;
            thisMethod          = GAMEPAD_METHOD_AUTO;
        }
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_WEST))
        {
            thisMode            = GAMEPAD_MODE_SINPUT;
            thisMethod          = GAMEPAD_METHOD_AUTO; 
        }
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_EAST))
        {
            thisMode            = GAMEPAD_MODE_XINPUT;
            thisMethod          = GAMEPAD_METHOD_AUTO; 
        }
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_NORTH))
        {
            thisMode    = GAMEPAD_MODE_GCUSB;
            thisMethod  = GAMEPAD_METHOD_USB; // Force USB for now
        }
#else 
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_SOUTH))
        {
            thisMode            = GAMEPAD_MODE_SWPRO;
            thisMethod          = GAMEPAD_METHOD_AUTO;
        }
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_EAST))
        {
            thisMode            = GAMEPAD_MODE_SINPUT;
            thisMethod          = GAMEPAD_METHOD_AUTO; 
        }
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_WEST))
        {
            thisMode            = GAMEPAD_MODE_XINPUT;
            thisMethod          = GAMEPAD_METHOD_AUTO; 
        }
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_NORTH))
        {
            thisMode    = GAMEPAD_MODE_GCUSB;
            thisMethod  = GAMEPAD_METHOD_USB; // Force USB for now
        }
#endif 
    }

    // Check reboot memory for any overrides for buttons and whatnot
    boot_memory_s   boot_memory = {0};
    boot_get_memory(&boot_memory);
    boot_clear_memory();

    hoja_set_debug_data(boot_memory.reserved_2);

    // If we have reboot memory, use it
    if(boot_memory.val != 0)
    {
        thisMode    = boot_memory.gamepad_mode;
        thisMethod  = boot_memory.gamepad_method;
        thisPair    = boot_memory.gamepad_pair ? true : false;
    }
    #if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER>0)
    else 
    {
        // Choose gamepad boot mode here based on button inputs
        if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_RB) && MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_START))
        {
            *mode = GAMEPAD_MODE_LOAD;
            *method = GAMEPAD_METHOD_BLUETOOTH;
            *pair = false;
            return;
        }
        else if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_START))
        {
            thisPair = true;
        }
    }
    #endif

    if(MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_LB) && MAPPER_BUTTON_DOWN(input.inputs, INPUT_CODE_START))
    {
        sys_hal_bootloader();
        return;
    }

    // Set outputs
    *mode   = thisMode;
    *method = thisMethod;
    *pair   = thisPair;
}
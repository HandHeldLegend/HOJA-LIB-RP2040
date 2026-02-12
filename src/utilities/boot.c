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

    if (tmp.magic_num != BOOT_MEM_MAGIC)
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

void boot_get_mode_method(gamepad_mode_t *mode, gamepad_transport_t *transport, bool *pair)
{
    boot_input_s boot_dat = {.bootloader = false, .gamepad_mode = GAMEPAD_MODE_LOAD, .gamepad_transport = GAMEPAD_TRANSPORT_AUTO, .pairing_mode = false};

    // Set default return states
    gamepad_transport_t thisTransport = GAMEPAD_TRANSPORT_AUTO;
    gamepad_mode_t thisMode = GAMEPAD_MODE_SWPRO;
    bool thisPair = false;
    bool thisBootloader = false;
    bool thisBTBootloader = false;

    if (cb_hoja_boot(&boot_dat))
    {
        thisPair = boot_dat.pairing_mode;
        thisBootloader = boot_dat.bootloader;
        thisTransport = boot_dat.gamepad_transport;
        thisMode = boot_dat.gamepad_mode;
    }
    else
    {
        // Access boot time button state
        mapper_input_s input = hover_access_boot();

        if (input.presses[INPUT_CODE_RB] && input.presses[INPUT_CODE_START])
        {
            thisBTBootloader = true;
        }
        else if (input.presses[INPUT_CODE_START])
        {
            thisPair = true;
        }

        if (input.presses[INPUT_CODE_LB] && input.presses[INPUT_CODE_START])
        {
            thisBootloader = true;
        }
        else if (input.presses[INPUT_CODE_START])
        {
            thisPair = true;
        }

        // Button overrides
        {
            // Input modes in order
            if (input.presses[INPUT_CODE_LEFT])
            {
                thisMode = GAMEPAD_MODE_SNES;
            }
            else if (input.presses[INPUT_CODE_DOWN])
            {
                thisMode = GAMEPAD_MODE_N64;
            }
            else if (input.presses[INPUT_CODE_RIGHT])
            {
                thisMode = GAMEPAD_MODE_GAMECUBE;
            }
#if defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_BAYX)
            else if (input.presses[INPUT_CODE_EAST])
            {
                thisMode = GAMEPAD_MODE_SWPRO;
            }
            else if (input.presses[INPUT_CODE_SOUTH])
            {
                thisMode = GAMEPAD_MODE_SINPUT;
            }
            else if (input.presses[INPUT_CODE_NORTH])
            {
                thisMode = GAMEPAD_MODE_XINPUT;
            }
            else if (input.presses[INPUT_CODE_WEST])
            {
                thisMode = GAMEPAD_MODE_GCUSB;
            }
#elif defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_AXBY)
            else if (input.presses[INPUT_CODE_SOUTH])
            {
                thisMode = GAMEPAD_MODE_SWPRO;
            }
            else if (input.presses[INPUT_CODE_WEST])
            {
                thisMode = GAMEPAD_MODE_SINPUT;
            }
            else if (input.presses[INPUT_CODE_EAST])
            {
                thisMode = GAMEPAD_MODE_XINPUT;
            }
            else if (input.presses[INPUT_CODE_NORTH])
            {
                thisMode = GAMEPAD_MODE_GCUSB;
            }
#else
            else if (input.presses[INPUT_CODE_SOUTH])
            {
                thisMode = GAMEPAD_MODE_SWPRO;
            }
            else if (input.presses[INPUT_CODE_EAST])
            {
                thisMode = GAMEPAD_MODE_SINPUT;
            }
            else if (input.presses[INPUT_CODE_WEST])
            {
                thisMode = GAMEPAD_MODE_XINPUT;
            }
            else if (input.presses[INPUT_CODE_NORTH])
            {
                thisMode = GAMEPAD_MODE_GCUSB;
            }
#endif
        }
    }

    if (thisMode != GAMEPAD_MODE_LOAD)
        goto skipAutoGamepadMode;
    switch (gamepad_config->gamepad_default_mode)
    {
    default:
    case 0:
        thisMode = GAMEPAD_MODE_SWPRO;
        break;

    case 1:
        thisMode = GAMEPAD_MODE_XINPUT;
        break;

    case 2:
        thisMode = GAMEPAD_MODE_GCUSB;
        break;

    case 3:
        thisMode = GAMEPAD_MODE_GAMECUBE;
        break;

    case 4:
        thisMode = GAMEPAD_MODE_N64;
        break;

    case 5:
        thisMode = GAMEPAD_MODE_SNES;
        break;

    case 6:
        thisMode = GAMEPAD_MODE_SINPUT;
        break;
    }
skipAutoGamepadMode:

    if (thisTransport != GAMEPAD_TRANSPORT_AUTO)
        goto skipAutoTransport;

    switch (thisMode)
    {
    default:
    case GAMEPAD_MODE_SWPRO:
    case GAMEPAD_MODE_SINPUT:
    case GAMEPAD_MODE_GCUSB:
    case GAMEPAD_MODE_XINPUT:
        thisTransport = GAMEPAD_TRANSPORT_AUTO;
        break;

    case GAMEPAD_MODE_SNES:
        thisTransport = GAMEPAD_TRANSPORT_NESBUS;
        break;

    case GAMEPAD_MODE_N64:
        thisTransport = GAMEPAD_TRANSPORT_JOYBUS64;
        break;

    case GAMEPAD_MODE_GAMECUBE:
        thisTransport = GAMEPAD_TRANSPORT_JOYBUSGC;
        break;
    }
skipAutoTransport:

    // Check reboot memory for any overrides for buttons and whatnot
    boot_memory_s boot_memory = {0};
    boot_get_memory(&boot_memory);
    boot_clear_memory();

    hoja_set_debug_data(boot_memory.reserved_2);

    // If we have reboot memory, use it
    if (boot_memory.val != 0)
    {
        thisMode = boot_memory.gamepad_mode;
        thisPair = boot_memory.gamepad_pair ? true : false;
    }
#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER > 0)
    else
    {
#if (HOJA_BLUETOOTH_DRIVER == BLUETOOTH_DRIVER_ESP32HOJA)
        if (thisBTBootloader)
        {
            *mode = GAMEPAD_MODE_LOAD;
            *transport = GAMEPAD_TRANSPORT_BLUETOOTH;
            *pair = false;
            return;
        }
#endif
    }
#endif

    if (thisBootloader)
    {
        sys_hal_bootloader();
        return;
    }

    // Battery
    switch (thisTransport)
    {
    default:
        battery_init();
        break;

    case GAMEPAD_TRANSPORT_WLAN:
        battery_init();
        goto skipTransportParse;

    case GAMEPAD_TRANSPORT_NESBUS:
    case GAMEPAD_TRANSPORT_JOYBUS64:
    case GAMEPAD_TRANSPORT_JOYBUSGC:
        goto skipTransportParse;
    }

    // Obtain battery status
    battery_status_s status;
    battery_get_status(&status);

doTransportParse:
    // PMIC unused
    if (!status.connected)
    {
        thisTransport = GAMEPAD_TRANSPORT_USB;
    }
    else
    {
        // Wireless
        if (!status.plugged)
        {
            switch (thisMode)
            {
            case GAMEPAD_MODE_SWPRO:
            case GAMEPAD_MODE_SINPUT:
            case GAMEPAD_MODE_XINPUT:
                thisTransport = GAMEPAD_TRANSPORT_BLUETOOTH;
                break;

            case GAMEPAD_MODE_GCUSB:
            case GAMEPAD_MODE_N64:
            case GAMEPAD_MODE_GAMECUBE:
                thisTransport = GAMEPAD_TRANSPORT_WLAN;
                break;

            default:
                thisTransport = GAMEPAD_TRANSPORT_USB;
                break;
            }
        }
        else
        {
            thisTransport = GAMEPAD_TRANSPORT_USB;
        }
    }
skipTransportParse:

    // Set outputs
    *mode = thisMode;
    *transport = thisTransport;
    *pair = thisPair;
}
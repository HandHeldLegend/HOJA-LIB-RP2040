#include "utilities/boot.h"
#include "hal/sys_hal.h"

#include "board_config.h"
#include "driver_define_helper.h"
#include "devices/battery.h"
#include "devices/bluetooth.h"
#include "utilities/settings.h"
#include "utilities/static_config.h"
#include "input/hover.h"
#include "input_shared_types.h"

#include "hoja.h"

uint8_t boot_pick_strongest_analog4(const uint16_t raw[4], uint16_t min_delta, uint16_t min_activation)
{
    uint16_t max_v = raw[0];
    uint8_t max_i = 0;
    uint8_t max_count = 1;

    for (uint8_t i = 1; i < 4; i++)
    {
        if (raw[i] > max_v)
        {
            max_v = raw[i];
            max_i = i;
            max_count = 1;
        }
        else if (raw[i] == max_v)
        {
            max_count++;
        }
    }

    if (max_count != 1)
        return 0;
    if (max_v < min_activation)
        return 0;

    uint16_t second = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        if (i == max_i)
            continue;
        if (raw[i] > second)
            second = raw[i];
    }

    if ((uint16_t)(max_v - second) < min_delta)
        return 0;

    return (uint8_t)(1u << max_i);
}

#if defined(HOJA_SEWN_TYPE)
#define BOOT_SEWN_LAYOUT (HOJA_SEWN_TYPE)
#else
#define BOOT_SEWN_LAYOUT SEWN_LAYOUT_ABXY
#endif

// Face button order: South, East, West, North (indices 0..3)
static const gamepad_mode_t k_sewn_face_modes[3][4] = {
    [SEWN_LAYOUT_ABXY] = {GAMEPAD_MODE_SWPRO, GAMEPAD_MODE_SINPUT, GAMEPAD_MODE_XINPUT, GAMEPAD_MODE_GCUSB},
    [SEWN_LAYOUT_BAYX] = {GAMEPAD_MODE_SINPUT, GAMEPAD_MODE_SWPRO, GAMEPAD_MODE_GCUSB, GAMEPAD_MODE_XINPUT},
    [SEWN_LAYOUT_AXBY] = {GAMEPAD_MODE_SWPRO, GAMEPAD_MODE_XINPUT, GAMEPAD_MODE_SINPUT, GAMEPAD_MODE_GCUSB},
};

static bool boot_face_inputs_all_hover(void)
{
    static const uint8_t face[4] = {
        INPUT_CODE_SOUTH,
        INPUT_CODE_EAST,
        INPUT_CODE_WEST,
        INPUT_CODE_NORTH,
    };
    for (int i = 0; i < 4; i++)
    {
        if (input_static.input_info[face[i]].input_type != MAPPER_INPUT_TYPE_HOVER)
            return false;
    }
    return true;
}

#if defined(HOJA_BOOT_ANALOG_FACE_MODE) && (HOJA_BOOT_ANALOG_FACE_MODE)
static bool boot_try_builtin_analog_face(const mapper_input_s *in, gamepad_mode_t *mode_out)
{
    if (!boot_face_inputs_all_hover())
        return false;

    if (!hover_config->hover_calibration_set)
    {
        *mode_out = GAMEPAD_MODE_LOAD;
        return true;
    }

    uint16_t raw[4] = {
        in->inputs[INPUT_CODE_SOUTH],
        in->inputs[INPUT_CODE_EAST],
        in->inputs[INPUT_CODE_WEST],
        in->inputs[INPUT_CODE_NORTH],
    };

    uint8_t bit = boot_pick_strongest_analog4(raw, HOJA_BOOT_ANALOG_FACE_DELTA, HOJA_BOOT_ANALOG_FACE_MIN);
    if (bit == 0)
    {
        *mode_out = GAMEPAD_MODE_LOAD;
        return true;
    }

    uint8_t idx = (uint8_t)__builtin_ctz((unsigned)bit);
    *mode_out = k_sewn_face_modes[BOOT_SEWN_LAYOUT][idx];
    return true;
}
#endif

static void boot_apply_dpad_face_digital(const mapper_input_s *input, gamepad_mode_t *mode, bool skip_digital_face)
{
    if (input->presses[INPUT_CODE_LEFT])
    {
        *mode = GAMEPAD_MODE_SNES;
        return;
    }
    if (input->presses[INPUT_CODE_DOWN])
    {
        *mode = GAMEPAD_MODE_N64;
        return;
    }
    if (input->presses[INPUT_CODE_RIGHT])
    {
        *mode = GAMEPAD_MODE_GAMECUBE;
        return;
    }

    if (skip_digital_face)
        return;

    if (input->presses[INPUT_CODE_SOUTH])
        *mode = k_sewn_face_modes[BOOT_SEWN_LAYOUT][0];
    else if (input->presses[INPUT_CODE_EAST])
        *mode = k_sewn_face_modes[BOOT_SEWN_LAYOUT][1];
    else if (input->presses[INPUT_CODE_WEST])
        *mode = k_sewn_face_modes[BOOT_SEWN_LAYOUT][2];
    else if (input->presses[INPUT_CODE_NORTH])
        *mode = k_sewn_face_modes[BOOT_SEWN_LAYOUT][3];
}

static void boot_apply_start_combos(const mapper_input_s *input, bool *pair_out, bool *bootloader_out, bool *bt_bootloader_out)
{
    if (input->presses[INPUT_CODE_RB] && input->presses[INPUT_CODE_START])
        *bt_bootloader_out = true;
    else if (input->presses[INPUT_CODE_LB] && input->presses[INPUT_CODE_START])
        *bootloader_out = true;
    else if (input->presses[INPUT_CODE_START])
        *pair_out = true;
}

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
        mapper_input_s input = hover_access_boot();

        boot_apply_start_combos(&input, &thisPair, &thisBootloader, &thisBTBootloader);

        bool skip_digital_face = false;
        gamepad_mode_t face_mode = thisMode;

        if (cb_hoja_boot_custom_face_mode(&input, &face_mode))
        {
            thisMode = face_mode;
            skip_digital_face = true;
        }
#if defined(HOJA_BOOT_ANALOG_FACE_MODE) && (HOJA_BOOT_ANALOG_FACE_MODE)
        else if (boot_try_builtin_analog_face(&input, &face_mode))
        {
            thisMode = face_mode;
            skip_digital_face = true;
        }
#endif

        boot_apply_dpad_face_digital(&input, &thisMode, skip_digital_face);
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

    boot_memory_s boot_memory = {0};
    boot_get_memory(&boot_memory);
    boot_clear_memory();

    hoja_set_debug_data(boot_memory.reserved_2);

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

    battery_status_s status;
    battery_get_status(&status);

doTransportParse:
    if (!status.connected)
    {
        thisTransport = GAMEPAD_TRANSPORT_USB;
    }
    else
    {
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

    *mode = thisMode;
    *transport = thisTransport;
    *pair = thisPair;
}

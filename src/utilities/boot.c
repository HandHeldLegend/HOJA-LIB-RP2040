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
#include "input/mapper.h"
#include "cores/cores.h"

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

static uint8_t boot_sewn_layout_clamp(void)
{
    const hoja_config_s *cfg = hoja_config_get();
    uint8_t l = cfg ? cfg->sewn_layout : (uint8_t)SEWN_LAYOUT_ABXY;
    if (l > (uint8_t)SEWN_LAYOUT_AXBY)
        l = (uint8_t)SEWN_LAYOUT_ABXY;
    return l;
}

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
    *mode_out = k_sewn_face_modes[boot_sewn_layout_clamp()][idx];
    return true;
}
#endif

static void boot_apply_dpad_face_digital(const mapper_input_s *input, gamepad_mode_t *mode, bool skip_digital_face)
{
    const bool d_l = input->presses[INPUT_CODE_LEFT];
    const bool d_d = input->presses[INPUT_CODE_DOWN];
    const bool d_r = input->presses[INPUT_CODE_RIGHT];
    const unsigned d_cnt = (unsigned)d_l + (unsigned)d_d + (unsigned)d_r;

    if (d_cnt > 1u)
    {
        *mode = GAMEPAD_MODE_LOAD;
        return;
    }
    if (d_cnt == 1u)
    {
        if (d_l)
            *mode = GAMEPAD_MODE_SNES;
        else if (d_d)
            *mode = GAMEPAD_MODE_N64;
        else
            *mode = GAMEPAD_MODE_GAMECUBE;
        return;
    }

    if (skip_digital_face)
        return;

    const bool f_s = input->presses[INPUT_CODE_SOUTH];
    const bool f_e = input->presses[INPUT_CODE_EAST];
    const bool f_w = input->presses[INPUT_CODE_WEST];
    const bool f_n = input->presses[INPUT_CODE_NORTH];
    const unsigned f_cnt = (unsigned)f_s + (unsigned)f_e + (unsigned)f_w + (unsigned)f_n;

    if (f_cnt > 1u)
    {
        *mode = GAMEPAD_MODE_LOAD;
        return;
    }
    if (f_cnt == 0u)
        return;

    uint8_t layout = boot_sewn_layout_clamp();
    if (f_s)
        *mode = k_sewn_face_modes[layout][0];
    else if (f_e)
        *mode = k_sewn_face_modes[layout][1];
    else if (f_w)
        *mode = k_sewn_face_modes[layout][2];
    else
        *mode = k_sewn_face_modes[layout][3];
}

static bool boot_input_code_pressed(const mapper_input_s *input, mapper_input_code_t code)
{
    if (code == INPUT_CODE_UNUSED || code < 0 || code >= INPUT_CODE_MAX)
    {
        return false;
    }

    switch (input_static.input_info[code].input_type)
    {
    case MAPPER_INPUT_TYPE_HOVER:
    case MAPPER_INPUT_TYPE_JOYSTICK:
        return input->inputs[code] > 0;

    default:
        return input->presses[code];
    }
}

bool boot_sync_on_boot_pressed(const mapper_input_s *input)
{
    const hoja_config_s *cfg = hoja_config_get();
    if (!cfg || cfg->sync_on_boot_code == INPUT_CODE_UNUSED)
    {
        return false;
    }

    return boot_input_code_pressed(input, cfg->sync_on_boot_code);
}

static void boot_apply_start_combos(const mapper_input_s *input, bool *pair_out, bool *bootloader_out, bool *bt_bootloader_out)
{
    if (input->presses[INPUT_CODE_RB] && input->presses[INPUT_CODE_START])
        *bt_bootloader_out = true;
    else if (input->presses[INPUT_CODE_LB] && input->presses[INPUT_CODE_START])
        *bootloader_out = true;
    else if (boot_sync_on_boot_pressed(input))
        *pair_out = true;
}

static void boot_apply_wlan_combo(const mapper_input_s *input, gamepad_transport_t *transport_out,
                                  uint16_t *boot_flags_out)
{
    if (boot_flags_out == NULL || transport_out == NULL)
        return;

    // RB+START is the BT bootloader combo; do not treat it as a WLAN boot.
    if (input->presses[INPUT_CODE_RB] && input->presses[INPUT_CODE_START])
        return;

    if (input->presses[INPUT_CODE_RB])
    {
        *boot_flags_out |= COREBOOT_FLAG_WLAN;
        *transport_out = GAMEPAD_TRANSPORT_WLAN;
    }
}

static void boot_apply_bt_combo(const mapper_input_s *input, gamepad_transport_t *transport_out)
{
    if (transport_out == NULL)
        return;

    // LB+START is the USB bootloader combo; do not treat it as a BT boot.
    if (input->presses[INPUT_CODE_LB] && input->presses[INPUT_CODE_START])
        return;

    if (input->presses[INPUT_CODE_LB] && *transport_out == GAMEPAD_TRANSPORT_AUTO)
        *transport_out = GAMEPAD_TRANSPORT_BLUETOOTH;
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

void boot_get_mode_method(gamepad_mode_t *mode, gamepad_transport_t *transport, bool *pair, uint16_t *boot_flags)
{
    if (boot_flags != NULL)
        *boot_flags = 0;

    boot_input_s boot_dat = {.bootloader = false, .gamepad_mode = GAMEPAD_MODE_LOAD, .gamepad_transport = GAMEPAD_TRANSPORT_AUTO, .pairing_mode = false};

    gamepad_transport_t thisTransport = GAMEPAD_TRANSPORT_AUTO;
    // LOAD until a boot combo / face / d-pad selects a mode; otherwise gamepad_default_mode applies.
    gamepad_mode_t thisMode = GAMEPAD_MODE_LOAD;
    bool thisPair = false;
    bool thisBootloader = false;
    bool thisBTBootloader = false;
    uint16_t thisBootFlags = 0;

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
        boot_apply_wlan_combo(&input, &thisTransport, &thisBootFlags);
        boot_apply_bt_combo(&input, &thisTransport);

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

        if (boot_memory.gamepad_method == (uint8_t)GAMEPAD_METHOD_BLUETOOTH)
        {
            thisTransport = GAMEPAD_TRANSPORT_BLUETOOTH;
        }
        else if (boot_memory.gamepad_method == (uint8_t)GAMEPAD_METHOD_WLAN)
        {
            thisTransport = GAMEPAD_TRANSPORT_WLAN;
            thisBootFlags |= COREBOOT_FLAG_WLAN;
        }
    }
#if defined(HOJA_TRANSPORT_BT_DRIVER) && (HOJA_TRANSPORT_BT_DRIVER > 0)
    else
    {
#if (HOJA_TRANSPORT_BT_DRIVER == BT_DRIVER_ESP32HOJA)
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
    case GAMEPAD_TRANSPORT_BLUETOOTH:
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
                thisTransport = GAMEPAD_TRANSPORT_BLUETOOTH;
                break;

            case GAMEPAD_MODE_GCUSB:
            case GAMEPAD_MODE_N64:
            case GAMEPAD_MODE_GAMECUBE:
            case GAMEPAD_MODE_XINPUT:
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
    if (boot_flags != NULL)
        *boot_flags = thisBootFlags;
}

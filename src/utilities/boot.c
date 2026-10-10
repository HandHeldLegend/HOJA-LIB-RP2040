#include "utilities/boot.h"
#include "hal/sys_hal.h"

#include "board_config.h"
#include "driver_define_helper.h"
#include "devices/battery.h"
#include "utilities/settings.h"
#include "utilities/static_config.h"
#include "input/hover.h"
#include "input_shared_types.h"
#include "input/mapper.h"
#include "cores/cores.h"

#include "hoja.h"
#include "utilities/autodetect.h"
#include "transport/transport_wlan.h"

#ifndef HOJA_BOOT_ANALOG_FACE_DELTA
#define HOJA_BOOT_ANALOG_FACE_DELTA 200
#endif

#ifndef HOJA_BOOT_ANALOG_FACE_MIN
#define HOJA_BOOT_ANALOG_FACE_MIN 350
#endif

static boot_info_s _boot_info = {0};

// ---------------------------------------------------------------------------
// Analog helpers
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Input helpers
// ---------------------------------------------------------------------------

static bool boot_input_pressed(const mapper_input_s *input, mapper_input_code_t code)
{
    if (code == INPUT_CODE_UNUSED || code < 0 || code >= INPUT_CODE_MAX)
        return false;

    switch (input_static.input_info[code].input_type)
    {
    case MAPPER_INPUT_TYPE_HOVER:
    case MAPPER_INPUT_TYPE_JOYSTICK:
        return input->inputs[code] > 0;

    default:
        return input->presses[code];
    }
}

static bool boot_combo_pressed(const mapper_input_s *input, const mapper_input_code_t codes[2])
{
    if (!codes || codes[0] == INPUT_CODE_UNUSED)
        return false;

    if (!boot_input_pressed(input, codes[0]))
        return false;

    if (codes[1] == INPUT_CODE_UNUSED)
        return true;

    return boot_input_pressed(input, codes[1]);
}

static bool boot_group_all_hover(const mapper_input_code_t *codes, uint8_t count)
{
    for (uint8_t i = 0; i < count; i++)
    {
        if (input_static.input_info[codes[i]].input_type != MAPPER_INPUT_TYPE_HOVER)
            return false;
    }
    return true;
}

static uint8_t boot_count_pressed(const mapper_input_s *input, const mapper_input_code_t *codes, uint8_t count)
{
    uint8_t pressed = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        if (boot_input_pressed(input, codes[i]))
            pressed++;
    }
    return pressed;
}

// ---------------------------------------------------------------------------
// Gamepad format: ABXY face buttons and d-pad cardinals
// ---------------------------------------------------------------------------

static uint8_t boot_sewn_layout_clamp(void)
{
    const hoja_config_s *cfg = hoja_config_get();
    uint8_t l = cfg ? cfg->sewn_layout : (uint8_t)SEWN_LAYOUT_ABXY;
    if (l > (uint8_t)SEWN_LAYOUT_AXBY)
        l = (uint8_t)SEWN_LAYOUT_ABXY;
    return l;
}

// Face button order: South, East, West, North (indices 0..3)
static const core_reportformat_t k_face_formats[3][4] = {
    [SEWN_LAYOUT_ABXY] = {CORE_REPORTFORMAT_SWPRO, CORE_REPORTFORMAT_SINPUT, CORE_REPORTFORMAT_XINPUT, CORE_REPORTFORMAT_SLIPPI},
    [SEWN_LAYOUT_BAYX] = {CORE_REPORTFORMAT_SINPUT, CORE_REPORTFORMAT_SWPRO, CORE_REPORTFORMAT_SLIPPI, CORE_REPORTFORMAT_XINPUT},
    [SEWN_LAYOUT_AXBY] = {CORE_REPORTFORMAT_SWPRO, CORE_REPORTFORMAT_XINPUT, CORE_REPORTFORMAT_SINPUT, CORE_REPORTFORMAT_SLIPPI},
};

static const mapper_input_code_t k_face_codes[4] = {
    INPUT_CODE_SOUTH,
    INPUT_CODE_EAST,
    INPUT_CODE_WEST,
    INPUT_CODE_NORTH,
};

// Wii mode needs BTstack (RM2 or the ESP32 HCI bridge). Without it d-pad up stays out of the boot
// mode group entirely, so those boards keep the three-way SNES / N64 / GameCube pick.
#if defined(HOJA_TRANSPORT_BT_DRIVER) && HOJA_BT_USES_BTSTACK
#define BOOT_WII_SUPPORTED 1
#define BOOT_DPAD_COUNT    4
#else
#define BOOT_WII_SUPPORTED 0
#define BOOT_DPAD_COUNT    3
#endif

static const mapper_input_code_t k_dpad_codes[4] = {
    INPUT_CODE_LEFT,
    INPUT_CODE_DOWN,
    INPUT_CODE_RIGHT,
    INPUT_CODE_UP,
};

static const core_reportformat_t k_dpad_formats[4] = {
    CORE_REPORTFORMAT_SNES,
    CORE_REPORTFORMAT_N64,
    CORE_REPORTFORMAT_GAMECUBE,
    CORE_REPORTFORMAT_WII,
};

static bool boot_format_supported(core_reportformat_t format)
{
    if (format == CORE_REPORTFORMAT_WII)
        return BOOT_WII_SUPPORTED;

    return (format > CORE_REPORTFORMAT_UNDEFINED) && (format < CORE_REPORTFORMAT_MAX);
}

// Strongest of up to four hover inputs; unused slots read as released.
static bool boot_try_hover_analog(const mapper_input_s *in, const mapper_input_code_t *codes, uint8_t count,
                                  uint16_t min_delta, uint16_t min_activation, uint8_t *index_out)
{
    if (!boot_group_all_hover(codes, count))
        return false;

    uint16_t raw[4] = {0};
    for (uint8_t i = 0; i < count; i++)
        raw[i] = in->inputs[codes[i]];

    uint8_t bit = boot_pick_strongest_analog4(raw, min_delta, min_activation);
    if (bit == 0 || bit >= (1u << count))
        return false;

    *index_out = (uint8_t)__builtin_ctz((unsigned)bit);
    return true;
}

static bool boot_resolve_hover_face(const mapper_input_s *in, core_reportformat_t *format_out)
{
    if (!boot_group_all_hover(k_face_codes, 4))
        return false;

    if (!hover_config->hover_calibration_set)
    {
        *format_out = CORE_REPORTFORMAT_UNDEFINED;
        return true;
    }

    uint8_t idx = 0;
    if (!boot_try_hover_analog(in, k_face_codes, 4, HOJA_BOOT_ANALOG_FACE_DELTA, HOJA_BOOT_ANALOG_FACE_MIN, &idx))
    {
        *format_out = CORE_REPORTFORMAT_UNDEFINED;
        return true;
    }

    *format_out = k_face_formats[boot_sewn_layout_clamp()][idx];
    return true;
}

static uint8_t boot_resolve_single_pressed_index(const mapper_input_s *input,
                                                 const mapper_input_code_t *codes, uint8_t count)
{
    const uint8_t pressed = boot_count_pressed(input, codes, count);
    if (pressed != 1u)
        return 0xFFu;

    for (uint8_t i = 0; i < count; i++)
    {
        if (boot_input_pressed(input, codes[i]))
            return i;
    }

    return 0xFFu;
}

static void boot_resolve_face_digital(const mapper_input_s *input, core_reportformat_t *format)
{
    const uint8_t idx = boot_resolve_single_pressed_index(input, k_face_codes, 4);
    if (idx == 0xFFu)
    {
        if (boot_count_pressed(input, k_face_codes, 4) > 1u)
            *format = CORE_REPORTFORMAT_UNDEFINED;
        return;
    }

    *format = k_face_formats[boot_sewn_layout_clamp()][idx];
}

static void boot_resolve_dpad(const mapper_input_s *input, core_reportformat_t *format)
{
    uint8_t idx = 0xFFu;

    if (boot_group_all_hover(k_dpad_codes, BOOT_DPAD_COUNT))
    {
        if (!hover_config->hover_calibration_set)
        {
            *format = CORE_REPORTFORMAT_UNDEFINED;
            return;
        }

        // Several directions held count as none held, leaving the face buttons / default. Analog
        // directions count as held past the boot activation level, so a light graze does not.
        uint8_t held = 0;
        for (uint8_t i = 0; i < BOOT_DPAD_COUNT; i++)
        {
            if (input->inputs[k_dpad_codes[i]] >= HOJA_BOOT_ANALOG_FACE_MIN)
                held++;
        }
        if (held > 1u)
            return;

        if (boot_try_hover_analog(input, k_dpad_codes, BOOT_DPAD_COUNT, HOJA_BOOT_ANALOG_FACE_DELTA,
                                  HOJA_BOOT_ANALOG_FACE_MIN, &idx))
        {
            *format = k_dpad_formats[idx];
            return;
        }

        // Several directions held count as none held, leaving the face buttons / default
        idx = boot_resolve_single_pressed_index(input, k_dpad_codes, BOOT_DPAD_COUNT);
        if (idx == 0xFFu)
            return;

        *format = k_dpad_formats[idx];
        return;
    }

    // Several directions held count as none held, leaving the face buttons / default
    idx = boot_resolve_single_pressed_index(input, k_dpad_codes, BOOT_DPAD_COUNT);
    if (idx == 0xFFu)
        return;

    *format = k_dpad_formats[idx];
}

static core_reportformat_t boot_resolve_reportformat(const mapper_input_s *input)
{
    core_reportformat_t format = CORE_REPORTFORMAT_UNDEFINED;
    bool hover_face_handled = false;

    if (cb_hoja_boot_custom_face_mode(input, &format))
        hover_face_handled = true;
    else if (boot_resolve_hover_face(input, &format))
        hover_face_handled = true;

    boot_resolve_dpad(input, &format);

    if (!hover_face_handled)
        boot_resolve_face_digital(input, &format);

    // Nothing held: boot_init picks the default for the power source
    if (format == CORE_REPORTFORMAT_UNDEFINED)
        return CORE_REPORTFORMAT_UNDEFINED;

    if (!boot_format_supported(format))
        format = CORE_REPORTFORMAT_SWPRO;

    return format;
}

// Only reads the PMIC: a retro console boot leaves it unconfigured, like a manual one
static autodetect_power_t boot_power_source(void)
{
    battery_status_s status = {0};
    if (!battery_peek_status(&status))
        return AUTODETECT_POWER_UNKNOWN;
    return status.plugged ? AUTODETECT_POWER_EXTERNAL : AUTODETECT_POWER_BATTERY;
}

// The saved defaults as modes. CORE_REPORTFORMAT_UNDEFINED means Auto.
static core_reportformat_t boot_default_wired(void)
{
    const uint8_t stored = gamepad_config->gamepad_default_mode;
    if (stored == GAMEPAD_DEFAULT_MODE_AUTO)
        return CORE_REPORTFORMAT_UNDEFINED;

    const core_reportformat_t format = core_reportformat_from_default(stored);
    return boot_format_supported(format) ? format : CORE_REPORTFORMAT_SWPRO;
}

static core_reportformat_t boot_default_wireless(void)
{
    const core_reportformat_t format = (core_reportformat_t)gamepad_config->gamepad_default_wireless;
    switch (format)
    {
    case CORE_REPORTFORMAT_SWPRO:
    case CORE_REPORTFORMAT_SINPUT:
    case CORE_REPORTFORMAT_WII:
        if (boot_format_supported(format))
            return format;
        break;

    default:
        break;
    }
    return CORE_REPORTFORMAT_UNDEFINED;
}

// "WLAN dongle" as the wireless default, on a board with WLAN
static bool boot_default_wlan(void)
{
    return (gamepad_config->gamepad_default_wireless == GAMEPAD_DEFAULT_WIRELESS_WLAN) &&
           transport_wlan_static_supported();
}

// Modes the WLAN dongle can present
static bool boot_format_wlan(core_reportformat_t format)
{
    switch (format)
    {
    case CORE_REPORTFORMAT_SWPRO:
    case CORE_REPORTFORMAT_SINPUT:
    case CORE_REPORTFORMAT_XINPUT:
    case CORE_REPORTFORMAT_SLIPPI:
    case CORE_REPORTFORMAT_SNES:
    case CORE_REPORTFORMAT_N64:
    case CORE_REPORTFORMAT_GAMECUBE:
        return true;

    default:
        return false;
    }
}

// On battery, wireless means the WLAN dongle: it is the default and the WLAN button wasn't held
static bool _boot_prefer_wlan = false;

// The default for this power source. On battery the wireless default applies, except that an N64
// doesn't power the controller: a wired default of N64 stays, and a wired Auto still answers an N64
// before going wireless.
static core_reportformat_t boot_resolve_default(autodetect_power_t power)
{
    const core_reportformat_t wired = boot_default_wired();
    if ((power != AUTODETECT_POWER_BATTERY) || (wired == CORE_REPORTFORMAT_N64) ||
        (wired == CORE_REPORTFORMAT_UNDEFINED))
        return wired;
    return boot_default_wireless();
}

// ---------------------------------------------------------------------------
// Boot combos (USB bootloader, baseband update, pairing, WLAN force)
// ---------------------------------------------------------------------------

static bool boot_combo_usb_bootloader(const mapper_input_s *input)
{
    const hoja_config_s *cfg = hoja_config_get();
    if (!cfg)
        return false;

    return boot_combo_pressed(input, cfg->usb_bootloader_code);
}

#if HOJA_BT_IS_ESP32
static bool boot_combo_baseband_update(const mapper_input_s *input)
{
    const hoja_config_s *cfg = hoja_config_get();
    if (!cfg || cfg->baseband_update_code[0] == INPUT_CODE_UNUSED)
        return false;

    return boot_combo_pressed(input, cfg->baseband_update_code);
}
#endif

static bool boot_combo_pairing(const mapper_input_s *input)
{
    const hoja_config_s *cfg = hoja_config_get();
    if (!cfg || cfg->sync_on_boot_code == INPUT_CODE_UNUSED)
        return false;

    return boot_input_pressed(input, cfg->sync_on_boot_code);
}

static bool boot_combo_wlan_force(const mapper_input_s *input)
{
    const hoja_config_s *cfg = hoja_config_get();
    if (!cfg || cfg->wlan_force_code == INPUT_CODE_UNUSED)
        return false;

    if (boot_combo_usb_bootloader(input))
        return false;

#if HOJA_BT_IS_ESP32
    if (boot_combo_baseband_update(input))
        return false;
#endif

    return boot_input_pressed(input, cfg->wlan_force_code);
}

static void boot_apply_wlan_force(boot_info_s *info)
{
    info->flags |= COREBOOT_FLAG_WLAN;

    if (boot_format_wlan(info->reportformat))
        info->transport = GAMEPAD_TRANSPORT_WLAN;
}

// ---------------------------------------------------------------------------
// Transport defaults and battery policy
// ---------------------------------------------------------------------------

static void boot_apply_wired_transport_default(core_reportformat_t format, boot_info_s *info)
{
    if (info->transport != GAMEPAD_TRANSPORT_AUTO)
        return;

    switch (format)
    {
    case CORE_REPORTFORMAT_SNES:
        info->transport = GAMEPAD_TRANSPORT_NESBUS;
        break;

    case CORE_REPORTFORMAT_N64:
        info->transport = GAMEPAD_TRANSPORT_JOYBUS64;
        break;

    case CORE_REPORTFORMAT_GAMECUBE:
        info->transport = GAMEPAD_TRANSPORT_JOYBUSGC;
        break;

    // The Wii only speaks Bluetooth
    case CORE_REPORTFORMAT_WII:
        info->transport = GAMEPAD_TRANSPORT_BLUETOOTH;
        break;

    default:
        break;
    }
}

static void boot_apply_lb_bt_hint(const mapper_input_s *input, boot_info_s *info)
{
    if (boot_combo_usb_bootloader(input))
        return;

    if (boot_input_pressed(input, INPUT_CODE_LB) && info->transport == GAMEPAD_TRANSPORT_AUTO)
        info->transport = GAMEPAD_TRANSPORT_BLUETOOTH;
}

static void boot_apply_persisted_memory(boot_info_s *info)
{
    boot_memory_s boot_memory = {0};
    boot_get_memory(&boot_memory);
    boot_clear_memory();

    if (boot_memory.val == 0)
        return;

#if HOJA_BT_IS_ESP32
    // Requested over WebUSB (config app). Mirror the physical boot combo: force
    // the Bluetooth transport into ALTFLASH load mode so the ESP32 baseband can
    // be reflashed.
    if (boot_memory.baseband_update)
    {
        // Fully mirror the physical boot-combo result (step 2). Steps 3-7 have
        // already run here, so reset reportformat/pairing/flags too — otherwise a
        // resolved reportformat (e.g. SWPRO) leaks into core_current_color_get()
        // and the status LED renders the wrong color instead of pulsing orange.
        info->reportformat = CORE_REPORTFORMAT_UNDEFINED;
        info->pairing = false;
        info->baseband_bootloader = true;
        info->flags = COREBOOT_FLAG_ALTFLASH;
        info->transport = GAMEPAD_TRANSPORT_BLUETOOTH;
        return;
    }
#endif

    if (boot_memory.report_format < (uint8_t)CORE_REPORTFORMAT_MAX &&
        boot_format_supported((core_reportformat_t)boot_memory.report_format))
        info->reportformat = (core_reportformat_t)boot_memory.report_format;

    info->pairing = boot_memory.gamepad_pair ? true : false;

    if (boot_memory.gamepad_method == (uint8_t)GAMEPAD_METHOD_BLUETOOTH)
        info->transport = GAMEPAD_TRANSPORT_BLUETOOTH;
    else if (boot_memory.gamepad_method == (uint8_t)GAMEPAD_METHOD_WLAN)
    {
        info->transport = GAMEPAD_TRANSPORT_WLAN;
        info->flags |= COREBOOT_FLAG_WLAN;
    }
}

static void boot_apply_battery_transport(boot_info_s *info)
{
    switch (info->transport)
    {
    case GAMEPAD_TRANSPORT_NESBUS:
    case GAMEPAD_TRANSPORT_JOYBUS64:
    case GAMEPAD_TRANSPORT_JOYBUSGC:
    case GAMEPAD_TRANSPORT_WLAN:
    case GAMEPAD_TRANSPORT_BLUETOOTH:
    case GAMEPAD_TRANSPORT_USB:
        return;

    default:
        break;
    }

    battery_init();

    battery_status_s status;
    battery_get_status(&status);

    if (!status.connected)
    {
        info->transport = GAMEPAD_TRANSPORT_USB;
        return;
    }

    if (status.plugged)
    {
        info->transport = GAMEPAD_TRANSPORT_USB;
        return;
    }

    // On battery power: the WLAN dongle when it is the default, else by report format
    if (_boot_prefer_wlan && boot_format_wlan(info->reportformat))
    {
        info->transport = GAMEPAD_TRANSPORT_WLAN;
        return;
    }

    switch (info->reportformat)
    {
    case CORE_REPORTFORMAT_SWPRO:
    case CORE_REPORTFORMAT_SINPUT:
        info->transport = GAMEPAD_TRANSPORT_BLUETOOTH;
        break;

    case CORE_REPORTFORMAT_SLIPPI:
    case CORE_REPORTFORMAT_N64:
    case CORE_REPORTFORMAT_GAMECUBE:
    case CORE_REPORTFORMAT_XINPUT:
        info->transport = GAMEPAD_TRANSPORT_WLAN;
        break;

    default:
        info->transport = GAMEPAD_TRANSPORT_USB;
        break;
    }
}

// ---------------------------------------------------------------------------
// Boot memory API
// ---------------------------------------------------------------------------

void boot_get_memory(boot_memory_s *out)
{
    boot_memory_s tmp = {0};
    tmp.val = sys_hal_get_bootmemory();

    if (tmp.magic_num != BOOT_MEM_MAGIC)
        out->val = 0x00000000;
    else
        out->val = tmp.val;
}

void boot_set_memory(boot_memory_s *in)
{
    in->magic_num = BOOT_MEM_MAGIC;
    sys_hal_set_bootmemory(in->val);
}

void boot_clear_memory(void)
{
    sys_hal_set_bootmemory(0);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

const boot_info_s *boot_get_info(void)
{
    return &_boot_info;
}

void boot_set_mode(core_reportformat_t reportformat, gamepad_transport_t transport)
{
    _boot_info.reportformat = reportformat;
    _boot_info.transport = transport;
}

void boot_init(void)
{
    _boot_info = (boot_info_s){
        .reportformat = CORE_REPORTFORMAT_UNDEFINED,
        .transport = GAMEPAD_TRANSPORT_AUTO,
        .flags = 0,
        .pairing = false,
        .usb_bootloader = false,
        .baseband_bootloader = false,
    };

    const mapper_input_s input = hover_access_boot();
    const hoja_config_s *cfg = hoja_config_get();

    // 1. USB bootloader — reboots into UF2, does not return.
    if (cfg && boot_combo_usb_bootloader(&input))
    {
        sys_hal_bootloader();
        return;
    }

#if HOJA_BT_IS_ESP32
    // 2. ESP32 baseband firmware update.
    if (cfg && boot_combo_baseband_update(&input))
    {
        _boot_info.baseband_bootloader = true;
        _boot_info.flags |= COREBOOT_FLAG_ALTFLASH;
        _boot_info.transport = GAMEPAD_TRANSPORT_BLUETOOTH;
        return;
    }
#endif

    // 3. Gamepad mode from ABXY / d-pad, else the saved default for the power source. Undefined
    //    means Auto.
    const autodetect_power_t power = boot_power_source();
    _boot_info.reportformat = boot_resolve_reportformat(&input);
    _boot_info.mode_chosen = (_boot_info.reportformat != CORE_REPORTFORMAT_UNDEFINED);
    if (_boot_info.reportformat == CORE_REPORTFORMAT_UNDEFINED)
        _boot_info.reportformat = boot_resolve_default(power);
    const bool auto_mode = (_boot_info.reportformat == CORE_REPORTFORMAT_UNDEFINED);
    if (auto_mode)
        _boot_info.reportformat = CORE_REPORTFORMAT_SWPRO;

    // 4. Pairing enable combo.
    if (boot_combo_pairing(&input))
        _boot_info.pairing = true;

    // 5. The WLAN button picks the wireless transport that isn't the default: WLAN when the default
    //    is Bluetooth, Bluetooth when it is the WLAN dongle.
    const bool wlan_button = boot_combo_wlan_force(&input);
    _boot_prefer_wlan = boot_default_wlan() && !wlan_button;
    if (wlan_button && !boot_default_wlan())
        boot_apply_wlan_force(&_boot_info);

    // 6. LB held at boot prefers Bluetooth when transport is still AUTO.
    boot_apply_lb_bt_hint(&input, &_boot_info);

    // 7. Wired formats default to their bus transport, Wii to Bluetooth.
    boot_apply_wired_transport_default(_boot_info.reportformat, &_boot_info);

    // 8. Runtime reboot memory (pairing macro, etc.) overrides selections above.
    boot_memory_s boot_memory = {0};
    boot_get_memory(&boot_memory);
    const bool from_memory = (boot_memory.val != 0);
    boot_apply_persisted_memory(&_boot_info);

    // A wired format chosen through boot memory still needs its bus
    boot_apply_wired_transport_default(_boot_info.reportformat, &_boot_info);

    // Auto with nothing in boot memory and nothing else picked (pairing, LB, WLAN force): detect,
    // starting from the power source
    if (auto_mode && !from_memory && !_boot_info.pairing && _boot_info.transport == GAMEPAD_TRANSPORT_AUTO)
    {
        const bool wired_auto = (boot_default_wired() == CORE_REPORTFORMAT_UNDEFINED);
        autodetect_boot(&_boot_info, power, wired_auto, boot_default_wireless(), _boot_prefer_wlan);
    }

    // 9. Battery status resolves AUTO transport to USB / BT / WLAN.
    boot_apply_battery_transport(&_boot_info);
}

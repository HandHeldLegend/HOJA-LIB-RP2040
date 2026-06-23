#include <string.h>

#include "cores/core_switch.h"

#include "ns_lib.h"

#include "transport/transport.h"
#include "hoja_shared_types.h"
#include "input/imu.h"
#include "input/mapper.h"
#include "input/dpad.h"

#include "utilities/settings.h"
#include "utilities/pcm.h"

#include "devices/battery.h"
#include "devices/fuelgauge.h"
#include "devices/haptics.h"

#include "hal/sys_hal.h"

#include "hoja_usb.h"
#include "hoja.h"

#include "devices_shared_types.h"

#define CORE_SWITCH_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

// USB/Bluetooth HID descriptors are owned by NS-LIB-HID and populated at init.
// For USB, HHL-TINYUSB-DRIVERS appends the WebUSB vendor interface dynamically.
static core_hid_device_t _core_switch_hid_device = {0};

static bool _core_switch_populate_hid_device(bool usb_transport)
{
    const uint8_t *report_desc = NULL;
    const uint8_t *config_desc = NULL;
    uint16_t report_len = 0;
    uint16_t config_len = 0;
    uint16_t vid = 0;
    uint16_t pid = 0;

    if (!ns_hid_get_descriptor_params(&report_desc, &report_len, &config_desc, &config_len, &vid, &pid))
    {
        return false;
    }

    _core_switch_hid_device.device_descriptor =
        (const hoja_usb_device_descriptor_t *)ns_hid_get_device_descriptor();
    _core_switch_hid_device.hid_report_descriptor = report_desc;
    _core_switch_hid_device.hid_report_descriptor_len = report_len;
    _core_switch_hid_device.config_descriptor = config_desc;
    _core_switch_hid_device.config_descriptor_len = config_len;
    _core_switch_hid_device.vid = vid;
    _core_switch_hid_device.pid = pid;

    const char *dev_name = ns_hid_get_device_name();
    if (dev_name != NULL)
    {
        strncpy(_core_switch_hid_device.name, dev_name, sizeof(_core_switch_hid_device.name) - 1);
        _core_switch_hid_device.name[sizeof(_core_switch_hid_device.name) - 1] = '\0';
    }

    // Preserve the historically advertised 250mA on USB (NS-LIB-HID encodes
    // 500mA). HHL-TINYUSB-DRIVERS applies this when composing the descriptor.
    _core_switch_hid_device.max_power_ma = usb_transport ? 250 : 0;

    return true;
}

static core_params_s *_core_switch_params = NULL;
static uint8_t _core_switch_report_size = 64;

bool _core_switch_get_generated_report(core_report_s *out)
{
    if (ns_api_generate_inputreport(out->data))
    {
        out->reportformat = CORE_REPORTFORMAT_SWPRO;
        out->size = _core_switch_report_size;

        out->reliable = out->data[0] != NS_INPUT_REPORT_ID_FULL;
        return true;
    }
    return false;
}

void ns_api_hook_get_time_ms(uint64_t *ms)
{
    if (!ms)
    {
        return;
    }
    sys_hal_time_ms(ms);
}

uint8_t ns_api_hook_get_random_u8(void)
{
    return (uint8_t)(sys_hal_random() & 0xFF);
}

void ns_api_hook_get_input(ns_input_s *out)
{
    if (!out)
    {
        return;
    }

    mapper_input_s input = mapper_get_input();

    bool dpad[4] = {input.presses[SWITCH_CODE_DOWN], input.presses[SWITCH_CODE_RIGHT], input.presses[SWITCH_CODE_LEFT],
                    input.presses[SWITCH_CODE_UP]};
    dpad_translate_input(dpad);

    memset(out, 0, sizeof(*out));
    out->down = dpad[0];
    out->right = dpad[1];
    out->left = dpad[2];
    out->up = dpad[3];

    out->y = input.presses[SWITCH_CODE_Y];
    out->x = input.presses[SWITCH_CODE_X];
    out->a = input.presses[SWITCH_CODE_A];
    out->b = input.presses[SWITCH_CODE_B];

    out->minus = input.presses[SWITCH_CODE_MINUS];
    out->plus = input.presses[SWITCH_CODE_PLUS];
    out->home = input.presses[SWITCH_CODE_HOME];
    out->capture = input.presses[SWITCH_CODE_CAPTURE];

    out->r_js = input.presses[SWITCH_CODE_RS];
    out->l_js = input.presses[SWITCH_CODE_LS];

    out->r = input.presses[SWITCH_CODE_R];
    out->l = input.presses[SWITCH_CODE_L];
    out->zl = input.presses[SWITCH_CODE_ZL];
    out->zr = input.presses[SWITCH_CODE_ZR];

    uint16_t lx = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_LX_LEFT], input.inputs[SWITCH_CODE_LX_RIGHT]);
    uint16_t ly = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_LY_DOWN], input.inputs[SWITCH_CODE_LY_UP]);
    uint16_t rx = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_RX_LEFT], input.inputs[SWITCH_CODE_RX_RIGHT]);
    uint16_t ry = mapper_joystick_concat(2048, input.inputs[SWITCH_CODE_RY_DOWN], input.inputs[SWITCH_CODE_RY_UP]);

    out->ls_x = (uint16_t)CORE_SWITCH_CLAMP(lx, 0, 4095);
    out->ls_y = (uint16_t)CORE_SWITCH_CLAMP(ly, 0, 4095);
    out->rs_x = (uint16_t)CORE_SWITCH_CLAMP(rx, 0, 4095);
    out->rs_y = (uint16_t)CORE_SWITCH_CLAMP(ry, 0, 4095);
}

void ns_api_hook_get_powerstatus(ns_powerstatus_s *out)
{
    if (!out)
    {
        return;
    }

    fuelgauge_status_s fstat = {0};
    fuelgauge_get_status(&fstat);

    battery_status_s bstat = {0};
    battery_get_status(&bstat);

    out->battery_level = NS_BATLVL_FULL;
    out->charging_status = NS_CHARGING_IDLE;
    out->power_source = NS_POWERSRC_EXTERNAL;

    if (bstat.connected)
    {
        out->charging_status = NS_CHARGING_IDLE;

        if (bstat.charging)
        {
            out->charging_status = NS_CHARGING_ACTIVE;
            out->power_source = NS_POWERSRC_EXTERNAL;
        }
        else if (bstat.plugged)
        {
            out->charging_status = NS_CHARGING_IDLE;
            out->power_source = NS_POWERSRC_EXTERNAL;
        }

        if (fstat.connected)
        {
            switch (fstat.simple)
            {
            default:
                out->battery_level = NS_BATLVL_CRITICAL;
                break;
            case 1:
                out->battery_level = NS_BATLVL_CRITICAL;
                break;
            case 2:
                out->battery_level = NS_BATLVL_LOW;
                break;
            case 3:
                out->battery_level = NS_BATLVL_MED;
                break;
            case 4:
                out->battery_level = NS_BATLVL_FULL;
                break;
            }
        }
    }
    else
    {
        out->charging_status = NS_CHARGING_IDLE;
        out->power_source = NS_POWERSRC_EXTERNAL;
    }
}

void ns_api_hook_set_led(int player_leds)
{
    if (player_leds < 0)
    {
        tp_evt_s devt = {.evt = TP_EVT_CONNECTIONCHANGE, .evt_connectionchange = {.connection=TP_CONNECTION_DISCONNECTED}};
        transport_evt_cb(devt);
        return;
    }

    tp_evt_s evt = {.evt = TP_EVT_CONNECTIONCHANGE, .evt_connectionchange = {.connection=TP_CONNECTION_CONNECTED}};
    transport_evt_cb(evt);

    tp_evt_s pevt = {.evt = TP_EVT_PLAYERLED, .evt_playernumber = {.player_number = (uint8_t)player_leds}};
    transport_evt_cb(pevt);
}

void ns_api_hook_set_power(uint8_t shutdown)
{
    if (!shutdown)
    {
        return;
    }
    tp_evt_s pevt = {.evt = TP_EVT_POWERCOMMAND, .evt_powercommand = {.power_command = TP_POWERCOMMAND_SHUTDOWN}};
    transport_evt_cb(pevt);
}

void ns_api_hook_set_usbpair(ns_usbpair_s pairing_data)
{
    memcpy(gamepad_config->host_mac_switch, pairing_data.host_mac, 6);
    memcpy(switchpair_config->link_key, pairing_data.link_key, 16);
    switchpair_config->switchpair_config_version = CFG_BLOCK_SWITCHPAIR_VERSION;
    settings_commit_blocks();
}

void ns_api_hook_set_imu_mode(ns_imu_mode_t imu_mode)
{
    //tp_imucommand_t cmd;
    switch(imu_mode)
    {
        default:
        case NS_IMU_OFF:
        imu_set_read_mode(IMU_MODE_OFF);
        break;

        case NS_IMU_RAW:
        imu_set_read_mode(IMU_MODE_STANDARD);
        break;

        case NS_IMU_QUAT:
        imu_set_read_mode(IMU_MODE_QUATERNION);
        break;
    }
    //tp_evt_s evt = {.evt = TP_EVT_IMUCOMMAND, .evt_imucommand = {.imu_command=cmd}};
    //transport_evt_cb(evt);
}

void ns_api_hook_get_imu(ns_gyrodata_s *out)
{
    imu_access_safe((imu_data_s *) out);
}

void ns_api_hook_get_quaternion(ns_quaternion_s *out)
{
    imu_quaternion_access_safe(out);
}

void ns_api_hook_set_haptic_packet_raw(ns_haptics_packet_raw_s *packet)
{
    haptics_set_ns_hd(packet);
}

bool core_switch_init(core_params_s *params)
{
    _core_switch_params = params;

    ns_device_config_s cfg = {0};
    cfg.type = NS_DEVTYPE_PROCON;

    if(switchpair_config->switchpair_config_version != CFG_BLOCK_SWITCHPAIR_VERSION)
    {
        switchpair_config->switchpair_config_version = CFG_BLOCK_SWITCHPAIR_VERSION;
        memset(switchpair_config->link_key, 0, sizeof(switchpair_config->link_key));
    }

    bool usb_transport = false;

    switch(params->transport_type)
    {
        case GAMEPAD_TRANSPORT_USB:
        _core_switch_report_size = 64; // 63 data + 1 report ID
        cfg.transport = NS_TRANSPORT_USB;
        usb_transport = true;
        params->core_pollrate_us = 8000;
        break;

        case GAMEPAD_TRANSPORT_BLUETOOTH:
        _core_switch_report_size = 49; // 48 data + 1 report ID
        cfg.transport = NS_TRANSPORT_BTC;
        params->core_pollrate_us = 8000;
        break;

        /*
         * HOJA WLAN dongle mode: the dongle owns USB to the console. NS-LIB is
         * configured as USB (same as Pico-W-NS-Example ns_wlan.c); transport_wlan
         * tunnels the 64-byte Switch reports over Wi-Fi.
         */
        case GAMEPAD_TRANSPORT_WLAN:
        _core_switch_report_size = 64;
        cfg.transport = NS_TRANSPORT_USB;
        usb_transport = true;
        params->core_pollrate_us = 8000;
        break;

        default:
        return false;
    }

    cfg.colors.body.hex = gamepad_config->gamepad_color_body;
    cfg.colors.l_grip.hex = gamepad_config->gamepad_color_grip_left;
    cfg.colors.r_grip.hex = gamepad_config->gamepad_color_grip_right;
    cfg.colors.buttons.hex = gamepad_config->gamepad_color_buttons;

    cfg.gyro_full_scale_dps = 2000.0f;
    cfg.gyro_rad_per_lsb = 0.0f;

    memcpy(cfg.device_serial, user_config->user_name, sizeof(cfg.device_serial));

    memcpy(cfg.device_mac, params->transport_dev_mac, 6);
    // Load persisted host MAC before NS-LIB init. transport_init also copies this
    // into params later, but ns_api_init must see the saved MAC for USB pairing.
    memcpy(params->transport_host_mac, gamepad_config->host_mac_switch, 6);
    memcpy(cfg.host_mac, params->transport_host_mac, 6);

    if (ns_api_init(&cfg) != NS_CONFIG_OK)
    {
        return false;
    }

    if (!_core_switch_populate_hid_device(usb_transport))
    {
        return false;
    }

    params->hid_device = &_core_switch_hid_device;

    // Re-init PCM
    pcm_init(-1);

    params->core_report_format = CORE_REPORTFORMAT_SWPRO;
    params->core_report_generator = _core_switch_get_generated_report;
    params->core_report_tunnel = ns_api_output_tunnel;

    return transport_init(params);
}

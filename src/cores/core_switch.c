#include <string.h>
#include <ns_lib.h>

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

#include "usb/webusb.h"

#include "devices_shared_types.h"

#define CORE_SWITCH_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

#define CORE_SWITCH_HID_NAME "Pro Controller"
static const uint16_t _core_switch_hid_pid = 0x2009;
static const uint16_t _core_switch_hid_vid = 0x057E;

static const hoja_usb_device_descriptor_t _core_switch_device_descriptor = {
    .bLength = sizeof(hoja_usb_device_descriptor_t),
    .bDescriptorType = HUSB_DESC_DEVICE,
    .bcdUSB = 0x0210, // Changed from 0x0200 to 2.1 for BOS & WebUSB
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = 64,
    .idVendor = _core_switch_hid_vid,
    .idProduct = _core_switch_hid_pid,

    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

static const uint8_t _core_switch_hid_report_descriptor_usb[203] = {
    0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
    0x15, 0x00, // Logical Minimum (0)

    0x09, 0x04, // Usage (Joystick)
    0xA1, 0x01, // Collection (Application)

    0x85, 0x30, //   Report ID (48)
    0x05, 0x01, //   Usage Page (Generic Desktop Ctrls)
    0x05, 0x09, //   Usage Page (Button)
    0x19, 0x01, //   Usage Minimum (0x01)
    0x29, 0x0A, //   Usage Maximum (0x0A)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x0A, //   Report Count (10)
    0x55, 0x00, //   Unit Exponent (0)
    0x65, 0x00, //   Unit (None)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09, //   Usage Page (Button)
    0x19, 0x0B, //   Usage Minimum (0x0B)
    0x29, 0x0E, //   Usage Maximum (0x0E)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x04, //   Report Count (4)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x02, //   Report Count (2)
    0x81, 0x03, //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0x0B, 0x01, 0x00, 0x01, 0x00, //   Usage (0x010001)
    0xA1, 0x00,                   //   Collection (Physical)
    0x0B, 0x30, 0x00, 0x01, 0x00, //     Usage (0x010030)
    0x0B, 0x31, 0x00, 0x01, 0x00, //     Usage (0x010031)
    0x0B, 0x32, 0x00, 0x01, 0x00, //     Usage (0x010032)
    0x0B, 0x35, 0x00, 0x01, 0x00, //     Usage (0x010035)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //     Logical Maximum (65534)
    0x75, 0x10,                   //     Report Size (16)
    0x95, 0x04,                   //     Report Count (4)
    0x81, 0x02,                   //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,                         //   End Collection

    0x0B, 0x39, 0x00, 0x01, 0x00, //   Usage (0x010039)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x07,                   //   Logical Maximum (7)
    0x35, 0x00,                   //   Physical Minimum (0)
    0x46, 0x3B, 0x01,             //   Physical Maximum (315)
    0x65, 0x14,                   //   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,                   //   Usage Page (Button)
    0x19, 0x0F,                   //   Usage Minimum (0x0F)
    0x29, 0x12,                   //   Usage Maximum (0x12)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x04,                   //   Report Count (4)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x34,                   //   Report Count (52)
    0x81, 0x03,                   //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
    0x85, 0x21,       //   Report ID (33)
    0x09, 0x01,       //   Usage (0x01)
    0x75, 0x08,       //   Report Size (8)
    0x95, 0x3F,       //   Report Count (63)
    0x81, 0x03,       //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0x85, 0x81, //   Report ID (-127)
    0x09, 0x02, //   Usage (0x02)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x3F, //   Report Count (63)
    0x81, 0x03, //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0x85, 0x01, //   Report ID (1)
    0x09, 0x03, //   Usage (0x03)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x3F, //   Report Count (63)
    0x91, 0x83, //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)

    0x85, 0x10, //   Report ID (16)
    0x09, 0x04, //   Usage (0x04)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x3F, //   Report Count (63)
    0x91, 0x83, //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)

    0x85, 0x80, //   Report ID (-128)
    0x09, 0x05, //   Usage (0x05)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x3F, //   Report Count (63)
    0x91, 0x83, //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)

    0x85, 0x82, //   Report ID (-126)
    0x09, 0x06, //   Usage (0x06)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x3F, //   Report Count (63)
    0x91, 0x83, //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)

    0xC0, // End Collection

    // 203 bytes
};

static const uint8_t _core_switch_hid_report_descriptor_bt[170] = {
    0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,       // Usage (Game Pad)
    0xA1, 0x01,       // Collection (Application)
    0x06, 0x01, 0xFF, //   Usage Page (Vendor Defined 0xFF01)

    0x85, 0x21, //   Report ID (33)
    0x09, 0x21, //   Usage (0x21)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x30, //   Report Count (48)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                //   Position)

    0x85, 0x30, //   Report ID (48)
    0x09, 0x30, //   Usage (0x30)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x30, //   Report Count (48)
    0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                //   Position)

    0x85, 0x31,       //   Report ID (49)
    0x09, 0x31,       //   Usage (0x31)
    0x75, 0x08,       //   Report Size (8)
    0x96, 0x69, 0x01, //   Report Count (361)
    0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                      //   Position)

    0x85, 0x32,       //   Report ID (50)
    0x09, 0x32,       //   Usage (0x32)
    0x75, 0x08,       //   Report Size (8)
    0x96, 0x69, 0x01, //   Report Count (361)
    0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                      //   Position)

    0x85, 0x33,       //   Report ID (51)
    0x09, 0x33,       //   Usage (0x33)
    0x75, 0x08,       //   Report Size (8)
    0x96, 0x69, 0x01, //   Report Count (361)
    0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                      //   Position)

    0x85, 0x3F,                   //   Report ID (63)
    0x05, 0x09,                   //   Usage Page (Button)
    0x19, 0x01,                   //   Usage Minimum (0x01)
    0x29, 0x10,                   //   Usage Maximum (0x10)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x10,                   //   Report Count (16)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                                  //   Position)
    0x05, 0x01,                   //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x39,                   //   Usage (Hat switch)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x07,                   //   Logical Maximum (7)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x42,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null
                                  //   State)
    0x05, 0x09,                   //   Usage Page (Button)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x01,                   //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No
                                  //   Null Position)
    0x05, 0x01,                   //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,                   //   Usage (X)
    0x09, 0x31,                   //   Usage (Y)
    0x09, 0x33,                   //   Usage (Rx)
    0x09, 0x34,                   //   Usage (Ry)
    0x16, 0x00, 0x00,             //   Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //   Logical Maximum (65534)
    0x75, 0x10,                   //   Report Size (16)
    0x95, 0x04,                   //   Report Count (4)
    0x81, 0x02,                   //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
                                  //   Position)
    0x06, 0x01, 0xFF,             //   Usage Page (Vendor Defined 0xFF01)

    0x85, 0x01, //   Report ID (1)
    0x09, 0x01, //   Usage (0x01)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x30, //   Report Count (48)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No
                //   Null Position,Non-volatile)

    0x85, 0x10, //   Report ID (16)
    0x09, 0x10, //   Usage (0x10)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x09, //   Report Count (9)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No
                //   Null Position,Non-volatile)

    0x85, 0x11, //   Report ID (17)
    0x09, 0x11, //   Usage (0x11)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x30, //   Report Count (48)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No
                //   Null Position,Non-volatile)

    0x85, 0x12, //   Report ID (18)
    0x09, 0x12, //   Usage (0x12)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x30, //   Report Count (48)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No
                //   Null Position,Non-volatile)
    0xC0,       // End Collection
};

#define CORE_SWITCH_CONFIG_DESCRIPTOR_LEN 64
const uint8_t _core_switch_config_descriptor[CORE_SWITCH_CONFIG_DESCRIPTOR_LEN] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    HUSB_CONFIG_DESCRIPTOR(1, 2, 0, CORE_SWITCH_CONFIG_DESCRIPTOR_LEN, HUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 250),

    // Interface
    9,
    HUSB_DESC_INTERFACE,
    0x00,
    0x00,
    0x02,
    HUSB_CLASS_HID,
    0x00,
    0x00,
    0x00,
    // HID Descriptor
    9,
    HHID_DESC_TYPE_HID,
    HUSB_U16_TO_U8S_LE(0x0111),
    0,
    1,
    HHID_DESC_TYPE_REPORT,
    HUSB_U16_TO_U8S_LE(sizeof(_core_switch_hid_report_descriptor_usb)),
    // Endpoint Descriptor
    7,
    HUSB_DESC_ENDPOINT,
    0x81,
    HUSB_XFER_INTERRUPT,
    HUSB_U16_TO_U8S_LE(64),
    1, // interval
    // Endpoint Descriptor
    7,
    HUSB_DESC_ENDPOINT,
    0x01,
    HUSB_XFER_INTERRUPT,
    HUSB_U16_TO_U8S_LE(64),
    1, // report interval

    // Alternate Interface for WebUSB
    // Interface
    9,
    HUSB_DESC_INTERFACE,
    0x01,
    0x00,
    0x02,
    HUSB_CLASS_VENDOR_SPECIFIC,
    0x00,
    0x00,
    0x00,
    // Endpoint Descriptor
    7,
    HUSB_DESC_ENDPOINT,
    0x82,
    HUSB_XFER_BULK,
    HUSB_U16_TO_U8S_LE(64),
    0,
    // Endpoint Descriptor
    7,
    HUSB_DESC_ENDPOINT,
    0x02,
    HUSB_XFER_BULK,
    HUSB_U16_TO_U8S_LE(64),
    0,
};

static core_hid_device_t _core_switch_hid_device_usb = {
    .name = CORE_SWITCH_HID_NAME,
    .pid = _core_switch_hid_pid,
    .vid = _core_switch_hid_vid,

    .device_descriptor = &_core_switch_device_descriptor,

    .hid_report_descriptor = _core_switch_hid_report_descriptor_usb,
    .hid_report_descriptor_len = sizeof(_core_switch_hid_report_descriptor_usb),

    .config_descriptor = _core_switch_config_descriptor,
    .config_descriptor_len = CORE_SWITCH_CONFIG_DESCRIPTOR_LEN,
};

static core_hid_device_t _core_switch_hid_device_bt = {
    .name = CORE_SWITCH_HID_NAME,
    .pid = _core_switch_hid_pid,
    .vid = _core_switch_hid_vid,

    .device_descriptor = &_core_switch_device_descriptor,

    .hid_report_descriptor = _core_switch_hid_report_descriptor_bt,
    .hid_report_descriptor_len = sizeof(_core_switch_hid_report_descriptor_bt),

    .config_descriptor = _core_switch_config_descriptor,
    .config_descriptor_len = CORE_SWITCH_CONFIG_DESCRIPTOR_LEN,
};

static core_params_s *_core_switch_params = NULL;
static uint8_t _core_switch_report_size = 64;

bool _core_switch_get_generated_report(core_report_s *out)
{
    if (ns_api_generate_inputreport(out->data))
    {
        out->reportformat = CORE_REPORTFORMAT_SWPRO;
        out->size = _core_switch_report_size;

        out->reliable = out->data != NS_INPUT_REPORT_ID_FULL;
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
    settings_commit_blocks();
}

void ns_api_hook_set_imu_mode(ns_imu_mode_t imu_mode)
{
    //tp_imucommand_t cmd;
    switch(imu_mode)
    {
        default:
        case NS_IMU_OFF:
        _core_switch_params->sys_gyro_task = NULL;
        break;

        case NS_IMU_RAW:
        _core_switch_params->sys_gyro_task = imu_forced_task_standard;
        break;

        case NS_IMU_QUAT:
        _core_switch_params->sys_gyro_task = imu_forced_task_quaternion;
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

    switch(params->transport_type)
    {
        case GAMEPAD_TRANSPORT_USB:
        _core_switch_report_size = 64; // 63 data + 1 report ID
        cfg.transport = NS_TRANSPORT_USB;
        params->hid_device = &_core_switch_hid_device_usb;
        params->core_pollrate_us = 8000;
        break;

        case GAMEPAD_TRANSPORT_BLUETOOTH:
        _core_switch_report_size = 49; // 48 data + 1 report ID
        cfg.transport = NS_TRANSPORT_BTC;
        params->hid_device = &_core_switch_hid_device_bt;
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
        params->hid_device = &_core_switch_hid_device_usb;
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

    memcpy(cfg.device_mac, params->transport_dev_mac, 6);
    // Load persisted host MAC before NS-LIB init. transport_init also copies this
    // into params later, but ns_api_init must see the saved MAC for USB pairing.
    memcpy(params->transport_host_mac, gamepad_config->host_mac_switch, 6);
    memcpy(cfg.host_mac, params->transport_host_mac, 6);

    if (ns_api_init(&cfg) != NS_CONFIG_OK)
    {
        return false;
    }

    // Re-init PCM
    pcm_init(-1);

    params->sys_gyro_task = imu_forced_task_standard;

    params->core_report_format = CORE_REPORTFORMAT_SWPRO;
    params->core_report_generator = _core_switch_get_generated_report;
    params->core_report_tunnel = ns_api_output_tunnel;

    return transport_init(params);
}

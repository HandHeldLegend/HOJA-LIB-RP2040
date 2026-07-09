#include "board_config.h"

#if defined(HOJA_TRANSPORT_USB_DRIVER) && (HOJA_TRANSPORT_USB_DRIVER == USB_DRIVER_HAL)

#include <stdio.h>

#include "transport/transport_usb.h"

#include "cores/cores.h"

#include "utilities/settings.h"
#include "utilities/tasks.h"

#include "hal/sys_hal.h"

#include "usb/webusb.h"

#include "hoja.h"

#include "hhl_tusb.h"

// WebUSB landing URL the browser offers when the device is plugged in. Defaults
// to the HOJA config web app; a board may override it by defining
// HOJA_WEBUSB_URL in its board_config.h (optional, rarely needed).
#if defined(HOJA_WEBUSB_URL)
#define USB_WEBUSB_URL HOJA_WEBUSB_URL
#else
#define USB_WEBUSB_URL "handheldlegend.github.io/hoja2"
#endif

core_params_s *_usb_core_params = NULL;
const core_hid_device_t *_usbhal_hiddev = NULL;

// USB string index 3; must outlive enumeration.
static char _usb_serial_str[13];

static void _usb_hal_set_serial_from_mac(const uint8_t mac[6])
{
    snprintf(_usb_serial_str, sizeof(_usb_serial_str),
             "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

//--------------------------------------------------------------------+
// USB poll-rate scheduling (registered as HHL SOF / mount hooks)
//--------------------------------------------------------------------+

volatile uint8_t sequence_step = 0;
volatile uint8_t ms_counter = 0;
uint32_t _usb_frames = 8;
volatile bool _usb_ready = false;
volatile bool _usb_sendit = false;
volatile uint8_t _usb_sent_id = 0xFF;

static bool _usb_hal_mounted = false;
static void _usb_hal_hid_output_report(const uint8_t *buffer, uint16_t len)
{
    if (_usb_core_params != NULL && _usb_core_params->core_report_tunnel != NULL)
    {
        _usb_core_params->core_report_tunnel(buffer, len);
    }
}

static void _usb_hal_on_mount(void)
{
    _usb_hal_mounted = true;
}

static void _usb_hal_on_sof(uint32_t frame_count)
{
    switch (_usb_frames)
    {
    case 1:
        // Align the task cycle to each USB frame so input is sampled before send.
        tasks_mark_sent_isr();
        _usb_sendit = true;
        break;

    case 8:
        ms_counter++;
        {
            uint8_t trigger_threshold = (sequence_step == 2) ? 8 : 7;
            if (ms_counter >= trigger_threshold)
            {
                ms_counter = 0;
                sequence_step = (sequence_step + 1) % 3;
                _usb_sendit = true;
            }
        }
        break;

    default:
        ms_counter++;
        if (ms_counter >= _usb_frames)
        {
            ms_counter = 0;
            _usb_sendit = true;
        }
        break;
    }
}

//--------------------------------------------------------------------+
// Transport
//--------------------------------------------------------------------+

static void _usb_hal_webusb_rx(const uint8_t *data, uint16_t len)
{
    webusb_command_handler((uint8_t *)data, len);
    hhl_tusb_task();
}

void transport_usb_stop()
{
    _usb_hal_mounted = false;
    hhl_tusb_stop();
}

core_report_s _core_report = {0};

bool transport_usb_init(core_params_s *params)
{
    _usb_core_params = params;
    _usb_hal_mounted = false;
    memset(_core_report.data, 0, 64);

    if (_usb_core_params->hid_device)
    {
        _usbhal_hiddev = _usb_core_params->hid_device;
    }
    else
    {
        return false;
    }

    const hoja_config_s *cfg = hoja_config_get();

    hhl_tusb_config_s tcfg = {0};
    tcfg.strings.manufacturer = (cfg && cfg->device_maker) ? cfg->device_maker : "HOJA";
    tcfg.strings.product      = (cfg && cfg->device_name)  ? cfg->device_name  : "Gamepad";
    _usb_hal_set_serial_from_mac(params->transport_dev_mac);
    tcfg.strings.serial_number = _usb_serial_str;

    tcfg.hooks.vendor_rx = _usb_hal_webusb_rx;
    tcfg.hooks.platform_sleep_ms = sys_hal_sleep_ms;
    tcfg.hooks.hid_output_report = _usb_hal_hid_output_report;
    tcfg.hooks.on_mount = _usb_hal_on_mount;
    tcfg.hooks.on_sof = _usb_hal_on_sof;

    switch (_usb_core_params->core_report_format)
    {
    case CORE_REPORTFORMAT_SINPUT:
        _usb_frames = 1;
        tcfg.driver = HHL_TUSB_DRIVER_HID;
        tcfg.webusb.enabled = true;
        tcfg.webusb.url = USB_WEBUSB_URL;
        tcfg.webusb.popup_enable_flag = &gamepad_config->webusb_enable_popup;
        break;

    case CORE_REPORTFORMAT_SWPRO:
        _usb_frames = 8;
        tcfg.driver = HHL_TUSB_DRIVER_HID;
        tcfg.webusb.enabled = true;
        tcfg.webusb.url = USB_WEBUSB_URL;
        tcfg.webusb.popup_enable_flag = &gamepad_config->webusb_enable_popup;
        break;

    case CORE_REPORTFORMAT_XINPUT:
#if (HHL_TUSB_DRIVER_XINPUT_ENABLE)
        _usb_frames = 1;
        tcfg.driver = HHL_TUSB_DRIVER_XINPUT;
        break;
#else
        return false;
#endif

    case CORE_REPORTFORMAT_SLIPPI:
#if (HHL_TUSB_DRIVER_SLIPPI_ENABLE)
        _usb_frames = 1;
        tcfg.driver = HHL_TUSB_DRIVER_SLIPPI;
        break;
#else
        return false;
#endif

    default:
        return false;
    }

    if (tcfg.driver == HHL_TUSB_DRIVER_HID)
    {
        tcfg.hid.device_descriptor = (const uint8_t *)_usbhal_hiddev->device_descriptor;
        tcfg.hid.device_descriptor_len = HHL_TUSB_STD_DEVICE_DESC_LEN;
        tcfg.hid.config_descriptor = _usbhal_hiddev->config_descriptor;
        tcfg.hid.config_descriptor_len = _usbhal_hiddev->config_descriptor_len;
        tcfg.hid.report_descriptor = _usbhal_hiddev->hid_report_descriptor;
        tcfg.hid.report_descriptor_len = _usbhal_hiddev->hid_report_descriptor_len;
        tcfg.hid.max_power_ma = _usbhal_hiddev->max_power_ma;
    }

    hhl_tusb_init(&tcfg);

    return hhl_tusb_start();
}

void transport_usb_task(uint64_t timestamp)
{
    hhl_tusb_task();

    if (!_usb_hal_mounted)
    {
        return;
    }

    if (webusb_outputting_check())
    {
        webusb_send_rawinput(timestamp);
        hhl_tusb_task();
        return;
    }

    if (!_usb_ready)
    {
        _usb_ready = hhl_tusb_report_ready();
    }

    if (_usb_sendit && _usb_ready)
    {
        if (!tasks_get_required_done())
        {
            return;
        }

        _usb_sendit = false;
        _usb_ready = false;

        if (core_get_generated_report(&_core_report))
        {
            if (hhl_tusb_report_send(_core_report.data[0], &_core_report.data[1], _core_report.size - 1))
            {
                if (_usb_frames != 1)
                {
                    tasks_mark_sent();
                }
                hhl_tusb_task();
            }
        }
    }
}

#endif

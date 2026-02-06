#include "board_config.h"

#if defined(HOJA_TRANSPORT_USB_DRIVER) && (HOJA_TRANSPORT_USB_DRIVER == USB_DRIVER_HAL)

#include <hoja_usb.h>
#include <tusb_config.h>

#include "transport/transport_usb.h"

#include "cores/cores.h"

#include "hardware/structs/usb.h"

#include "bsp/board.h"
#include "tusb.h"
#include "device/usbd_pvt.h"

#include "utilities/settings.h"
#include "hal/sys_hal.h"

#include "usb/webusb.h"

#if !defined(HOJA_MANUFACTURER)
#define USB_MANUFACTURER "HOJA"
#else
#define USB_MANUFACTURER HOJA_MANUFACTURER
#endif

#if !defined(HOJA_PRODUCT)
#define USB_PRODUCT "Gamepad"
#else
#define USB_PRODUCT HOJA_PRODUCT
#endif

const char *global_string_descriptor[] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    USB_MANUFACTURER,     // 1: Manufacturer
    USB_PRODUCT,          // 2: Product
    "000000",             // 3: Serials, should use chip ID
    "Hoja Gamepad"        // 4 Identifier for GC Mode
};

typedef bool (*usb_hal_report_cb_t)(uint8_t report_id, void const *report, uint16_t len);
typedef bool (*usb_hal_ready_cb_t)(void);

usb_hal_report_cb_t _usb_hal_report_cb = NULL;
usb_hal_ready_cb_t _usb_hal_ready_cb = NULL;
core_params_s *_usb_core_params = NULL;
const core_hid_device_t *_usbhal_hiddev = NULL;

//--------------------------------------------------------------------+
// Slippi Types Pre-Defines
//--------------------------------------------------------------------+
#pragma region SLIPPI

#define CFG_TUD_GC_TX_BUFSIZE 37
#define CFG_TUD_GC_RX_BUFSIZE 6
typedef struct
{
    uint8_t itf_num;
    uint8_t ep_in;
    uint8_t ep_out;       // optional Out endpoint
    uint8_t itf_protocol; // Boot mouse or keyboard

    uint8_t protocol_mode; // Boot (0) or Report protocol (1)
    uint8_t idle_rate;     // up to application to handle idle rate
    uint16_t report_desc_len;

    CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_GC_TX_BUFSIZE];
    CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_GC_RX_BUFSIZE];

    // TODO save hid descriptor since host can specifically request this after enumeration
    // Note: HID descriptor may be not available from application after enumeration
    tusb_hid_descriptor_hid_t const *hid_descriptor;
} slippid_interface_t;

CFG_TUSB_MEM_SECTION static slippid_interface_t _slippid_itf[1];

/*------------- Helpers -------------*/
static inline uint8_t slippi_get_index_by_itfnum(uint8_t itf_num)
{
    for (uint8_t i = 0; i < CFG_TUD_GC; i++)
    {
        if (itf_num == _slippid_itf[i].itf_num)
            return i;
    }

    return 0xFF;
}

//--------------------------------------------------------------------+
// APPLICATION API
//--------------------------------------------------------------------+
bool tud_slippi_n_ready(uint8_t instance)
{
    uint8_t const rhport = 0;
    uint8_t const ep_in = _slippid_itf[instance].ep_in;
    return tud_ready() && (ep_in != 0) && !usbd_edpt_busy(rhport, ep_in);
}

bool tud_slippi_ready()
{
    return tud_slippi_n_ready(0);
}

bool tud_slippi_n_report(uint8_t instance, uint8_t report_id, void const *report, uint16_t len)
{
    uint8_t const rhport = 0;
    slippid_interface_t *p_hid = &_slippid_itf[instance];

    // claim endpoint
    TU_VERIFY(usbd_edpt_claim(rhport, p_hid->ep_in));

    // prepare data
    if (report_id)
    {
        len = tu_min16(len, CFG_TUD_GC_TX_BUFSIZE - 1);

        p_hid->epin_buf[0] = report_id;
        memcpy(p_hid->epin_buf + 1, report, len);
        len++;
    }
    else
    {
        // If report id = 0, skip ID field
        len = tu_min16(len, CFG_TUD_GC_TX_BUFSIZE);
        memcpy(p_hid->epin_buf, report, len);
    }

    return usbd_edpt_xfer(rhport, p_hid->ep_in, p_hid->epin_buf, len);
}

bool tud_slippi_report(uint8_t report_id, void const *report, uint16_t len)
{
    return tud_slippi_n_report(0, report_id, report, len);
}

uint8_t tud_slippi_n_interface_protocol(uint8_t instance)
{
    return _slippid_itf[instance].itf_protocol;
}

uint8_t tud_slippi_n_get_protocol(uint8_t instance)
{
    return _slippid_itf[instance].protocol_mode;
}

//--------------------------------------------------------------------+
// USBD-CLASS API
//--------------------------------------------------------------------+
void slippid_reset(uint8_t rhport)
{
    (void)rhport;
    tu_memclr(_slippid_itf, sizeof(_slippid_itf));
}

void slippid_init(void)
{
    slippid_reset(0);
}

uint16_t slippid_open(uint8_t rhport, tusb_desc_interface_t const *desc_itf, uint16_t max_len)
{
    // Do not open if we aren't in Slippi reporting mode
    if (_usb_core_params)
    {
        if (_usb_core_params->core_report_format != CORE_REPORTFORMAT_SINPUT)
            return 0;
    }

    // len = interface + hid + n*endpoints
    uint16_t const drv_len = (uint16_t)(sizeof(tusb_desc_interface_t) + sizeof(tusb_hid_descriptor_hid_t) +
                                        desc_itf->bNumEndpoints * sizeof(tusb_desc_endpoint_t));
    TU_ASSERT(max_len >= drv_len, 0);

    // Find available interface
    slippid_interface_t *p_hid = NULL;
    uint8_t hid_id;
    for (hid_id = 0; hid_id < CFG_TUD_GC; hid_id++)
    {
        if (_slippid_itf[hid_id].ep_in == 0)
        {
            p_hid = &_slippid_itf[hid_id];
            break;
        }
    }
    TU_ASSERT(p_hid, 0);

    uint8_t const *p_desc = (uint8_t const *)desc_itf;

    //------------- HID descriptor -------------//
    p_desc = tu_desc_next(p_desc);
    TU_ASSERT(HID_DESC_TYPE_HID == tu_desc_type(p_desc), 0);
    p_hid->hid_descriptor = (tusb_hid_descriptor_hid_t const *)p_desc;

    //------------- Endpoint Descriptor -------------//
    p_desc = tu_desc_next(p_desc);
    TU_ASSERT(usbd_open_edpt_pair(rhport, p_desc, desc_itf->bNumEndpoints, TUSB_XFER_INTERRUPT, &p_hid->ep_out, &p_hid->ep_in), 0);

    if (desc_itf->bInterfaceSubClass == HID_SUBCLASS_BOOT)
        p_hid->itf_protocol = desc_itf->bInterfaceProtocol;

    p_hid->protocol_mode = HID_PROTOCOL_REPORT; // Per Specs: default is report mode
    p_hid->itf_num = desc_itf->bInterfaceNumber;

    // Use offsetof to avoid pointer to the odd/misaligned address
    p_hid->report_desc_len = tu_unaligned_read16((uint8_t const *)p_hid->hid_descriptor + offsetof(tusb_hid_descriptor_hid_t, wReportLength));

    // Prepare for output endpoint
    if (p_hid->ep_out)
    {
        if (!usbd_edpt_xfer(rhport, p_hid->ep_out, p_hid->epout_buf, sizeof(p_hid->epout_buf)))
        {
            TU_LOG_FAILED();
            TU_BREAKPOINT();
        }
    }

    return drv_len;
}

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool slippid_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    TU_VERIFY(request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE);

    uint8_t const hid_itf = slippi_get_index_by_itfnum((uint8_t)request->wIndex);
    TU_VERIFY(hid_itf < CFG_TUD_GC);

    slippid_interface_t *p_hid = &_slippid_itf[hid_itf];

    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD)
    {
        //------------- STD Request -------------//
        if (stage == CONTROL_STAGE_SETUP)
        {
            uint8_t const desc_type = tu_u16_high(request->wValue);
            // uint8_t const desc_index = tu_u16_low (request->wValue);

            if (request->bRequest == TUSB_REQ_GET_DESCRIPTOR && desc_type == HID_DESC_TYPE_HID)
            {
                TU_VERIFY(p_hid->hid_descriptor);
                TU_VERIFY(tud_control_xfer(rhport, request, (void *)(uintptr_t)p_hid->hid_descriptor, p_hid->hid_descriptor->bLength));
            }
            else if (request->bRequest == TUSB_REQ_GET_DESCRIPTOR && desc_type == HID_DESC_TYPE_REPORT)
            {
                uint8_t const *desc_report = tud_hid_descriptor_report_cb(hid_itf);
                tud_control_xfer(rhport, request, (void *)(uintptr_t)desc_report, p_hid->report_desc_len);
            }
            else
            {
                return false; // stall unsupported request
            }
        }
    }
    else if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS)
    {
        //------------- Class Specific Request -------------//
        switch (request->bRequest)
        {
        case HID_REQ_CONTROL_GET_REPORT:
            if (stage == CONTROL_STAGE_SETUP)
            {
                uint8_t const report_type = tu_u16_high(request->wValue);
                uint8_t const report_id = tu_u16_low(request->wValue);

                uint8_t *report_buf = p_hid->epin_buf;
                uint16_t req_len = tu_min16(request->wLength, CFG_TUD_GC_TX_BUFSIZE);

                uint16_t xferlen = 0;

                // If host request a specific Report ID, add ID to as 1 byte of response
                if ((report_id != HID_REPORT_TYPE_INVALID) && (req_len > 1))
                {
                    *report_buf++ = report_id;
                    req_len--;

                    xferlen++;
                }

                xferlen += tud_hid_get_report_cb(hid_itf, report_id, (hid_report_type_t)report_type, report_buf, req_len);
                TU_ASSERT(xferlen > 0);

                tud_control_xfer(rhport, request, p_hid->epin_buf, xferlen);
            }
            break;

        case HID_REQ_CONTROL_SET_REPORT:
            if (stage == CONTROL_STAGE_SETUP)
            {
                TU_VERIFY(request->wLength <= sizeof(p_hid->epout_buf));
                tud_control_xfer(rhport, request, p_hid->epout_buf, request->wLength);
            }
            else if (stage == CONTROL_STAGE_ACK)
            {
                uint8_t const report_type = tu_u16_high(request->wValue);
                uint8_t const report_id = tu_u16_low(request->wValue);

                uint8_t const *report_buf = p_hid->epout_buf;
                uint16_t report_len = tu_min16(request->wLength, CFG_TUD_GC_RX_BUFSIZE);

                // If host request a specific Report ID, extract report ID in buffer before invoking callback
                if ((report_id != HID_REPORT_TYPE_INVALID) && (report_len > 1) && (report_id == report_buf[0]))
                {
                    report_buf++;
                    report_len--;
                }

                tud_hid_set_report_cb(hid_itf, report_id, (hid_report_type_t)report_type, report_buf, report_len);
            }
            break;

        case HID_REQ_CONTROL_SET_IDLE:
            if (stage == CONTROL_STAGE_SETUP)
            {
                p_hid->idle_rate = tu_u16_high(request->wValue);
                if (tud_hid_set_idle_cb)
                {
                    // stall request if callback return false
                    TU_VERIFY(tud_hid_set_idle_cb(hid_itf, p_hid->idle_rate));
                }

                tud_control_status(rhport, request);
            }
            break;

        case HID_REQ_CONTROL_GET_IDLE:
            if (stage == CONTROL_STAGE_SETUP)
            {
                // TODO idle rate of report
                tud_control_xfer(rhport, request, &p_hid->idle_rate, 1);
            }
            break;

        case HID_REQ_CONTROL_GET_PROTOCOL:
            if (stage == CONTROL_STAGE_SETUP)
            {
                tud_control_xfer(rhport, request, &p_hid->protocol_mode, 1);
            }
            break;

        case HID_REQ_CONTROL_SET_PROTOCOL:
            if (stage == CONTROL_STAGE_SETUP)
            {
                tud_control_status(rhport, request);
            }
            else if (stage == CONTROL_STAGE_ACK)
            {
                p_hid->protocol_mode = (uint8_t)request->wValue;
                if (tud_hid_set_protocol_cb)
                {
                    tud_hid_set_protocol_cb(hid_itf, p_hid->protocol_mode);
                }
            }
            break;

        default:
            return false; // stall unsupported request
        }
    }
    else
    {
        return false; // stall unsupported request
    }

    return true;
}

bool slippid_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    (void)result;

    uint8_t instance = 0;
    slippid_interface_t *p_hid = _slippid_itf;

    // Identify which interface to use
    for (instance = 0; instance < CFG_TUD_GC; instance++)
    {
        p_hid = &_slippid_itf[instance];
        if ((ep_addr == p_hid->ep_out) || (ep_addr == p_hid->ep_in))
            break;
    }
    TU_ASSERT(instance < CFG_TUD_GC);

    // Sent report successfully
    if (ep_addr == p_hid->ep_in)
    {
        if (tud_hid_report_complete_cb)
        {
            tud_hid_report_complete_cb(instance, p_hid->epin_buf, (uint16_t)xferred_bytes);
        }
    }
    // Received report
    else if (ep_addr == p_hid->ep_out)
    {
        tud_hid_set_report_cb(instance, 0, HID_REPORT_TYPE_INVALID, p_hid->epout_buf, (uint16_t)xferred_bytes);
        TU_ASSERT(usbd_edpt_xfer(rhport, p_hid->ep_out, p_hid->epout_buf, sizeof(p_hid->epout_buf)));
    }

    return true;
}

const usbd_class_driver_t tud_slippi_driver =
    {
#if CFG_TUSB_DEBUG >= 2
        .name = "slippi",
#endif
        .init = slippid_init,
        .reset = slippid_reset,
        .open = slippid_open,
        .control_xfer_cb = slippid_control_xfer_cb,
        .xfer_cb = slippid_xfer_cb,
        .sof = NULL,
};
#pragma endregion

//--------------------------------------------------------------------+
// XInput Types Pre-Defines
//--------------------------------------------------------------------+
#pragma region XINPUT

#define CFG_TUD_XINPUT_EP_BUFSIZE 64
typedef struct
{
    uint8_t itf_num;
    uint8_t ep_in;
    uint8_t ep_out; // optional Out endpoint

    CFG_TUSB_MEM_ALIGN uint8_t epin_buf[CFG_TUD_XINPUT_EP_BUFSIZE];
    CFG_TUSB_MEM_ALIGN uint8_t epout_buf[CFG_TUD_XINPUT_EP_BUFSIZE];

    // TODO save hid descriptor since host can specifically request this after enumeration
    // Note: HID descriptor may be not available from application after enumeration
    // tusb_xinput_descriptor_hid_t const * hid_descriptor;
} xinputd_interface_t;

CFG_TUSB_MEM_SECTION static xinputd_interface_t _xinputd_itf;

void xinputd_reset(uint8_t rhport)
{
    (void)rhport;
    tu_memclr(&_xinputd_itf, sizeof(_xinputd_itf));
}

void xinputd_init(void)
{
    xinputd_reset(0);
}

uint16_t xinputd_open(uint8_t rhport, tusb_desc_interface_t const *desc_itf, uint16_t max_len)
{
    const char *TAG = "xinputd_open";
    // Verify our descriptor is the correct class
    TU_VERIFY(0x5D == desc_itf->bInterfaceSubClass, 0);

    // len = interface + hid + n*endpoints
    uint16_t const drv_len = (uint16_t)(sizeof(tusb_desc_interface_t) +
                                        desc_itf->bNumEndpoints * sizeof(tusb_desc_endpoint_t)) +
                             16;

    TU_ASSERT(max_len >= drv_len, 0);

    uint8_t const *p_desc = tu_desc_next(desc_itf);
    uint8_t total_endpoints = 0;
    while ((total_endpoints < desc_itf->bNumEndpoints) && (drv_len <= max_len))
    {
        tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *)p_desc;
        if (TUSB_DESC_ENDPOINT == tu_desc_type(desc_ep))
        {
            TU_ASSERT(usbd_edpt_open(rhport, desc_ep));

            if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN)
            {
                _xinputd_itf.ep_in = desc_ep->bEndpointAddress;
            }
            else
            {
                _xinputd_itf.ep_out = desc_ep->bEndpointAddress;
            }
            total_endpoints += 1;
        }
        p_desc = tu_desc_next(p_desc);
    }

    return drv_len;
}

bool xinputd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    return true;
}

bool xinputd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    (void)result;

    uint8_t instance = 0;

    // Sent report successfully
    if (ep_addr == _xinputd_itf.ep_in)
    {
        if (tud_hid_report_complete_cb)
        {
            tud_hid_report_complete_cb(instance, _xinputd_itf.epin_buf, (uint16_t)xferred_bytes);
        }
    }
    // Received report
    else if (ep_addr == _xinputd_itf.ep_out)
    {
        tud_hid_set_report_cb(instance, 0, HID_REPORT_TYPE_INVALID, _xinputd_itf.epout_buf, (uint16_t)xferred_bytes);
        TU_ASSERT(usbd_edpt_xfer(rhport, _xinputd_itf.ep_out, _xinputd_itf.epout_buf, sizeof(_xinputd_itf.epout_buf)));
    }

    return true;
}

void tud_xinput_getout(void)
{
    if (tud_ready() && (!usbd_edpt_busy(0, _xinputd_itf.ep_out)))
    {
        usbd_edpt_claim(0, _xinputd_itf.ep_out);
        usbd_edpt_xfer(0, _xinputd_itf.ep_out, _xinputd_itf.epout_buf, sizeof(_xinputd_itf.epout_buf));
        usbd_edpt_release(0, _xinputd_itf.ep_out);
    }
}

// USER API ACCESSIBLE
bool tud_n_xinput_report(uint8_t report_id, void const *report, uint16_t len)
{
    uint8_t const rhport = 0;

    // Remote wakeup
    if (tud_suspended())
    {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    }

    // claim endpoint
    TU_VERIFY(usbd_edpt_claim(rhport, 0x81));

    _xinputd_itf.epin_buf[0] = report_id;
    memcpy(&_xinputd_itf.epin_buf[1], report, CFG_TUD_XINPUT_EP_BUFSIZE - 1);
    bool out = usbd_edpt_xfer(rhport, _xinputd_itf.ep_in, _xinputd_itf.epin_buf, CFG_TUD_XINPUT_EP_BUFSIZE);
    usbd_edpt_release(0, _xinputd_itf.ep_in);

    tud_xinput_getout();

    return out;
}

bool tud_xinput_report(uint8_t report_id, void const *report, uint16_t len)
{
    return tud_n_xinput_report(report_id, report, len);
}

bool tud_xinput_ready(void)
{
    uint8_t const rhport = 0;
    uint8_t const ep_in = _xinputd_itf.ep_in;
    return tud_ready() && (ep_in != 0) && !usbd_edpt_busy(rhport, ep_in);
}

const usbd_class_driver_t tud_xinput_driver =
    {
#if CFG_TUSB_DEBUG >= 2
        .name = "XINPUT",
#endif
        .init = xinputd_init,
        .reset = xinputd_reset,
        .open = xinputd_open,
        .control_xfer_cb = xinputd_control_xfer_cb,
        .xfer_cb = xinputd_xfer_cb,
        .sof = NULL,
};

#pragma endregion

//--------------------------------------------------------------------+
// TinyUSB Driver Get Definition
//--------------------------------------------------------------------+
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
    *driver_count += 1;

    if (_usb_core_params)
    {
        switch (_usb_core_params->core_report_format)
        {
        case CORE_REPORTFORMAT_SLIPPI:
            return &tud_slippi_driver;

        // Default to XInput
        default:
            return &tud_xinput_driver;
        }
    }
}

//--------------------------------------------------------------------+
// Windows/WebUSB Descriptor Handlers
//--------------------------------------------------------------------+
#pragma region MS_OS_DESC

static uint16_t _desc_str[64];

enum
{
    VENDOR_REQUEST_WEBUSB = 1,
    VENDOR_REQUEST_MICROSOFT = 2
};

#define ITF_NUM_VENDOR 1
#define GC_ITF_NUM_VENDOR 0
#define VENDOR_REQUEST_GET_MS_OS_DESCRIPTOR 7

extern uint8_t const desc_bos[];
extern uint8_t const desc_ms_os_20[];
extern uint8_t const gc_desc_ms_os_20[];

// Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
// Define the MS OS 1.0 Descriptor
static const uint8_t MS_OS_Descriptor[] = {
    0x12,                                                                               // Descriptor length (18 bytes)
    0x03,                                                                               // Descriptor type (3 = String)
    0x4D, 0x00, 0x53, 0x00, 0x46, 0x00, 0x54, 0x00, 0x31, 0x00, 0x30, 0x00, 0x30, 0x00, // Signature: "MSFT100"
    VENDOR_REQUEST_GET_MS_OS_DESCRIPTOR,                                                // Vendor Code
    0x00                                                                                // Padding
};

// Size of the uint16_t array
#define SIZE_UINT16_ARRAY (sizeof(MS_OS_Descriptor) / 2)

static uint16_t MS_OS_Descriptor_LE_UINT16[SIZE_UINT16_ARRAY];

//--------------------------------------------------------------------+
// BOS Descriptor
//--------------------------------------------------------------------+

/* Microsoft OS 2.0 registry property descriptor
Per MS requirements https://msdn.microsoft.com/en-us/library/windows/hardware/hh450799(v=vs.85).aspx
device should create DeviceInterfaceGUIDs. It can be done by driver and
in case of real PnP solution device should expose MS "Microsoft OS 2.0
registry property descriptor". Such descriptor can insert any record
into Windows registry per device/configuration/interface. In our case it
will insert "DeviceInterfaceGUIDs" multistring property.

GUID is freshly generated and should be OK to use.

https://developers.google.com/web/fundamentals/native-hardware/build-for-webusb/
(Section Microsoft OS compatibility descriptors)
*/

#define BOS_TOTAL_LEN (TUD_BOS_DESC_LEN + TUD_BOS_WEBUSB_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)
#define GC_BOS_TOTAL_LEN (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

#define MS_OS_20_DESC_LEN 0xB2
#define GC_MS_OS_20_DESC_LEN 158

// BOS Descriptor is required for webUSB
uint8_t const desc_bos[] = {
    // total length, number of device caps
    TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 2),

    // Vendor Code, iLandingPage
    TUD_BOS_WEBUSB_DESCRIPTOR(VENDOR_REQUEST_WEBUSB, 1),

    // Microsoft OS 2.0 descriptor
    TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MICROSOFT)};

uint8_t const gc_desc_bos[] = {
    // BOS descriptor
    0x05,       // Descriptor size (5 bytes)
    0x0F,       // Descriptor type (BOS)
    0x21, 0x00, // Length of this + subordinate descriptors
                // (33 bytes)
    0x01,       // Number of subordinate descriptors

    // Microsoft OS 2.0 Platform Capability Descriptor

    0x1C, // Descriptor size (28 bytes)
    0x10, // Descriptor type (Device Capability)
    0x05, // Capability type (Platform)
    0x00, // Reserved

    // MS OS 2.0 Platform Capability ID (D8DD60DF-4589-4CC7-9CD2-659D9E648A9F)

    0xDF, 0x60, 0xDD, 0xD8,
    0x89, 0x45,
    0xC7, 0x4C,
    0x9C, 0xD2,
    0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,

    0x00, 0x00, 0x03, 0x06,   // Windows version (8.1) (0x06030000)
    0x9E, 0x00,               // Size, MS OS 2.0 descriptor set (158 bytes)
    VENDOR_REQUEST_MICROSOFT, // Vendor-assigned bMS_VendorCode
    0x00                      // Doesn’t support alternate enumeration
};

uint8_t const *tud_descriptor_bos_cb(void)
{
    if(_usb_core_params)
    {
        switch(_usb_core_params->core_report_format)
        {
            case CORE_REPORTFORMAT_SLIPPI:
            return gc_desc_bos;

            default:
            return desc_bos;
        }
    }
    return 0;
}

uint8_t const desc_ms_os_20[] =
    {
        // Set header: length, type, windows version, total length
        U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

        // Configuration subset header: length, type, configuration index, reserved, configuration total length
        U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), 0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A),

        // Function Subset header: length, type, first interface, reserved, subset length
        U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), ITF_NUM_VENDOR, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x08),

        // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
        U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // sub-compatible

        // MS OS 2.0 Registry property descriptor: length, type
        U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x08 - 0x08 - 0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
        U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A), // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
        'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
        'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
        U16_TO_U8S_LE(0x0050), // wPropertyDataLength
                               // bPropertyData: “{8B3E9D2E-7EEC-4994-AAE7-0C40DE84D36D}”.
        '{', 0x00, '8', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, '9', 0x00, 'D', 0x00, '2', 0x00, 'E', 0x00, '-', 0x00,
        '7', 0x00, 'E', 0x00, 'E', 0x00, 'C', 0x00, '-', 0x00, '4', 0x00, '9', 0x00, '9', 0x00, '4', 0x00, '-', 0x00,
        'A', 0x00, 'A', 0x00, 'E', 0x00, '7', 0x00, '-', 0x00, '0', 0x00, 'C', 0x00, '4', 0x00, '0', 0x00, 'D', 0x00,
        'E', 0x00, '8', 0x00, '4', 0x00, 'D', 0x00, '3', 0x00, '6', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00};

uint8_t const tmp[] =
    {
        U16_TO_U8S_LE(GC_MS_OS_20_DESC_LEN - 0x0A - 0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
        U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A), // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
        'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
        'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
        U16_TO_U8S_LE(0x0050), // wPropertyDataLength
                               // bPropertyData: “{8B3E9D2E-7EEC-4994-AAE7-0C40DE84D36D}”.
        '{', 0x00, '9', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, '9', 0x00, 'D', 0x00, '2', 0x00, 'E', 0x00, '-', 0x00,
        '7', 0x00, 'E', 0x00, 'E', 0x00, 'C', 0x00, '-', 0x00, '4', 0x00, '9', 0x00, '9', 0x00, '4', 0x00, '-', 0x00,
        'A', 0x00, 'A', 0x00, 'E', 0x00, '7', 0x00, '-', 0x00, '0', 0x00, 'C', 0x00, '4', 0x00, '0', 0x00, 'D', 0x00,
        'E', 0x00, '8', 0x00, '4', 0x00, 'D', 0x00, '3', 0x00, '6', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00};

uint8_t const gc_desc_ms_os_20[] =
    {
        0x0A, 0x00,             // Descriptor size (10 bytes)
        0x00, 0x00,             // MS OS 2.0 descriptor set header
        0x00, 0x00, 0x03, 0x06, // Windows version (8.1) (0x06030000)
        0x9E, 0x00,             // Size, MS OS 2.0 descriptor set (158 bytes)

        // Microsoft OS 2.0 compatible ID descriptor

        0x14, 0x00,                                     // Descriptor size (20 bytes)
        0x03, 0x00,                                     // MS OS 2.0 compatible ID descriptor
        0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00, // WINUSB string
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Sub-compatible ID

        // Registry property descriptor

        0x80, 0x00, // Descriptor size (130 bytes)
        0x04, 0x00, // Registry Property descriptor
        0x01, 0x00, // Strings are null-terminated Unicode
        0x28, 0x00, // Size of Property Name (40 bytes)

        // Property Name ("DeviceInterfaceGUID")

        0x44, 0x00, 0x65, 0x00, 0x76, 0x00, 0x69, 0x00, 0x63, 0x00, 0x65, 0x00,
        0x49, 0x00, 0x6E, 0x00, 0x74, 0x00, 0x65, 0x00, 0x72, 0x00, 0x66, 0x00,
        0x61, 0x00, 0x63, 0x00, 0x65, 0x00, 0x47, 0x00, 0x55, 0x00, 0x49, 0x00,
        0x44, 0x00, 0x00, 0x00,

        0x4E, 0x00, // Size of Property Data (78 bytes)

        // Vendor-defined Property Data: {ecceff35-146c-4ff3-acd9-8f992d09acdd}

        0x7B, 0x00, 0x65, 0x00, 0x63, 0x00, 0x63, 0x00, 0x65, 0x00, 0x66, 0x00,
        0x66, 0x00, 0x33, 0x00, 0x35, 0x00, 0x2D, 0x00, 0x31, 0x00, 0x34, 0x00,
        0x36, 0x00, 0x33, 0x00, 0x2D, 0x00, 0x34, 0x00, 0x66, 0x00, 0x66, 0x00,
        0x33, 0x00, 0x2D, 0x00, 0x61, 0x00, 0x63, 0x00, 0x64, 0x00, 0x39, 0x00,
        0x2D, 0x00, 0x38, 0x00, 0x66, 0x00, 0x39, 0x00, 0x39, 0x00, 0x32, 0x00,
        0x64, 0x00, 0x30, 0x00, 0x39, 0x00, 0x61, 0x00, 0x63, 0x00, 0x64, 0x00,
        0x64, 0x00, 0x7D, 0x00, 0x00, 0x00};

TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "Incorrect size");
TU_VERIFY_STATIC(sizeof(gc_desc_ms_os_20) == GC_MS_OS_20_DESC_LEN, "Incorrect size");

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;

    uint8_t chr_count;

    if (index == 0xEE)
    {
        for (int i = 0, j = 0; i < sizeof(MS_OS_Descriptor); i += 2, j++)
        {
            MS_OS_Descriptor_LE_UINT16[j] = (MS_OS_Descriptor[i + 1] << 8) | MS_OS_Descriptor[i];
        }

        memcpy(&_desc_str[0], &MS_OS_Descriptor_LE_UINT16[0], sizeof(MS_OS_Descriptor_LE_UINT16));
        return _desc_str;
    }
    else if (index == 0)
    {
        memcpy(&_desc_str[1], global_string_descriptor[0], 2);
        chr_count = 1;
    }
    else
    {

        const char *str = global_string_descriptor[index];

        // Cap at max char... WHY?
        chr_count = strlen(str);
        if (chr_count > 31)
            chr_count = 31;

        // Convert ASCII string into UTF-16
        for (uint8_t i = 0; i < chr_count; i++)
        {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return _desc_str;
}

// Vendor Device Class CB for receiving data
void tud_vendor_rx_cb(uint8_t itf, uint8_t const *buffer, uint16_t bufsize)
{
    sys_hal_tick();

    uint8_t nuBuffer[64];

    uint32_t size = tud_vendor_n_read(0, nuBuffer, 64);
    tud_vendor_n_read_flush(0);

    // if (hoja_gamepad_mode_get() == GAMEPAD_MODE_SWPRO)
    {
        // printf("WebUSB Data Received.\n");
        webusb_command_handler(nuBuffer, size);
    }
}

const tusb_desc_webusb_url_t desc_url =
    {
        .bLength = 3 + sizeof(HOJA_WEBUSB_URL) - 1,
        .bDescriptorType = 3, // WEBUSB URL type
        .bScheme = 1,         // 0: http, 1: https
        .url = HOJA_WEBUSB_URL};

uint8_t MS_OS_10_CompatibleID_Descriptor[] = {
    0x28, 0x00, 0x00, 0x00,                         // DWORD (LE)	 Descriptor length (40 bytes)
    0x00, 0x01,                                     // BCD WORD (LE)	 Version ('1.0')
    0x04, 0x00,                                     // WORD (LE)	 Compatibility ID Descriptor index (0x0004)
    0x01,                                           // BYTE	 Number of sections (1)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       // 7 BYTES	 Reserved
    0x00,                                           //	 BYTE	 Interface Number (Interface #0)
    0x01,                                           //	 BYTE	 Reserved
    0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00, // 8 BYTES ASCII String Compatible ID ("WINUSB\0\0")
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 8 BYTES ASCII String	 Sub-Compatible ID (unused)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00              // 6 BYTES Reserved
};

uint8_t MS_Extended_Feature_Descriptor[] =
    {
        0x92, 0x00, 0x00, 0x00, // DWORD (LE)	 Descriptor length (146 bytes)
        0x00, 0x01,             // BCD WORD (LE) Version ('1.0')
        0x05, 0x00,             // WORD (LE)	 Extended Property Descriptor index (0x0005)
        0x01, 0x00,             // WORD          Number of sections (1)
        0x88, 0x00, 0x00, 0x00, // DWORD (LE)	 Size of the property section (136 bytes)
        0x07, 0x00, 0x00, 0x00, // DWORD (LE)	 Property data type (7 = Unicode REG_MULTI_SZ)
        0x2A, 0x00,             // WORD (LE)	 Property name length (42 bytes)
                                // NULL-terminated Unicode String (LE)	 Property Name (L"DeviceInterfaceGUIDs")
        'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0,
        'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0,
        'G', 0, 'U', 0, 'I', 0, 'D', 0, 's', 0, 0x00, 0x00,
        0x50, 0x00, 0x00, 0x00, // DWORD (LE)	 Property data length (80 bytes)

        // NULL-terminated Unicode String (LE), followed by another Unicode NULL
        // Property Name ("{6E45736A-2B1B-4078-B772-B3AF2B6FDE1C}")
        '{', 0, '6', 0, 'E', 0, '4', 0, '5', 0, '7', 0, '3', 0, '6', 0, 'A', 0, '-', 0,
        '2', 0, 'B', 0, '1', 0, 'B', 0, '-', 0, '4', 0, '0', 0, '7', 0, '8', 0, '-', 0,
        'B', 0, '7', 0, '7', 0, '2', 0, '-', 0, 'B', 0, '3', 0, 'A', 0, 'F', 0, '2', 0,
        'B', 0, '6', 0, 'F', 0, 'D', 0, 'E', 0, '1', 0, 'C', 0, '}', 0,
        0x00, 0x00, 0x00, 0x00};

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    // nothing to with DATA & ACK stage
    if (stage != CONTROL_STAGE_SETUP)
        return true;

    uint8_t const desc_type = tu_u16_high(request->wValue);
    uint8_t const itf = 0;

    switch (request->bmRequestType_bit.type)
    {
    case TUSB_REQ_TYPE_STANDARD:
        // Unused for vendor control transfer
        // TinyUSB hooks in and forces this for Vendor requests only

    case TUSB_REQ_TYPE_VENDOR:
        switch (request->bRequest)
        {

        // MS OS 1.0 Descriptor
        case VENDOR_REQUEST_GET_MS_OS_DESCRIPTOR:
        {
            if (request->wIndex == 4)
            {
                if (tud_control_xfer(rhport, request, MS_OS_10_CompatibleID_Descriptor, sizeof(MS_OS_10_CompatibleID_Descriptor)))
                {
                    return true;
                }

                return false;
            }
            else if (request->wIndex == 5)
            {
                // MS descriptor 1.0 stuff

                if (tud_control_xfer(rhport, request, MS_Extended_Feature_Descriptor, sizeof(MS_Extended_Feature_Descriptor)))
                {
                    return true;
                }
            }
        }
        break;

        // Web USB Descriptor
        case VENDOR_REQUEST_WEBUSB:
        {
            if (gamepad_config->webusb_enable_popup == 1)
                // match vendor request in BOS descriptor
                // Get landing page url
                return tud_control_xfer(rhport, request, (void *)(uintptr_t)&desc_url, desc_url.bLength);
            else
                return false;
        }

        // MS OS 2.0 Descriptor
        case VENDOR_REQUEST_MICROSOFT:
        {
            if (request->wIndex == 7)
            {
                // Get Microsoft OS 2.0 compatible descriptor
                uint16_t total_len;

                if (_usb_core_params)
                {
                    if (_usb_core_params->core_report_format == CORE_REPORTFORMAT_SLIPPI)
                    {
                        memcpy(&total_len, gc_desc_ms_os_20 + 8, 2);
                        return tud_control_xfer(rhport, request, (void *)(uintptr_t)gc_desc_ms_os_20, total_len);
                    }
                    else
                    {
                        memcpy(&total_len, desc_ms_os_20 + 8, 2);
                        return tud_control_xfer(rhport, request, (void *)(uintptr_t)desc_ms_os_20, total_len);
                    }
                }
            }
            else
            {
                return false;
            }
        }

        default:
            break;
        }
        break;

    case TUSB_REQ_TYPE_CLASS:
        printf("Vendor Request: %x", request->bRequest);

        // response with status OK
        return tud_control_status(rhport, request);
        break;

    default:
        break;
    }

    // stall unknown request
    return false;
}

#pragma endregion

//--------------------------------------------------------------------+
// TinyUSB Function Callbacks
//--------------------------------------------------------------------+
#pragma region TUSB CALLBACKS

/***********************************************/
/********* TinyUSB HID callbacks ***************/

volatile uint8_t sequence_step = 0;
volatile uint8_t ms_counter = 0;
uint32_t _usb_frames = 8;
// Whether USB is ready for another input
volatile bool _usb_ready = false;
volatile bool _usb_sendit = false;

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void)
{
    // TO DO set connected on transport layer
    // END TO DO
    if (_usbhal_hiddev)
    {
        return (uint8_t const *)_usbhal_hiddev->device_descriptor;
    }

    return NULL;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    if (_usbhal_hiddev)
    {
        return _usbhal_hiddev->config_descriptor;
    }

    return 0;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)reqlen;
    (void)report_type;

    return 0;
}

// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;

    if (_usbhal_hiddev)
    {
        if(_usbhal_hiddev->hid_report_descriptor)
            return _usbhal_hiddev->hid_report_descriptor;
    }

    return 0;
}

// Invoked when report complete
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len)
{
    (void)instance;
    (void)report;
    (void)len;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    if (!report_id && report_type == HID_REPORT_TYPE_OUTPUT)
    {
        if (_usb_core_params->core_report_tunnel)
        {
            _usb_core_params->core_report_tunnel(buffer, bufsize);
        }
    }
}

void tud_mount_cb()
{
    tud_sof_cb_enable(false);
    tud_sof_cb_enable(true);
}

void tud_sof_cb(uint32_t frame_count_ext) 
{
    switch (_usb_frames)
    {
        case 1:
            _usb_sendit = true;
            break;

        case 8:
            ms_counter++;

            // To hit 8, 8, 9 intervals, we trigger at 7, 7, 8
            uint8_t trigger_threshold = (sequence_step == 2) ? 8 : 7;

            if (ms_counter >= trigger_threshold) {
                // Reset counters
                ms_counter = 0;
                sequence_step = (sequence_step + 1) % 3;
                _usb_sendit = true;
            }
            break;
    }
}

#pragma endregion

/***********************************************/
/********* Transport Defines *******************/

void transport_usb_stop()
{
    _usb_hal_report_cb = NULL;
    tud_deinit(0);
}
core_report_s _core_report = {0};

bool transport_usb_init(core_params_s *params)
{
    // Copy pointer
    _usb_core_params = params;
    memset(_core_report.data, 0, 64);

    switch (_usb_core_params->core_report_format)
    {
    // Supported report formats
    case CORE_REPORTFORMAT_SINPUT:
        _usb_frames = 1;
        _usb_hal_ready_cb = tud_hid_ready;
        _usb_hal_report_cb = tud_hid_report;
        break;
    case CORE_REPORTFORMAT_SWPRO:
        _usb_frames = 8;
        _usb_hal_ready_cb = tud_hid_ready;
        _usb_hal_report_cb = tud_hid_report;
        break;

    case CORE_REPORTFORMAT_XINPUT:
        _usb_frames = 1;
        _usb_hal_ready_cb = tud_xinput_ready;
        _usb_hal_report_cb = tud_xinput_report;
        break;

    case CORE_REPORTFORMAT_SLIPPI:
        _usb_frames = 1;
        _usb_hal_ready_cb = tud_slippi_ready;
        _usb_hal_report_cb = tud_slippi_report;
        break;

    // Unsupported report formats
    default:
        return false;
    }

    if (_usb_core_params->hid_device)
    {
        _usbhal_hiddev = _usb_core_params->hid_device;
    }
    // We need the USB device properties
    else
        return false;

    return tusb_init();
}

void transport_usb_task(uint64_t timestamp)
{
    static bool sofen = false;
    tud_task();

    if (!_usb_ready && (_usb_hal_ready_cb!=NULL))
    {
        _usb_ready = _usb_hal_ready_cb();
    }

    if (_usb_sendit && _usb_ready)
    {
        _usb_sendit = false;
        _usb_ready = false;
    
        
        if(core_get_generated_report(&_core_report))
        {
            if(_usb_hal_report_cb)
            {
                _usb_hal_report_cb(_core_report.data[0], &_core_report.data[1], _core_report.size-1);
            }
        }
    }
}

#endif
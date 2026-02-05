#include "board_config.h"

#if defined(HOJA_TRANSPORT_USB_DRIVER) && (HOJA_TRANSPORT_USB_DRIVER == USB_DRIVER_HAL)

#include <hoja_usb.h>

#include "hal/usb_hal.h"

#include "cores/cores.h"

#include "hardware/structs/usb.h"

#include "tusb.h"


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

core_params_s *_usb_core_params = NULL;
core_hid_device_t *_usbhal_hiddev = NULL;

uint32_t usb_hal_sof()
{
    return usb_hw->sof_rd;
}

/***********************************************/
/********* TinyUSB HID callbacks ***************/

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void)
{
    // TO DO set connected on transport layer
    // END TO DO

    if (_usbhal_hiddev)
    {

        return _usbhal_hiddev->device_descriptor;
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

    return NULL;
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

        return _usbhal_hiddev->hid_report_descriptor;
    }
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
        if (_usb_core_params->report_tunnel)
        {
            _usb_core_params->report_tunnel(buffer, bufsize);
        }
    }
}

static uint16_t _desc_str[64];

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

/***********************************************/
/********* Transport Defines *******************/
bool transport_usb_init(core_params_s *params)
{
    _usb_core_params = params;

    if (_usb_core_params)
    {
        if (_usb_core_params->hid_device != NULL)
        {
            _usbhal_hiddev = _usb_core_params->hid_device;
        }
    }
}

void transport_usb_task(uint64_t timestamp)
{

}

#endif
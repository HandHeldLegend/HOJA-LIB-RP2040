#include "desc_bos.h"

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
    TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MICROSOFT)
};

uint8_t const gc_desc_bos[] = {
    // BOS descriptor
    0x05,				  // Descriptor size (5 bytes)
    0x0F,       	// Descriptor type (BOS)
    0x21, 0x00,   // Length of this + subordinate descriptors		
                            // (33 bytes)
    0x01,					// Number of subordinate descriptors

    // Microsoft OS 2.0 Platform Capability Descriptor

    0x1C,					// Descriptor size (28 bytes)
    0x10,					// Descriptor type (Device Capability)
    0x05,					// Capability type (Platform)
    0x00,					// Reserved

    // MS OS 2.0 Platform Capability ID (D8DD60DF-4589-4CC7-9CD2-659D9E648A9F)

    0xDF, 0x60, 0xDD, 0xD8, 
    0x89, 0x45,
    0xC7, 0x4C,
    0x9C, 0xD2, 
    0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,

    0x00, 0x00, 0x03, 0x06,		// Windows version (8.1) (0x06030000)
    0x9E, 0x00, 			       	// Size, MS OS 2.0 descriptor set (158 bytes)
    VENDOR_REQUEST_MICROSOFT,           // Vendor-assigned bMS_VendorCode
    0x00					            // Doesn’t support alternate enumeration
};

uint8_t const *tud_descriptor_bos_cb(void)
{
    if (hoja_comms_current_mode() == INPUT_MODE_GCUSB) return gc_desc_bos;

    return desc_bos;
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
    'E', 0x00, '8', 0x00, '4', 0x00, 'D', 0x00, '3', 0x00, '6', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
};

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
        'E', 0x00, '8', 0x00, '4', 0x00, 'D', 0x00, '3', 0x00, '6', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
};

uint8_t const gc_desc_ms_os_20[] =
{
    0x0A, 0x00,					// Descriptor size (10 bytes)
    0x00, 0x00,					// MS OS 2.0 descriptor set header
    0x00, 0x00, 0x03, 0x06,					// Windows version (8.1) (0x06030000)
    0x9E, 0x00,					// Size, MS OS 2.0 descriptor set (158 bytes)

    // Microsoft OS 2.0 compatible ID descriptor

    0x14, 0x00,						// Descriptor size (20 bytes)
    0x03, 0x00,			 		  // MS OS 2.0 compatible ID descriptor
    0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00,			// WINUSB string
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,			// Sub-compatible ID

    // Registry property descriptor

    0x80, 0x00,				// Descriptor size (130 bytes)
    0x04, 0x00,				// Registry Property descriptor
    0x01, 0x00,				// Strings are null-terminated Unicode
    0x28, 0x00,				// Size of Property Name (40 bytes)

    //Property Name ("DeviceInterfaceGUID")

    0x44, 0x00, 0x65, 0x00, 0x76, 0x00, 0x69, 0x00, 0x63, 0x00, 0x65, 0x00,
    0x49, 0x00, 0x6E, 0x00, 0x74, 0x00, 0x65, 0x00, 0x72, 0x00, 0x66, 0x00,
    0x61, 0x00, 0x63, 0x00, 0x65, 0x00, 0x47, 0x00, 0x55, 0x00, 0x49, 0x00,
    0x44, 0x00, 0x00, 0x00, 

    0x4E, 0x00,				// Size of Property Data (78 bytes)

    // Vendor-defined Property Data: {ecceff35-146c-4ff3-acd9-8f992d09acdd}

    0x7B, 0x00, 0x65, 0x00, 0x63, 0x00, 0x63, 0x00, 0x65, 0x00, 0x66, 0x00,
    0x66, 0x00, 0x33, 0x00, 0x35, 0x00, 0x2D, 0x00, 0x31, 0x00, 0x34, 0x00,
    0x36, 0x00, 0x33, 0x00, 0x2D, 0x00, 0x34, 0x00, 0x66, 0x00, 0x66, 0x00,
    0x33, 0x00, 0x2D, 0x00, 0x61, 0x00, 0x63, 0x00, 0x64, 0x00, 0x39, 0x00,
    0x2D, 0x00, 0x38, 0x00, 0x66, 0x00, 0x39, 0x00, 0x39, 0x00, 0x32, 0x00,
    0x64, 0x00, 0x30, 0x00, 0x39, 0x00, 0x61, 0x00, 0x63, 0x00, 0x64, 0x00,
    0x64, 0x00, 0x7D, 0x00, 0x00, 0x00
};


TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "Incorrect size");
TU_VERIFY_STATIC(sizeof(gc_desc_ms_os_20) == GC_MS_OS_20_DESC_LEN, "Incorrect size");

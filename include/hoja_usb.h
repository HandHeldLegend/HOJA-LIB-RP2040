#ifndef HOJA_USB_H
#define HOJA_USB_H

#include <stdint.h>

/* --- Utility Helpers --- */
#define HUSB_ARRAY_SIZE(_arr)          (sizeof(_arr) / sizeof((_arr)[0]))
#define HUSB_MIN(_x, _y)               (((_x) < (_y)) ? (_x) : (_y))
#define HUSB_MAX(_x, _y)               (((_x) > (_y)) ? (_x) : (_y))
#define HUSB_DIV_CEIL(_n, _d)          (((_n) + (_d) - 1) / (_d))

/* --- 16-bit Byte Manipulation --- */
#define HUSB_U16(_high, _low)          ((uint16_t)(((uint8_t)(_high) << 8) | (uint8_t)(_low)))
#define HUSB_U16_HIGH(_u16)            ((uint8_t)(((uint16_t)(_u16) >> 8) & 0x00FF))
#define HUSB_U16_LOW(_u16)             ((uint8_t)((uint16_t)(_u16) & 0x00FF))

/* Endian-specific splits for descriptors */
#define HUSB_U16_TO_U8S_BE(_u16)       HUSB_U16_HIGH(_u16), HUSB_U16_LOW(_u16)
#define HUSB_U16_TO_U8S_LE(_u16)       HUSB_U16_LOW(_u16), HUSB_U16_HIGH(_u16)

/* --- 32-bit Byte Manipulation --- */
#define HUSB_U32_BYTE3(_u32)           ((uint8_t)((((uint32_t)(_u32)) >> 24) & 0x000000FF))
#define HUSB_U32_BYTE2(_u32)           ((uint8_t)((((uint32_t)(_u32)) >> 16) & 0x000000FF))
#define HUSB_U32_BYTE1(_u32)           ((uint8_t)((((uint32_t)(_u32)) >> 8) & 0x000000FF))
#define HUSB_U32_BYTE0(_u32)           ((uint8_t)(((uint32_t)(_u32)) & 0x000000FF))

#define HUSB_U32_TO_U8S_BE(_u32)       HUSB_U32_BYTE3(_u32), HUSB_U32_BYTE2(_u32), HUSB_U32_BYTE1(_u32), HUSB_U32_BYTE0(_u32)
#define HUSB_U32_TO_U8S_LE(_u32)       HUSB_U32_BYTE0(_u32), HUSB_U32_BYTE1(_u32), HUSB_U32_BYTE2(_u32), HUSB_U32_BYTE3(_u32)

/* --- Bitwise Operations --- */
#define HUSB_BIT(_n)                   (1UL << (_n))

/* Generates a bitmask from high bit (h) to low bit (l) */
#define HUSB_GENMASK(_h, _l)           (((~0UL) << (_l)) & (~0UL >> (31 - (_h))))

/// USB Device Descriptor
typedef struct __attribute__ ((packed)) {
    uint8_t  bLength            ; ///< Size of this descriptor in bytes.
    uint8_t  bDescriptorType    ; ///< DEVICE Descriptor Type.
    uint16_t bcdUSB             ; ///< BUSB Specification Release Number in Binary-Coded Decimal (i.e., 2.10 is 210H).

    uint8_t  bDeviceClass       ; ///< Class code (assigned by the USB-IF).
    uint8_t  bDeviceSubClass    ; ///< Subclass code (assigned by the USB-IF).
    uint8_t  bDeviceProtocol    ; ///< Protocol code (assigned by the USB-IF).
    uint8_t  bMaxPacketSize0    ; ///< Maximum packet size for endpoint zero (only 8, 16, 32, or 64 are valid). For HS devices is fixed to 64.

    uint16_t idVendor           ; ///< Vendor ID (assigned by the USB-IF).
    uint16_t idProduct          ; ///< Product ID (assigned by the manufacturer).
    uint16_t bcdDevice          ; ///< Device release number in binary-coded decimal.
    uint8_t  iManufacturer      ; ///< Index of string descriptor describing manufacturer.
    uint8_t  iProduct           ; ///< Index of string descriptor describing product.
    uint8_t  iSerialNumber      ; ///< Index of string descriptor describing the device's serial number.

    uint8_t  bNumConfigurations ; ///< Number of possible configurations.
} hoja_usb_device_descriptor_t;

/// USB Descriptor Types
typedef enum {
  HUSB_DESC_DEVICE                = 0x01,
  HUSB_DESC_CONFIGURATION         = 0x02,
  HUSB_DESC_STRING                = 0x03,
  HUSB_DESC_INTERFACE             = 0x04,
  HUSB_DESC_ENDPOINT              = 0x05,
  HUSB_DESC_DEVICE_QUALIFIER      = 0x06,
  HUSB_DESC_OTHER_SPEED_CONFIG    = 0x07,
  HUSB_DESC_INTERFACE_POWER       = 0x08,
  HUSB_DESC_OTG                   = 0x09,
  HUSB_DESC_DEBUG                 = 0x0A,
  HUSB_DESC_INTERFACE_ASSOCIATION = 0x0B,

  HUSB_DESC_BOS                   = 0x0F,
  HUSB_DESC_DEVICE_CAPABILITY     = 0x10,

  HUSB_DESC_FUNCTIONAL            = 0x21,

  // Class Specific Descriptor
  HUSB_DESC_CS_DEVICE             = 0x21,
  HUSB_DESC_CS_CONFIGURATION      = 0x22,
  HUSB_DESC_CS_STRING             = 0x23,
  HUSB_DESC_CS_INTERFACE          = 0x24,
  HUSB_DESC_CS_ENDPOINT           = 0x25,

  HUSB_DESC_SUPERSPEED_ENDPOINT_COMPANION     = 0x30,
  HUSB_DESC_SUPERSPEED_ISO_ENDPOINT_COMPANION = 0x31
} hoja_usb_desc_type_t;

// https://www.usb.org/defined-class-codes
typedef enum {
  HUSB_CLASS_UNSPECIFIED          = 0    ,
  HUSB_CLASS_AUDIO                = 1    ,
  HUSB_CLASS_CDC                  = 2    ,
  HUSB_CLASS_HID                  = 3    ,
  HUSB_CLASS_RESERVED_4           = 4    ,
  HUSB_CLASS_PHYSICAL             = 5    ,
  HUSB_CLASS_IMAGE                = 6    ,
  HUSB_CLASS_PRINTER              = 7    ,
  HUSB_CLASS_MSC                  = 8    ,
  HUSB_CLASS_HUB                  = 9    ,
  HUSB_CLASS_CDC_DATA             = 10   ,
  HUSB_CLASS_SMART_CARD           = 11   ,
  HUSB_CLASS_RESERVED_12          = 12   ,
  HUSB_CLASS_CONTENT_SECURITY     = 13   ,
  HUSB_CLASS_VIDEO                = 14   ,
  HUSB_CLASS_PERSONAL_HEALTHCARE  = 15   ,
  HUSB_CLASS_AUDIO_VIDEO          = 16   ,

  HUSB_CLASS_DIAGNOSTIC           = 0xDC ,
  HUSB_CLASS_WIRELESS_CONTROLLER  = 0xE0 ,
  HUSB_CLASS_MISC                 = 0xEF ,
  HUSB_CLASS_APPLICATION_SPECIFIC = 0xFE ,
  HUSB_CLASS_VENDOR_SPECIFIC      = 0xFF
} hoja_usb_class_code_t;

/// HID Descriptor Type
typedef enum
{
  HHID_DESC_TYPE_HID      = 0x21, ///< HID Descriptor
  HHID_DESC_TYPE_REPORT   = 0x22, ///< Report Descriptor
  HHID_DESC_TYPE_PHYSICAL = 0x23  ///< Physical Descriptor
} hoja_hid_descriptor_enum_t;

/// defined base on USB Specs Endpoint's bmAttributes
typedef enum {
  HUSB_XFER_CONTROL     = 0,
  HUSB_XFER_ISOCHRONOUS = 1,
  HUSB_XFER_BULK        = 2,
  HUSB_XFER_INTERRUPT   = 3
} hoja_usb_xfer_type_t;

enum {
  HUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP = 1u << 5,
  HUSB_DESC_CONFIG_ATT_SELF_POWERED  = 1u << 6,
};

// Config number, interface count, string index, total length, attribute, power in mA
#define HUSB_CONFIG_DESCRIPTOR(config_num, _itfcount, _stridx, _total_len, _attribute, _power_ma) \
  9, 0x02, HUSB_U16_TO_U8S_LE(_total_len), _itfcount, config_num, _stridx, (HUSB_BIT(7) | (_attribute)), ((_power_ma) / 2)

#endif
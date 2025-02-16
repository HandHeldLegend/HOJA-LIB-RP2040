#ifndef USB_SWPRO_H
#define USB_SWPRO_H

#include <stdint.h>
#include <stdbool.h>
#include "utilities/callback.h"

#define REPORT_ID_SWITCH_INPUT 0x30
#define REPORT_ID_SWITCH_CMD 0x21
#define REPORT_ID_SWITCH_INIT 0x81

typedef struct
{
    union
    {
        struct
        {
            // Y and C-Up (N64)
            uint8_t b_y       : 1;

            // X and C-Left (N64)
            uint8_t b_x       : 1;

            uint8_t b_b       : 1;
            uint8_t b_a       : 1;
            uint8_t t_r_sr    : 1;
            uint8_t t_r_sl    : 1;
            uint8_t t_r       : 1;

            // ZR and C-Down (N64)
            uint8_t t_zr      : 1;
        };
        uint8_t right_buttons;
    };
    union
    {
        struct
        {
            // Minus and C-Right (N64)
            uint8_t b_minus     : 1;

            // Plus and Start
            uint8_t b_plus      : 1;

            uint8_t sb_right    : 1;
            uint8_t sb_left     : 1;
            uint8_t b_home      : 1;
            uint8_t b_capture   : 1;
            uint8_t none        : 1;
            uint8_t charge_grip_active : 1;
        };
        uint8_t shared_buttons;
    };
    union
    {
        struct
        {
            uint8_t d_down    : 1;
            uint8_t d_up      : 1;
            uint8_t d_right   : 1;
            uint8_t d_left    : 1;
            uint8_t t_l_sr    : 1;
            uint8_t t_l_sl    : 1;
            uint8_t t_l       : 1;

            // ZL and Z (N64)
            uint8_t t_zl      : 1;

        };
        uint8_t left_buttons;
    };

    uint16_t ls_x;
    uint16_t ls_y;
    uint16_t rs_x;
    uint16_t rs_y;

} sw_input_s;

#define SWPRO_REPORT_SIZE 64-1 // -1 because of report ID

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
  } ext_tusb_desc_device_t;

extern const ext_tusb_desc_device_t swpro_device_descriptor;
extern const uint8_t swpro_hid_report_descriptor[];
extern const uint8_t swpro_configuration_descriptor[];

void swpro_hid_report(uint32_t timestamp, hid_report_tunnel_cb cb);

#endif

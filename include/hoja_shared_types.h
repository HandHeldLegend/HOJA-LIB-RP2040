#ifndef HOJA_SHARED_TYPES_H
#define HOJA_SHARED_TYPES_H

#include "devices_shared_types.h"
#include <stdbool.h>

typedef enum 
{
    CONN_STATUS_SHUTDOWN = -3,
    CONN_STATUS_INIT = -2,
    CONN_STATUS_DISCONNECTED  = -1,
    CONN_STATUS_CONNECTING  = 0,
    CONN_STATUS_PLAYER_1    = 1,
    CONN_STATUS_PLAYER_2    = 2,
    CONN_STATUS_PLAYER_3    = 3,
    CONN_STATUS_PLAYER_4    = 4,
    CONN_STATUS_PLAYER_5    = 5,
    CONN_STATUS_PLAYER_6    = 6,
    CONN_STATUS_PLAYER_7    = 7,
    CONN_STATUS_PLAYER_8    = 8,
} connection_status_t;

typedef enum 
{
    PLAYER_NUMBER_INIT = -1,
    
} player_number_t;

typedef enum
{
    GAMEPAD_MODE_UNDEFINED = -2,
    GAMEPAD_MODE_LOAD     = -1, // Firmware load (bluetooth)
    GAMEPAD_MODE_SWPRO    = 0,
    GAMEPAD_MODE_XINPUT   = 1,
    GAMEPAD_MODE_GCUSB    = 2,
    GAMEPAD_MODE_GAMECUBE = 3,
    GAMEPAD_MODE_N64      = 4,
    GAMEPAD_MODE_SNES     = 5,
    GAMEPAD_MODE_SINPUT   = 6,
    GAMEPAD_MODE_MAX,
} gamepad_mode_t;

typedef enum
{
    GAMEPAD_METHOD_AUTO  = -1, // Automatically determine if we are plugged or wireless
    GAMEPAD_METHOD_WIRED = 0, // Used for modes that should retain power even when unplugged
    GAMEPAD_METHOD_USB   = 1, // Use for USB modes where we should power off when unplugged
    GAMEPAD_METHOD_BLUETOOTH = 2, // Wireless Bluetooth modes
} gamepad_method_t;

typedef struct 
{
    int8_t connection_status;
    gamepad_mode_t gamepad_mode;
    gamepad_method_t gamepad_method;
    bool   init_status;
    rgb_s  notification_color;
    rgb_s  gamepad_color;
    bool   ss_notif_pending; // Single-shot notification pending
    rgb_s  ss_notif_color; // Single-shot notification color
} hoja_status_s;

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

#endif
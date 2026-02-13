#ifndef HOJA_SHARED_TYPES_H
#define HOJA_SHARED_TYPES_H

#include "devices_shared_types.h"
#include <stdbool.h>

typedef enum 
{
    CONNECTION_STATUS_DOWN,
    CONNECTION_STATUS_CONNECTED,
    CONNECTION_STATUS_DISCONNECTED,
} connection_status_t;

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
    GAMEPAD_METHOD_AUTO  = -1,      // Automatically determine if we are plugged or wireless
    GAMEPAD_METHOD_WIRED = 0,       // Used for modes that should retain power even when unplugged
    GAMEPAD_METHOD_USB   = 1,       // Use for USB modes where we should power off when unplugged
    GAMEPAD_METHOD_BLUETOOTH = 2,   // Wireless Bluetooth modes
    GAMEPAD_METHOD_WLAN = 3,        // Wireless WLAN modes (dongle)
} gamepad_method_t;

typedef enum 
{
    GAMEPAD_TRANSPORT_UNDEFINED = -2,
    GAMEPAD_TRANSPORT_AUTO = -1,
    GAMEPAD_TRANSPORT_NESBUS,
    GAMEPAD_TRANSPORT_JOYBUS64,
    GAMEPAD_TRANSPORT_JOYBUSGC,
    GAMEPAD_TRANSPORT_USB,
    GAMEPAD_TRANSPORT_BLUETOOTH,
    GAMEPAD_TRANSPORT_WLAN,
} gamepad_transport_t;

typedef struct 
{
    int8_t connection_status;
    uint8_t player_number;
    gamepad_mode_t gamepad_mode;
    gamepad_method_t gamepad_method;
    bool   init_status;
    rgb_s  notification_color;
    rgb_s  gamepad_color;
    bool   ss_notif_pending; // Single-shot notification pending
    rgb_s  ss_notif_color; // Single-shot notification color
    uint8_t debug_data;
} hoja_status_s;

typedef struct 
{
  uint64_t benchmark;
} interval_task_s;

#endif
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

// Numeric values match gamepadConfig_s.gamepad_default_mode (legacy gamepad_mode_t order).
typedef enum
{
    CORE_REPORTFORMAT_UNDEFINED = -1,
    CORE_REPORTFORMAT_SWPRO       = 0,
    CORE_REPORTFORMAT_XINPUT      = 1,
    CORE_REPORTFORMAT_SLIPPI      = 2,
    CORE_REPORTFORMAT_GAMECUBE    = 3,
    CORE_REPORTFORMAT_N64         = 4,
    CORE_REPORTFORMAT_SNES        = 5,
    CORE_REPORTFORMAT_SINPUT      = 6,
    CORE_REPORTFORMAT_MAX,
} core_reportformat_t;

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
    core_reportformat_t reportformat;
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

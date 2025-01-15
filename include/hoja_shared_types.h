#ifndef HOJA_SHARED_TYPES_H
#define HOJA_SHARED_TYPES_H

#include "devices_shared_types.h"
#include <stdbool.h>

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
    GAMEPAD_MODE_OPENGP   = 6,
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
    int8_t player_number;
    int8_t connection_status;
    gamepad_mode_t gamepad_mode;
    gamepad_method_t gamepad_method;
    bool   init_status;
    rgb_s  notification_color;
    rgb_s  gamepad_color;
} hoja_status_s;

#endif
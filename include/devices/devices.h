#ifndef DEVICES_DEVICES_H
#define DEVICES_DEVICES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    DEVICE_MODE_LOAD     = -1,
    DEVICE_MODE_SWPRO    = 0,
    DEVICE_MODE_XINPUT   = 1,
    DEVICE_MODE_GCUSB    = 2,
    DEVICE_MODE_GAMECUBE = 3,
    DEVICE_MODE_N64      = 4,
    DEVICE_MODE_SNES     = 5,
    DEVICE_MODE_DS4      = 6,
    DEVICE_MODE_MAX,
} device_mode_t;

#define DEVICE_MODE_BASEBANDUPDATE DEVICE_MODE_MAX

typedef enum
{
    DEVICE_METHOD_AUTO  = -1, // Automatically determine if we are plugged or wireless
    DEVICE_METHOD_WIRED = 0, // Used for modes that should retain power even when unplugged
    DEVICE_METHOD_USB   = 1, // Use for USB modes where we should power off when unplugged
    DEVICE_METHOD_BLUETOOTH = 2, // Wireless Bluetooth modes
} device_method_t;

device_mode_t device_mode_get();

void device_init();

void device_task(uint64_t timestamp);

bool device_start(device_mode_t mode, device_method_t method);

#endif
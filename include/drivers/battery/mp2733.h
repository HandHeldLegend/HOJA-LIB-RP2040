#ifndef DRIVERS_BATTERY_MP2733_H
#define DRIVERS_BATTERY_MP2733_H

#include <stdbool.h>
#include <stdint.h>

// Driver-specific configuration for the MP2733 charger.
// When HOJA_BATTERY_DRIVER == BATTERY_DRIVER_MP2733, hoja_config_s embeds one of these as its
// `.battery` member; the board fills it in (main.c) and the driver reads it via
// hoja_config_get()->battery.
typedef struct
{
    uint8_t i2c_instance; // I2C bus instance the charger lives on

    // Optional: true while external power is present. The charger only reports an input once it has
    // worked out what kind it is, a second or so after the plug goes in, which is too late for a
    // board that starts up on that plug. NULL goes by the charger.
    bool (*input_present)(void);
} mp2733_cfg_s;

#endif

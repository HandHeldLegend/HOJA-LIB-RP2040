#ifndef DRIVERS_BATTERY_BQ25180_H
#define DRIVERS_BATTERY_BQ25180_H

#include <stdint.h>
#include <stdbool.h>
#include "devices/battery.h"

// Driver-specific configuration for the BQ25180 PMIC.
// A board declares a const instance of this and points a battery_driver_s
// at it (alongside bq25180_battery_api).
typedef struct
{
    uint8_t  i2c_instance;   // I2C bus instance the PMIC lives on
    uint16_t charge_rate_ma; // charge current applied during init (mA)
} bq25180_cfg_s;

// Vtable for the BQ25180. Inject via battery_driver_s.api.
extern const battery_driver_api_s bq25180_battery_api;

#endif

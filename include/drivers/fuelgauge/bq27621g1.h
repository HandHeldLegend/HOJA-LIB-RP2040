#ifndef DRIVERS_FUELGAUGE_BQ27621G1_H
#define DRIVERS_FUELGAUGE_BQ27621G1_H

#include "devices/fuelgauge.h"

#include <stdint.h>
#include <stdbool.h>

// Driver-specific configuration for the BQ27621G1 fuel gauge.
// A board declares a const instance and points a fuelgauge_driver_s at it
// (alongside bq27621g1_fuelgauge_api).
typedef struct
{
    uint8_t  i2c_instance;  // I2C bus instance the gauge lives on
    uint16_t terminate_mv;  // System minimum operating voltage (mV)
    uint16_t taper_ma;      // Charger taper/termination current (mA)
} bq27621g1_cfg_s;

// Vtable for the BQ27621G1. Inject via fuelgauge_driver_s.api.
extern const fuelgauge_driver_api_s bq27621g1_fuelgauge_api;

#endif

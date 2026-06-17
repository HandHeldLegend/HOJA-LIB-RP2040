#ifndef DRIVERS_FUELGAUGE_BQ27621G1_H
#define DRIVERS_FUELGAUGE_BQ27621G1_H

#include <stdint.h>
#include <stdbool.h>

// Driver-specific configuration for the BQ27621G1 fuel gauge.
// When HOJA_FUELGAUGE_DRIVER == FUELGAUGE_DRIVER_BQ27621G1, hoja_config_s embeds
// one of these as its `.fuelgauge` member; the board fills it in (main.c) and
// the driver reads it via hoja_config_get()->fuelgauge.
typedef struct
{
    uint8_t  i2c_instance;  // I2C bus instance the gauge lives on
    uint16_t terminate_mv;  // System minimum operating voltage (mV)
    uint16_t taper_ma;      // Charger taper/termination current (mA)
} bq27621g1_cfg_s;

#endif

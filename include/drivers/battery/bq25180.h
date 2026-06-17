#ifndef DRIVERS_BATTERY_BQ25180_H
#define DRIVERS_BATTERY_BQ25180_H

#include <stdint.h>
#include <stdbool.h>

// Driver-specific configuration for the BQ25180 PMIC.
// When HOJA_BATTERY_DRIVER == BATTERY_DRIVER_BQ25180, hoja_config_s embeds one
// of these as its `.battery` member; the board fills it in (main.c) and the
// driver reads it via hoja_config_get()->battery.
typedef struct
{
    uint8_t  i2c_instance;   // I2C bus instance the PMIC lives on
    uint16_t charge_rate_ma; // charge current applied during init (mA)
} bq25180_cfg_s;

#endif

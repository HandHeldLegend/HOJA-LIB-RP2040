#ifndef DRIVERS_BATTERY_BQ25180_H
#define DRIVERS_BATTERY_BQ25180_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "devices/battery.h"

#include "hoja_bsp.h"
#include "board_config.h"

// Requires SPI to function
#if (HOJA_BSP_HAS_I2C==0)
    #error "LSM6DSR driver requires I2C." 
#endif

#if defined(HOJA_BATTERY_DRIVER) && (HOJA_BATTERY_DRIVER==BATTERY_DRIVER_BQ25810)
    #ifndef HOJA_BATTERY_I2C_INSTANCE
        #error "HOJA_BATTERY_I2C_INSTANCE undefined in board_config.h" 
    #endif

    #define HOJA_BATTERY_INIT() bq25180_init() 
    #define HOJA_BATTERY_UPDATE_STATUS() bq25180_update_status() 
    #define HOJA_BATTERY_SET_SOURCE(source) bq25180_set_source(source) 
    
    #define HOJA_BATTERY_SET_SHIP_MODE() bq25180_set_ship_mode()
    #define HOJA_BATTERY_SET_CHARGE_RATE(rate_ma) bq25180_set_charge_rate(rate_ma)
#endif

bool bq25180_init();
battery_status_s bq25180_update_status();
int  bq25180_get_level();
bool bq25180_set_source(battery_source_t source);
bool bq25180_set_ship_mode();
bool bq25180_set_charge_rate(uint16_t rate_ma);

#endif
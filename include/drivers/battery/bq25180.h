#ifndef DRIVERS_BATTERY_BQ25180_H
#define DRIVERS_BATTERY_BQ25180_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "devices/battery.h"
#include "devices/adc.h"

#include "hoja_bsp.h"
#include "board_config.h"

#if defined(HOJA_BATTERY_DRIVER) && (HOJA_BATTERY_DRIVER==BATTERY_DRIVER_BQ25180)
    // Requires SPI to function
    #if (HOJA_BSP_HAS_I2C==0)
        #error "BQ25180 driver requires I2C." 
    #endif

    #ifndef HOJA_BATTERY_I2C_INSTANCE
        #error "HOJA_BATTERY_I2C_INSTANCE undefined in board_config.h" 
    #endif

    #define HOJA_BATTERY_INIT()             bq25180_init() 
    #define HOJA_BATTERY_GET_STATUS()       bq25180_get_status() 
    #define HOJA_BATTERY_UPDATE_STATUS()    bq25180_update_status()
    #define HOJA_BATTERY_SET_SOURCE(source) bq25180_set_source(source) 
    

    #define HOJA_BATTERY_SET_SHIP_MODE()    bq25180_set_ship_mode()
    #define HOJA_BATTERY_SET_CHARGE_RATE(rate_ma) bq25180_set_charge_rate(rate_ma)

    #if defined(HOJA_BATTERY_ADC_CFG)
        #warning "HOJA_BATTERY_ADC_CFG is undefined for BQ25180 and there will be no low voltage shutdown."
        #define HOJA_BATTERY_GET_VOLTAGE()    adc_read_battery()
    #endif
#endif

bool bq25180_init();
uint32_t bq25180_get_status();
int  bq25180_get_level();
bool bq25180_update_status();
bool bq25180_set_source(battery_source_t source);
bool bq25180_set_ship_mode();
bool bq25180_set_charge_rate(uint16_t rate_ma);

#endif
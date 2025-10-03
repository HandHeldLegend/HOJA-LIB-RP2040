#ifndef DRIVERS_FUELGAUGE_BQ27621G1_H
#define DRIVERS_FUELGAUGE_BQ27621G1_H

#include "hoja_bsp.h"
#include "board_config.h"
#include "devices/fuelgauge.h"

#include <stdint.h>
#include <stdbool.h>

#if defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER==FUELGAUGE_DRIVER_BQ27621G1)
    // Requires SPI to function
    #if (HOJA_BSP_HAS_I2C==0)
        #error "BQ27621G1 driver requires I2C." 
    #endif

    #ifndef HOJA_FUELGAUGE_I2C_INSTANCE
        #error "HOJA_FUELGAUGE_I2C_INSTANCE undefined in board_config.h" 
    #endif

    #define HOJA_FUELGAUGE_PRESENT()          bq27621g1_is_present()
    #define HOJA_FUELGAUGE_INIT(capacity_mah) bq27621g1_init(capacity_mah)
    #define HOJA_FUELGAUGE_GETPERCENT()       bq27621g1_get_percent()
    #define HOJA_FUELGAUGE_GET_STATUS()       bq27621g1_get_status()
#endif

fuelgauge_status_s bq27621g1_get_status(void);
bool bq27621g1_is_present(void);
uint8_t bq27621g1_get_percent(void);
bool bq27621g1_init(uint16_t capacity_mah);

#endif
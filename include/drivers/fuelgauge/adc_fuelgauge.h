#ifndef DRIVERS_FUELGAUGE_ADC
#define DRIVERS_FUELGAUGE_ADC

#include "hoja_bsp.h"
#include "board_config.h"
#include "devices/fuelgauge.h"

#include <stdint.h>
#include <stdbool.h>

#if defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER==FUELGAUGE_DRIVER_ADC)
    #if !defined(HOJA_BATTERY_ADC_CFG)
        #error "HOJA_BATTERY_ADC_CFG undefined in board_config.h" 
    #endif 

    #if !defined(HOJA_BATTERY_VOLTAGE_MEASURE_OFFSET)
        #warning "HOJA_BATTERY_VOLTAGE_MEASURE_OFFSET float optional define in board_config.h" 
    #endif

    #define HOJA_FUELGAUGE_PRESENT()          adc_fuelgauge_is_present()
    #define HOJA_FUELGAUGE_INIT(capacity_mah) adc_fuelgauge_init(capacity_mah)
    #define HOJA_FUELGAUGE_GETPERCENT()       adc_fuelgauge_get_percent()
    #define HOJA_FUELGAUGE_GET_STATUS()       add_fuelgauge_get_status()
#endif

static inline bool adc_fuelgauge_is_present(void)
{
    return true;
}

fuelgauge_status_s add_fuelgauge_get_status(void);
uint8_t adc_fuelgauge_get_percent(void);
bool adc_fuelgauge_init(uint16_t capacity_mah);

#endif
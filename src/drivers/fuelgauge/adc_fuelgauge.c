#include "drivers/fuelgauge/adc_fuelgauge.h"
#include "devices/adc.h"

#if defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER==FUELGAUGE_DRIVER_ADC)

#if defined(HOJA_BATTERY_VOLTAGE_MEASURE_OFFSET)
#define VOLTAGE_MEASURE_OFFSET HOJA_BATTERY_VOLTAGE_MEASURE_OFFSET
#else
#define VOLTAGE_MEASURE_OFFSET 0.125f
#endif

#define VOLTAGE_LEVEL_CRITICAL  3.125f
#define VOLTAGE_LEVEL_LOW       3.3f
#define VOLTAGE_LEVEL_MID       3.975f

uint8_t adc_fuelgauge_get_percent(void)
{
    uint16_t raw_voltage = adc_read_battery();

    // Convert to a voltage value (we use a voltage divider on this pin)
    float voltage = ( ( ((float)raw_voltage / 4095.0f) *  3.3f ) * 2.0f ) + VOLTAGE_MEASURE_OFFSET;

    if(voltage <= VOLTAGE_LEVEL_CRITICAL)
    {
        return 1; // Dead battery
    }
    else if(voltage <= VOLTAGE_LEVEL_LOW)
    {
        return 30; // 30%
    }
    else if(voltage <= VOLTAGE_LEVEL_MID)
    {
        return 70; // 70%
    }
    else 
    {
        return 100; // 100%
    }
}

bool adc_fuelgauge_init(uint16_t capacity_mah)
{
    return true;
}

#endif
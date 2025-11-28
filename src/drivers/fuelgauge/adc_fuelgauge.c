#include "drivers/fuelgauge/adc_fuelgauge.h"

#if defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER==FUELGAUGE_DRIVER_ADC)

#if defined(HOJA_BATTERY_VOLTAGE_MEASURE_OFFSET)
#define VOLTAGE_MEASURE_OFFSET HOJA_BATTERY_VOLTAGE_MEASURE_OFFSET
#else
#define VOLTAGE_MEASURE_OFFSET 0.125f
#endif

#define VOLTAGE_LEVEL_FULL      4.2f
#define VOLTAGE_LEVEL_CRITICAL  3.125f
#define VOLTAGE_LEVEL_LOW       3.3f
#define VOLTAGE_LEVEL_MID       3.975f

#define VOLTAGE_RANGE (VOLTAGE_LEVEL_FULL - VOLTAGE_LEVEL_CRITICAL)

uint8_t adc_fuelgauge_get_percent(void)
{
    uint16_t raw_voltage = adc_read_battery();

    // Convert to a voltage value (we use a voltage divider on this pin)
    float voltage = ( ( ((float)raw_voltage / 4095.0f) *  3.3f ) * 2.0f ) + VOLTAGE_MEASURE_OFFSET;

    float remaining = (voltage-VOLTAGE_LEVEL_CRITICAL);

    if(remaining > 0)
    {
        float percentf = (remaining / VOLTAGE_RANGE) * 100.0f;
        percentf = (percentf > 100.0f) ? 100.0f : percentf;
        return (uint8_t) percentf;
    }

    return 0;
}

fuelgauge_status_s adc_fuelgauge_get_status()
{
    static fuelgauge_status_s status = {0};
    status.percent = adc_fuelgauge_get_percent();
    status.connected = true;
    status.discharge_only = true;

    return status;
}


bool adc_fuelgauge_init(uint16_t capacity_mah)
{
    return true;
}

#endif
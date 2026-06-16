#ifndef DRIVERS_FUELGAUGE_ADC
#define DRIVERS_FUELGAUGE_ADC

#include "devices/fuelgauge.h"

#include <stdint.h>
#include <stdbool.h>

// Driver-specific configuration for the ADC (voltage-based) fuel gauge.
// Reads pack voltage via the board's cb_hoja_read_battery() callback.
typedef struct
{
    float voltage_offset; // Calibration offset added to the measured voltage
} adc_fuelgauge_cfg_s;

// Vtable for the ADC fuel gauge. Inject via fuelgauge_driver_s.api.
extern const fuelgauge_driver_api_s adc_fuelgauge_fuelgauge_api;

#endif

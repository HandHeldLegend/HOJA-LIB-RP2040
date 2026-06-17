#ifndef DRIVERS_FUELGAUGE_ADC
#define DRIVERS_FUELGAUGE_ADC

#include <stdint.h>
#include <stdbool.h>

// Driver-specific configuration for the ADC (voltage-based) fuel gauge.
// Reads pack voltage via the board's cb_hoja_read_battery() callback.
// When HOJA_FUELGAUGE_DRIVER == FUELGAUGE_DRIVER_ADC, hoja_config_s embeds one
// of these as its `.fuelgauge` member; the board fills it in (main.c) and the
// driver reads it via hoja_config_get()->fuelgauge.
typedef struct
{
    float voltage_offset; // Calibration offset added to the measured voltage
} adc_fuelgauge_cfg_s;

#endif

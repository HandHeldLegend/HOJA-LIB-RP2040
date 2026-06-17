#include "board_config.h"

#if defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER == FUELGAUGE_DRIVER_ADC)

#include "drivers/fuelgauge/adc_fuelgauge.h"
#include "devices/fuelgauge.h"
#include "hoja.h"

#define VOLTAGE_DEFAULT_OFFSET  0.125f

#define VOLTAGE_LEVEL_FULL      4.2f
#define VOLTAGE_LEVEL_CRITICAL  3.125f

#define VOLTAGE_RANGE (VOLTAGE_LEVEL_FULL - VOLTAGE_LEVEL_CRITICAL)

static uint8_t adc_fuelgauge_get_percent(float voltage_offset)
{
    uint16_t raw_voltage = cb_hoja_read_battery();
    if(raw_voltage==0xFFFF) // Invalid reading or not implemented
        return 100;

    // Convert to a voltage value (we use a voltage divider on this pin)
    float voltage = ( ( ((float)raw_voltage / 4095.0f) *  3.3f ) * 2.0f ) + voltage_offset;

    float remaining = (voltage-VOLTAGE_LEVEL_CRITICAL);

    if(remaining > 0)
    {
        float percentf = (remaining / VOLTAGE_RANGE) * 100.0f;
        percentf = (percentf > 100.0f) ? 100.0f : percentf;
        return (uint8_t) percentf;
    }

    return 0;
}

// ---- Strong driver contract overrides (weak-function model) ----

bool fuelgauge_driver_init(uint16_t capacity_mah)
{
    (void)capacity_mah;
    // Voltage-based gauge is always "present" (relies on the board's ADC).
    return true;
}

fuelgauge_status_s fuelgauge_driver_get_status(void)
{
    const hoja_config_s *c = hoja_config_get();
    const adc_fuelgauge_cfg_s *cfg = c ? &c->fuelgauge : NULL;
    float offset = cfg ? cfg->voltage_offset : VOLTAGE_DEFAULT_OFFSET;

    fuelgauge_status_s status = {0};
    status.percent = adc_fuelgauge_get_percent(offset);
    status.connected = true;
    status.discharge_only = true;

    return status;
}

const char *fuelgauge_driver_part_code(void)
{
    return "ADC";
}

#endif // HOJA_FUELGAUGE_DRIVER == FUELGAUGE_DRIVER_ADC

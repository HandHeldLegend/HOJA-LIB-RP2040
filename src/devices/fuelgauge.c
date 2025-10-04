
#include "devices/fuelgauge.h"
#include "hoja.h"
#include "hal/sys_hal.h"
#include "utilities/interval.h"
#include "utilities/hwtest.h"
#include "utilities/crosscore_snapshot.h"

#if defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER==FUELGAUGE_DRIVER_BQ27621G1)
    #include "drivers/fuelgauge/bq27621g1.h"
#elif defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER==FUELGAUGE_DRIVER_ADC)
    #include "drivers/fuelgauge/adc_fuelgauge.h"
#endif

SNAPSHOT_TYPE(fuelgauge, fuelgauge_status_s);
snapshot_fuelgauge_t _fuelgauge_snap;

void fuelgauge_update_status(void)
{
    fuelgauge_status_s status = {0};

    #if defined(HOJA_FUELGAUGE_GET_STATUS)
    status = HOJA_FUELGAUGE_GET_STATUS();

    if(status.percent>=55)
    {
        status.simple = BATTERY_LEVEL_HIGH;
    }
    else if (status.percent>=15)
    {
        status.simple = BATTERY_LEVEL_MID;
    }
    else if (status.percent >= 5)
    {
        status.simple = BATTERY_LEVEL_LOW;
    }
    else 
    {
        status.simple = BATTERY_LEVEL_CRITICAL;
    }
    #endif

    if(!status.connected) 
    {
        status.percent = 100;
        status.simple = 4;
    }

    snapshot_fuelgauge_write(&_fuelgauge_snap, &status);
}

void fuelgauge_get_status(fuelgauge_status_s *out)
{
    snapshot_fuelgauge_read(&_fuelgauge_snap, out);
}

void _fuelgauge_set_connected(bool connected)
{
    fuelgauge_status_s tmp;
    snapshot_fuelgauge_read(&_fuelgauge_snap, &tmp);
    tmp.connected = connected;
    snapshot_fuelgauge_write(&_fuelgauge_snap, &tmp);
}

void _fuelgauge_set_percent(uint8_t percent)
{
    fuelgauge_status_s tmp;
    snapshot_fuelgauge_read(&_fuelgauge_snap, &tmp);
    tmp.percent = percent;
    tmp.simple = 4;
    snapshot_fuelgauge_write(&_fuelgauge_snap, &tmp);
}

#if defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER==FUELGAUGE_DRIVER_BQ27621G1)
    #include "drivers/fuelgauge/bq27621g1.h"
#endif

#define VOLTAGE_LEVEL_CRITICAL  3.125f
#define VOLTAGE_LEVEL_LOW       3.3f
#define VOLTAGE_LEVEL_MID       3.975f

bool fuelgauge_init(uint16_t capacity_mah) 
{
    _fuelgauge_set_connected(false);

    bool present = false;

    #if defined(HOJA_FUELGAUGE_PRESENT)
    present = HOJA_FUELGAUGE_PRESENT();
    #endif

    // Fuel gauge isn't present or not responding
    if(!present) return false;

    bool init = false;

    #if defined(HOJA_FUELGAUGE_INIT)
    HOJA_FUELGAUGE_INIT(capacity_mah);
    #endif 

    // Fuel gauge init failure
    if(!init) return false;

    // Get initial percentage
    uint8_t percent = 100;
    #if defined(HOJA_FUELGAUGE_GETPERCENT)
    percent = HOJA_FUELGAUGE_GETPERCENT();
    #endif 

    _fuelgauge_set_percent(percent);
}
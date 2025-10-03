
#include "devices/fuelgauge.h"
#include "hoja.h"
#include "hal/sys_hal.h"
#include "utilities/interval.h"
#include "utilities/hwtest.h"
#include "utilities/crosscore_snapshot.h"

SNAPSHOT_TYPE(fuelgauge, fuelgauge_status_s);
snapshot_fuelgauge_t _fuelgauge_snap;

void fuelgauge_get_status(fuelgauge_status_s *out)
{
    snapshot_fuelgauge_read(&_fuelgauge_snap, out);
}

void fuelgauge_set_connected(bool connected)
{
    fuelgauge_status_s tmp;
    snapshot_fuelgauge_read(&_fuelgauge_snap, &tmp);
    tmp.connected = connected;
    snapshot_fuelgauge_write(&_fuelgauge_snap, &tmp);
}

void fuelgauge_set_percent(uint8_t percent)
{
    fuelgauge_status_s tmp;
    snapshot_fuelgauge_read(&_fuelgauge_snap, &tmp);
    tmp.percent = percent;
    snapshot_fuelgauge_write(&_fuelgauge_snap, &tmp);
}

#if defined(HOJA_FUELGAUGE_DRIVER) && (HOJA_FUELGAUGE_DRIVER==FUELGAUGE_DRIVER_BQ27621G1)
    #include "drivers/fuelgauge/bq27621g1.h"
#endif

typedef enum 
{
    BATTERY_LEVEL_UNAVAILABLE = -1,
    BATTERY_LEVEL_CRITICAL,
    BATTERY_LEVEL_LOW,
    BATTERY_LEVEL_MID,
    BATTERY_LEVEL_HIGH
} battery_level_t;

#define VOLTAGE_LEVEL_CRITICAL  3.125f
#define VOLTAGE_LEVEL_LOW       3.3f
#define VOLTAGE_LEVEL_MID       3.975f

battery_level_t fuelgauge_get_level_basic()
{

}

uint8_t fuelgauge_get_level_percent()
{

}

void fuelgauge_task(uint64_t timestamp)
{
    static interval_s interval = {0};

    if (interval_run(timestamp, 5*1000*1000, &interval))
    {
    
        if(false)
        {
            sys_hal_reboot();
        }
    }
}

bool fuelgauge_init(uint16_t capacity_mah) 
{
    bool present = false;
#if defined(HOJA_FUELGAUGE_PRESENT)
    //_gauge_present = HOJA_FUELGAUGE_PRESENT();
#endif
}

#include "devices/fuelgauge.h"
#include "hoja.h"
#include "hal/sys_hal.h"
#include "utilities/interval.h"
#include "utilities/hwtest.h"
#include "utilities/crosscore_snapshot.h"

SNAPSHOT_TYPE(fuelgauge, fuelgauge_status_s);
snapshot_fuelgauge_t _fuelgauge_snap;

// Active fuel gauge driver, injected via the board's hoja_config_s.
static const fuelgauge_driver_s *_fuelgauge_driver = NULL;

void fuelgauge_update_status(void)
{
    fuelgauge_status_s status = {0};

    if(_fuelgauge_driver && _fuelgauge_driver->api->get_status)
    {
        status = _fuelgauge_driver->api->get_status(_fuelgauge_driver);

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
    }

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

// Init fuel gauge. Always safe to call; the return value reports whether a
// driver was assigned and whether the gauge initialized.
fuelgauge_result_t fuelgauge_init(void)
{
    _fuelgauge_set_connected(false);

    const hoja_config_s *config = hoja_config_get();
    _fuelgauge_driver = config ? config->fuelgauge_driver : NULL;

    // No fuel gauge driver assigned for this board.
    if(!_fuelgauge_driver || !_fuelgauge_driver->api || !_fuelgauge_driver->api->init)
        return FUELGAUGE_RESULT_NO_DRIVER;

    uint16_t capacity_mah = config ? config->battery_capacity_mah : 0;

    // init() also performs presence detection.
    if(!_fuelgauge_driver->api->init(_fuelgauge_driver, capacity_mah))
        return FUELGAUGE_RESULT_FAILED;

    // Pull initial status (percent + connected) from the driver.
    fuelgauge_update_status();

    return FUELGAUGE_RESULT_OK;
}


#include "devices/fuelgauge.h"
#include "hoja.h"
#include "hal/sys_hal.h"
#include "utilities/interval.h"
#include "utilities/hwtest.h"
#include "utilities/crosscore_snapshot.h"

SNAPSHOT_TYPE(fuelgauge, fuelgauge_status_s);
snapshot_fuelgauge_t _fuelgauge_snap;

// Weak driver contract defaults. A board with no fuel gauge driver selected
// links these, making every fuel gauge call a safe no-op. The selected driver
// (compiled in by the HOJA_FUELGAUGE_DRIVER gate) overrides them.
__attribute__((weak)) bool fuelgauge_driver_init(uint16_t capacity_mah) { (void)capacity_mah; return false; }
__attribute__((weak)) fuelgauge_status_s fuelgauge_driver_get_status(void) { fuelgauge_status_s s = {0}; return s; }
__attribute__((weak)) const char *fuelgauge_driver_part_code(void) { return NULL; }

// A driver is present iff it supplies a part code (weak default returns NULL).
static inline bool _fuelgauge_present(void) { return fuelgauge_driver_part_code() != NULL; }

void fuelgauge_update_status(void)
{
    fuelgauge_status_s status = {0};

    if(_fuelgauge_present())
    {
        status = fuelgauge_driver_get_status();

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

    // No fuel gauge driver compiled in for this board.
    if(!_fuelgauge_present())
        return FUELGAUGE_RESULT_NO_DRIVER;

    const hoja_config_s *config = hoja_config_get();
    uint16_t capacity_mah = config ? config->battery_capacity_mah : 0;

    // init() also performs hardware presence detection.
    if(!fuelgauge_driver_init(capacity_mah))
        return FUELGAUGE_RESULT_FAILED;

    // Pull initial status (percent + connected) from the driver.
    fuelgauge_update_status();

    return FUELGAUGE_RESULT_OK;
}

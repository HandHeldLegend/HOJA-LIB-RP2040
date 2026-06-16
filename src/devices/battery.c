#include "devices/battery.h"
#include "devices/rgb.h"
#include "utilities/interval.h"
#include "utilities/settings.h"
#include "utilities/crosscore_snapshot.h"
#include "hoja.h"
#include "board_config.h"

SNAPSHOT_TYPE(battery, battery_status_s);
snapshot_battery_t _battery_snap;
bool _battery_init_done = false;
volatile bool _shipping_lockout = false;

// Active battery driver, injected via the board's hoja_config_s.
static const battery_driver_s *_battery_driver = NULL;

void _battery_set_connected(bool connected)
{
    battery_status_s tmp;
    snapshot_battery_read(&_battery_snap, &tmp);
    tmp.connected = connected;
    snapshot_battery_write(&_battery_snap, &tmp);
}

void _battery_set_charging(bool charging)
{
    battery_status_s tmp;
    snapshot_battery_read(&_battery_snap, &tmp);
    tmp.charging = charging;
    snapshot_battery_write(&_battery_snap, &tmp);
}

void _battery_set_plugged(bool plugged)
{
    battery_status_s tmp;
    snapshot_battery_read(&_battery_snap, &tmp);
    tmp.plugged = plugged;
    snapshot_battery_write(&_battery_snap, &tmp);
}

// Perform a hardware level transaction
// to obtain the latest status
void battery_update_status(void)
{
    if(_shipping_lockout) return;

    battery_status_s status = {0};

    if(_battery_init_done && _battery_driver && _battery_driver->api->get_status)
        status = _battery_driver->api->get_status(_battery_driver);

    snapshot_battery_write(&_battery_snap, &status);
}

void battery_get_status(battery_status_s *out)
{
    snapshot_battery_read(&_battery_snap, out);
}

// Init battery PMIC. Always safe to call; the return value reports whether a
// driver was assigned and whether the PMIC initialized.
battery_result_t battery_init(void)
{
    _battery_init_done = false;

    // Reset connected status
    _battery_set_connected(false);

    // Grab the board-assigned driver (if any).
    const hoja_config_s *config = hoja_config_get();
    _battery_driver = config ? config->battery_driver : NULL;

    // No battery driver assigned for this board.
    if(!_battery_driver || !_battery_driver->api || !_battery_driver->api->init)
        return BATTERY_RESULT_NO_DRIVER;

    // init() also performs presence detection.
    if(!_battery_driver->api->init(_battery_driver))
        return BATTERY_RESULT_FAILED;

    _battery_set_connected(true);
    _battery_init_done = true;

    // Get initial status
    battery_update_status();
    return BATTERY_RESULT_OK;
}

// Set PMIC charge rate. Always safe to call.
battery_result_t battery_set_charge_rate(uint16_t rate_ma)
{
    if(!_battery_driver || !_battery_driver->api->set_charge_rate)
        return BATTERY_RESULT_NO_DRIVER;

    return _battery_driver->api->set_charge_rate(_battery_driver, rate_ma)
         ? BATTERY_RESULT_OK
         : BATTERY_RESULT_FAILED;
}

// Enable PMIC ship mode (power off with power conservation). Always safe to
// call; returns BATTERY_RESULT_NO_DRIVER when there's no PMIC to power down.
battery_result_t battery_set_ship_mode()
{
    _shipping_lockout = true;

    if(!_battery_driver || !_battery_driver->api->set_ship_mode)
        return BATTERY_RESULT_NO_DRIVER;

    return _battery_driver->api->set_ship_mode(_battery_driver)
         ? BATTERY_RESULT_OK
         : BATTERY_RESULT_FAILED;
}
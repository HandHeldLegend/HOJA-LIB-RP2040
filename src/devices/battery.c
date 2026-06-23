#include "devices/battery.h"
#include "devices/rgb.h"
#include "utilities/interval.h"
#include "utilities/settings.h"
#include "utilities/crosscore_utils.h"
#include "utilities/boot.h"
#include "hoja.h"
#include "board_config.h"

SNAPSHOT_TYPE(battery, battery_status_s);
snapshot_battery_t _battery_snap;
bool _battery_init_done = false;
volatile bool _shipping_lockout = false;

// Weak driver contract defaults. A board that selects no battery driver links
// these, making every battery call a safe no-op. The selected driver (compiled
// in by the HOJA_BATTERY_DRIVER gate) overrides them with strong definitions.
__attribute__((weak)) bool battery_driver_init(void) { return false; }
__attribute__((weak)) battery_status_s battery_driver_get_status(void) { battery_status_s s = {0}; return s; }
__attribute__((weak)) bool battery_driver_set_charge_rate(uint16_t rate_ma) { (void)rate_ma; return false; }
__attribute__((weak)) bool battery_driver_set_ship_mode(void) { return false; }
__attribute__((weak)) const char *battery_driver_part_code(void) { return NULL; }

// A driver is present iff it supplies a part code (weak default returns NULL).
static inline bool _battery_present(void) { return battery_driver_part_code() != NULL; }

static bool _battery_boot_wants_init(void)
{
    const boot_info_s *boot = boot_get_info();
    if (!boot)
        return true;

    switch (boot->transport)
    {
    case GAMEPAD_TRANSPORT_NESBUS:
    case GAMEPAD_TRANSPORT_JOYBUS64:
    case GAMEPAD_TRANSPORT_JOYBUSGC:
        return false;

    default:
        return true;
    }
}

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

    if(_battery_init_done)
        status = battery_driver_get_status();

    snapshot_battery_write(&_battery_snap, &status);
}

void battery_get_status(battery_status_s *out)
{
    snapshot_battery_read(&_battery_snap, out);
}

// Init battery PMIC. Always safe to call.
battery_result_t battery_init(void)
{
    if (!_battery_boot_wants_init())
        return BATTERY_RESULT_SKIPPED;

    _battery_init_done = false;

    // Reset connected status
    _battery_set_connected(false);

    // No battery driver compiled in for this board.
    if(!_battery_present())
        return BATTERY_RESULT_NO_DRIVER;

    // init() also performs hardware presence detection.
    if(!battery_driver_init())
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
    if(!_battery_present())
        return BATTERY_RESULT_NO_DRIVER;

    return battery_driver_set_charge_rate(rate_ma)
         ? BATTERY_RESULT_OK
         : BATTERY_RESULT_FAILED;
}

// Enable PMIC ship mode (power off with power conservation). Always safe to
// call; returns BATTERY_RESULT_NO_DRIVER when there's no PMIC to power down.
battery_result_t battery_set_ship_mode()
{
    _shipping_lockout = true;

    if(!_battery_present())
        return BATTERY_RESULT_NO_DRIVER;

    return battery_driver_set_ship_mode()
         ? BATTERY_RESULT_OK
         : BATTERY_RESULT_FAILED;
}
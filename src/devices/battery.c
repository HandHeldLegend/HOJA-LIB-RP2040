#include "devices/battery.h"
#include "devices/rgb.h"
#include "utilities/interval.h"
#include "utilities/settings.h"
#include "utilities/crosscore_snapshot.h"
#include "hoja.h"
#include "board_config.h"

#if defined(HOJA_BATTERY_DRIVER) && (HOJA_BATTERY_DRIVER==BATTERY_DRIVER_BQ25180)
    #include "drivers/battery/bq25180.h"
#endif

SNAPSHOT_TYPE(battery, battery_status_s);
snapshot_battery_t _battery_snap;

void battery_get_status(battery_status_s *out)
{
    snapshot_battery_read(&_battery_snap, out);
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
void _battery_update_status(void)
{
    battery_status_s status = {0};

    #if defined(HOJA_BATTERY_GET_STATUS)
    status = HOJA_BATTERY_GET_STATUS();
    #endif

    snapshot_battery_write(&_battery_snap, &status);
}

#define BATTERY_LEVEL_INTERVAL_US 1000*1000 // 1 second

// When I measure past the divider, it's 2v when the system is powered on
// The battery voltage is 4.2v when I measure this. The ratio is 

#if defined(HOJA_BATTERY_VOLTAGE_MEASURE_OFFSET)
#define VOLTAGE_MEASURE_OFFSET HOJA_BATTERY_VOLTAGE_MEASURE_OFFSET
#else
#define VOLTAGE_MEASURE_OFFSET 0.125f
#endif

int _battery_update_charge()
{
    #if defined(HOJA_BATTERY_GET_VOLTAGE)
    uint16_t raw_voltage = HOJA_BATTERY_GET_VOLTAGE();
    // Convert to a voltage value (we use a voltage divider on this pin)
    float voltage = ( ( ((float)raw_voltage / 4095.0f) *  3.3f ) * 2.0f ) + VOLTAGE_MEASURE_OFFSET;

    if(voltage <= VOLTAGE_LEVEL_CRITICAL)
    {
        if(_battery_status.plug_status == BATTERY_PLUG_UNPLUGGED)
        {
            _battery_status.battery_level = BATTERY_LEVEL_CRITICAL;
            _battery_event_handler(BATTERY_EVENT_BATTERY_DEAD);
            return -1;
        }
    }
    else if(voltage <= VOLTAGE_LEVEL_LOW)
    {
        _battery_status.battery_level = BATTERY_LEVEL_LOW;
    }
    else if(voltage <= VOLTAGE_LEVEL_MID)
    {
        _battery_status.battery_level = BATTERY_LEVEL_MID;
    }
    else 
    {
        _battery_status.battery_level = BATTERY_LEVEL_HIGH;
    }
    #else 
    return 0;
    #endif 
    return 0;
}

// Init battery PMIC, returns battery_init_t status
bool battery_init(void)
{
    // Reset connected status
    _battery_set_connected(false);

    bool present = false;

    #if defined(HOJA_BATTERY_PRESENT)
    present = HOJA_BATTERY_PRESENT();
    #endif

    // PMIC is not present or not responding
    if(!present) return false;

    _battery_set_connected(true);

    bool init = false;

    #if defined(HOJA_BATTERY_INIT)
    init = HOJA_BATTERY_INIT();
    #endif

    // PMIC initialization failure
    if(!init) return false;

    // Get initial status
    _battery_update_status();
    return true;
}

// Set PMIC charge rate.
bool battery_set_charge_rate(uint16_t rate_ma)
{
    #if defined(HOJA_BATTERY_SET_CHARGE_RATE)
    return HOJA_BATTERY_SET_CHARGE_RATE(rate_ma);
    #else 
    return false;
    #endif 
}

// Enable PMIC ship mode (power off with power conservation).
void battery_set_ship_mode()
{
    #if defined(HOJA_BATTERY_SET_SHIP_MODE)
    HOJA_BATTERY_SET_SHIP_MODE();
    #endif
    return;
}
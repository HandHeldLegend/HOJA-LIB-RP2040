#include "utilities/sysmon.h"
#include "utilities/interval.h"

#include "devices/battery.h"
#include "devices/fuelgauge.h"
#include "devices/rgb.h"

#include "utilities/transport.h"
#include "utilities/crosscore_snapshot.h"
#include "hoja.h"

transport_status_s _systransport = {.connected=false, .player_number=0, .polling_rate_us=8000, .wireless=false};
battery_status_s _sysbattery = {.connected=false, .charging=false, .plugged=false};
fuelgauge_status_s _sysfuel = {.connected=false, .percent=100};

// Critical power shutdown function
#define CRITICAL_SHUTDOWN_TIMEOUT_US 6500 * 1000 // 6.5 seconds
#define SYSMON_INTERVAL_US 1 * 1000 * 1000 // 2 seconds
#define DISCONNECT_TIMEOUT_US 30 * 1000 * 1000 // 30 second timeout to shut down when disconnected
bool _shutdown_timeout_initiated = false;
int  _shutdown_timeout_reset = 0;
bool _shutdown_lockout = false;
void sysmon_set_critical_shutdown(void);

typedef enum
{
    SYSMON_EVT_UNPLUG,
    SYSMON_EVT_PLUG,
} sysmon_evt_t;

void _sysmon_event_handler(int evt)
{
    switch(evt)
    {
        // 5V source plugged
        case SYSMON_EVT_PLUG:

        break;

        // 5V source unplugged
        case SYSMON_EVT_UNPLUG:
        _shutdown_lockout = true;
        hoja_deinit(hoja_shutdown);
        break;
    }
}

void sysmon_task(uint64_t timestamp)
{
    if(_shutdown_lockout) return;

    if(_shutdown_timeout_initiated)
    {
        static interval_s critical_interval = {0};
        if(interval_resettable_run(timestamp, CRITICAL_SHUTDOWN_TIMEOUT_US, (_shutdown_timeout_reset==1), &critical_interval))
        {
            _shutdown_lockout = true;
            hoja_deinit(hoja_shutdown);
        }
        _shutdown_timeout_reset = 2;

        // Do not run any other sysmon tasks 
        // during critical power shutdown
        return;
    }

    static interval_s interval = {0};
    static bool flipflop = false;

    if (interval_run(timestamp, SYSMON_INTERVAL_US, &interval))
    {
        // static transport_status_s tmp_transport;
        // transport_get_status(&tmp_transport);

        static battery_status_s tmp_battery;
        static fuelgauge_status_s tmp_fuel;

        // Flip between updating battery and fuel gauge status
        if(flipflop)
        {
            battery_update_status();
            battery_get_status(&tmp_battery);
        }
        else
        {
            fuelgauge_update_status();
            fuelgauge_get_status(&tmp_fuel);
        }
        flipflop = !flipflop;

        // PMIC is functional
        if(tmp_battery.connected)
        {
            if(_sysbattery.plugged && !tmp_battery.plugged)
            {
                // UNPLUG EVENT
                _sysmon_event_handler(SYSMON_EVT_UNPLUG);
                return;
            }
            else if(!_sysbattery.plugged && tmp_battery.plugged)
            {
                // PLUG EVENT
                _sysmon_event_handler(SYSMON_EVT_PLUG);
            }

            _sysbattery = tmp_battery;
        }

        // PMIC and Fuel Gauge need to be present to
        // utilize the fuel gauge status
        if(tmp_battery.connected && tmp_fuel.connected)
        {
            _sysfuel = tmp_fuel;

            if(_sysfuel.simple == BATTERY_LEVEL_CRITICAL && !_sysbattery.plugged)
            {
                sysmon_set_critical_shutdown();
            }
        }
    }
}

void sysmon_set_critical_shutdown(void)
{
    if(_shutdown_timeout_initiated) return;

    rgb_set_idle(true);
    hoja_set_notification_status(COLOR_RED);

    _shutdown_timeout_initiated = true;
    _shutdown_timeout_reset = 1;
}
#include "utilities/sysmon.h"
#include "utilities/interval.h"

#include "devices/battery.h"
#include "devices/fuelgauge.h"
#include "utilities/transport.h"
#include "hoja.h"

transport_status_s _systransport = {.connected=false, .player_number=0, .polling_rate_us=8000, .wireless=false};
battery_status_s _sysbattery = {.connected=false, .charging=false, .plugged=true};
fuelgauge_status_s _sysfuel = {.connected=false, .percent=100};

// Critical power shutdown function
#define CRITICAL_SHUTDOWN_TIMEOUT_US 6500 * 1000 // 6.5 seconds
#define SYSMON_INTERVAL_US 2 * 1000 * 1000 // 2 seconds
#define DISCONNECT_TIMEOUT_US 30 * 1000 * 1000 // 30 second timeout to shut down when disconnected
bool _shutdown_timeout_initiated = false;
int  _shutdown_timeout_reset = 0;
bool _shutdown_lockout = false;
void sysmon_set_critical_shutdown(void);

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

    if (interval_run(timestamp, SYSMON_INTERVAL_US, &interval))
    {
        static transport_status_s tmp_transport;
        transport_get_status(&tmp_transport);

        battery_status_s tmp_battery;
        battery_get_status(&tmp_battery);

        fuelgauge_status_s tmp_fuel;
        fuelgauge_get_status(&tmp_fuel);
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
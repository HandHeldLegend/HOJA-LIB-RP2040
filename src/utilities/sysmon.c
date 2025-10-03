#include "utilities/sysmon.h"
#include "utilities/interval.h"

#include "devices/battery.h"
#include "devices/fuelgauge.h"
#include "utilities/transport.h"

transport_status_s _systransport = {.connected=false, .player_number=0, .polling_rate_us=8000, .wireless=false};
battery_status_s _sysbattery = {.connected=false, .charging=false, .plugged=true};
fuelgauge_status_s _sysfuel = {.connected=false, .percent=100};

void sysmon_task(uint64_t timestamp)
{
    static interval_s interval = {0};

    if (interval_run(timestamp, 2 * 1000 * 1000, &interval))
    {
        transport_status_s tmp_transport = transport_get_status();
        battery_status_s tmp_battery = battery_get_status();
        fuelgauge_status_s tmp_fuel = fuelgauge_get_status();
    }
}
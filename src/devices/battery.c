#include "devices/battery.h"
#include "devices/rgb.h"
#include "utilities/interval.h"
#include "utilities/settings.h"
#include "hoja.h"
#include "board_config.h"

#if defined(HOJA_BATTERY_DRIVER) && (HOJA_BATTERY_DRIVER==BATTERY_DRIVER_BQ25180)
    #include "drivers/battery/bq25180.h"
#endif

bool _wired_override = false;

battery_status_s _battery_status = {
    .battery_status = BATTERY_STATUS_UNAVAILABLE,
    .charge_status  = BATTERY_CHARGE_UNAVAILABLE,
    .plug_status    = BATTERY_PLUG_UNAVAILABLE,
    .battery_level  = BATTERY_LEVEL_UNAVAILABLE,
    };

battery_status_s _new_battery_status = {0};

#define BATTERY_LEVEL_INTERVAL_US 1000*1000 // 1 second

// When I measure past the divider, it's 2v when the system is powered on
// The battery voltage is 4.2v when I measure this. The ratio is 

#if defined(HOJA_BATTERY_VOLTAGE_MEASURE_OFFSET)
#define VOLTAGE_MEASURE_OFFSET HOJA_BATTERY_VOLTAGE_MEASURE_OFFSET
#else
#define VOLTAGE_MEASURE_OFFSET 0.125f
#endif

bool _battery_shutdown_lockout = false;

// Battery event handler
void _battery_event_handler(battery_event_t event)
{
    switch(event)
    {
        case BATTERY_EVENT_CABLE_PLUGGED:
        break;

        case BATTERY_EVENT_CABLE_UNPLUGGED:
            _battery_shutdown_lockout = true;
            hoja_deinit(hoja_shutdown);
        break;

        case BATTERY_EVENT_CHARGE_COMPLETE:
        break;

        case BATTERY_EVENT_CHARGE_START:
        break;

        case BATTERY_EVENT_BATTERY_DEAD:
        break;
    }
}

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
    _battery_status.battery_level = BATTERY_LEVEL_HIGH;
    #endif 
    return 0;
}

// Init battery PMIC, returns battery_init_t status
int battery_init(bool wired_override)
{
    _wired_override = wired_override;
    if(wired_override) 
    {
        return BATTERY_INIT_WIRED_OVERRIDE;
    }

    if(battery_config->battery_config_version != CFG_BLOCK_BATTERY_VERSION)
    {
        battery_config->battery_config_version = CFG_BLOCK_BATTERY_VERSION;
        battery_config->charge_level_percent = 100.0f;
    }

    #if defined(HOJA_BATTERY_INIT)
    if(HOJA_BATTERY_INIT())
    {
        // Set initial status
        _battery_status.val = HOJA_BATTERY_GET_STATUS();
        _new_battery_status.val = _battery_status.val;

        if(_battery_status.plug_status != BATTERY_PLUG_UNAVAILABLE)
            _battery_update_charge();

        return BATTERY_INIT_OK;
    }
    #endif 
    
    return BATTERY_INIT_NOT_SUPPORTED;
}

// Get current battery level. Returns -1 if unsupported.
battery_level_t battery_get_level()
{
    #if defined(HOJA_BATTERY_GET_VOLTAGE)
    return _battery_status.battery_level;
    #else 
    return BATTERY_LEVEL_UNAVAILABLE;
    #endif 
}

// Set the PMIC power source.
bool battery_set_source(battery_source_t source)
{
    #if defined(HOJA_BATTERY_SET_SOURCE)
    return HOJA_BATTERY_SET_SOURCE(source);
    #else 
    (void) source;
    return false;
    #endif 
}

void battery_set_plug(battery_plug_t plug)
{
    _battery_status.plug_status = plug;
}

// Get the PMIC plugged status.
battery_plug_t battery_get_plug()
{
    if(_wired_override) return BATTERY_PLUG_UNAVAILABLE;

    #if defined(HOJA_BATTERY_GET_STATUS)
        return _battery_status.plug_status;
    #else
        return BATTERY_PLUG_UNAVAILABLE;
    #endif
}

// Get the PMIC charging status.
battery_charge_t battery_get_charge()
{
    #if defined(HOJA_BATTERY_GET_STATUS)
        return _battery_status.charge_status;
    #else 
        return BATTERY_CHARGE_UNAVAILABLE;
    #endif
}

// Get the PMIC battery status.
battery_status_t battery_get_battery()
{
    #if defined(HOJA_BATTERY_GET_STATUS)
        return _battery_status.battery_status;
    #else 
        return BATTERY_STATUS_UNAVAILABLE;
    #endif
}

battery_status_s battery_get_status()
{
    return _battery_status;
}

#define BATTERY_TASK_INTERVAL 1000 * 1000 // 0.5 second (microseconds)

bool _lowbat_timeout_shutdown = false;
static int _shutdown_reset = 0;
#define LOWBAT_TIMEOUT_US 6500 * 1000

// This will set the battery loop to indicate critical power
// initiating a shut-down and blinking the LED red for notifications
void battery_set_critical_shutdown()
{
    if(_lowbat_timeout_shutdown) return;

    _lowbat_timeout_shutdown = true;
    rgb_set_idle(true);
    hoja_set_notification_status(COLOR_RED);
    _shutdown_reset = 1; 
}

// PMIC management task.
void battery_task(uint64_t timestamp)
{
    #if defined(HOJA_BATTERY_DRIVER)
    if(_wired_override) return;

    static interval_s interval = {0};
    static interval_s voltage_interval = {0};
    

    if(interval_run(timestamp, BATTERY_TASK_INTERVAL, &interval))
    {
        if(_battery_shutdown_lockout) return;

        // Update status
        HOJA_BATTERY_UPDATE_STATUS();
        _new_battery_status.val = HOJA_BATTERY_GET_STATUS();

        // Check change to battery status
        if( (_battery_status.charge_status       !=  BATTERY_CHARGE_UNAVAILABLE) &&
            (_new_battery_status.charge_status   !=  _battery_status.charge_status) )
        {
            switch(_new_battery_status.charge_status)
            {
                case BATTERY_CHARGE_IDLE:
                    _battery_event_handler(BATTERY_EVENT_CHARGE_STOP);
                break;

                case BATTERY_CHARGE_CHARGING:
                    _battery_event_handler(BATTERY_EVENT_CHARGE_START);
                break;

                case BATTERY_CHARGE_DONE:
                    _battery_event_handler(BATTERY_EVENT_CHARGE_COMPLETE);
                break;

                case BATTERY_CHARGE_DISCHARGING:
                    _battery_event_handler(BATTERY_EVENT_CHARGE_STOP);
                break;

                default:
                case BATTERY_CHARGE_UNAVAILABLE:
                break;
            }
        }

        // Check change to plug status
        if( (_battery_status.plug_status      !=  BATTERY_PLUG_UNAVAILABLE) &&
            (_new_battery_status.plug_status  != _battery_status.plug_status) )
        {
            switch(_new_battery_status.plug_status)
            {
                case BATTERY_PLUG_PLUGGED:
                    _battery_event_handler(BATTERY_EVENT_CABLE_PLUGGED);
                break;

                case BATTERY_PLUG_UNPLUGGED:
                    _battery_event_handler(BATTERY_EVENT_CABLE_UNPLUGGED);
                break;

                default:
                case BATTERY_PLUG_UNAVAILABLE:
                break;
            }
        }

        // Update battery status to new status
        _battery_status.val = _new_battery_status.val;
    }

    #if defined(HOJA_BATTERY_GET_VOLTAGE)
    if(interval_run(timestamp, BATTERY_LEVEL_INTERVAL_US, &voltage_interval))
    {
        if((_battery_status.plug_status == BATTERY_PLUG_PLUGGED) || (_battery_status.plug_status == BATTERY_CHARGE_UNAVAILABLE)) 
        {
            _battery_status.battery_level = BATTERY_LEVEL_HIGH;
            return;
        }

        _battery_update_charge();

        if(_battery_status.battery_level == BATTERY_LEVEL_CRITICAL)
        {
            battery_set_critical_shutdown();
        }
    }

    if(_lowbat_timeout_shutdown)
    {
        static interval_s lowbat_interval = {0};
        if(interval_resettable_run(timestamp, LOWBAT_TIMEOUT_US, (_shutdown_reset==1), &lowbat_interval))
        {
            hoja_deinit(hoja_shutdown);
        }
        _shutdown_reset = 2;
    }
    #endif
    #endif 
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

#include "devices/battery.h"
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
    };

battery_status_s _new_battery_status = {0};

#define BATTERY_LEVEL_INTERVAL_US 5000*1000 // 5 seconds
#define INTERVALS_PER_HOUR (3600 / (BATTERY_LEVEL_INTERVAL_US / 1000000))

// When I measure past the divider, it's 2v when the system is powered on
// The battery voltage is 4.2v when I measure this. The ratio is 

#define VOLTAGE_MEASURE_OFFSET 0.125f
#define DANGER_BATTERY_VOLTAGE 3.575f // 3.5v is the minimum voltage for the battery to be considered "alive"

typedef struct 
{
    float charge_level_ma;
    uint32_t charge_rate_ma;
    float charge_rate_per_interval;
    float discharge_rate_per_interval;
} charge_status_s;

charge_status_s _charge_status = {0};

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

    if(voltage <= DANGER_BATTERY_VOLTAGE)
    {
        if(_battery_status.plug_status == BATTERY_PLUG_UNPLUGGED)
        {
            _battery_event_handler(BATTERY_EVENT_BATTERY_DEAD);
            return -1;
        }
    }

    switch(_battery_status.charge_status)
    {
        case BATTERY_CHARGE_CHARGING:
            _charge_status.charge_level_ma += _charge_status.charge_rate_per_interval;
            if(_charge_status.charge_level_ma >= HOJA_BATTERY_CAPACITY_MAH)
            {
                _charge_status.charge_level_ma = HOJA_BATTERY_CAPACITY_MAH;
            }
        break;

        case BATTERY_CHARGE_DISCHARGING:
            _charge_status.charge_level_ma -= _charge_status.discharge_rate_per_interval;
            if(_charge_status.charge_level_ma <= 0)
            {
                _charge_status.charge_level_ma = 0;
            }
        break;

        case BATTERY_CHARGE_DONE:
            _charge_status.charge_level_ma = HOJA_BATTERY_CAPACITY_MAH;
        break;

        case BATTERY_CHARGE_IDLE:
        default:
        break;

        case BATTERY_CHARGE_UNAVAILABLE:
            return -2;
        break;
    } 

    // Convert and update our status 
    _battery_status.charge_percent = (battery_status_t)((_charge_status.charge_level_ma / HOJA_BATTERY_CAPACITY_MAH) * 100.0f);
    _battery_status.charge_percent = (_battery_status.charge_percent < 5.1f) ? 5.1f : _battery_status.charge_percent;
    #else 
    _battery_status.charge_percent = 100;
    #endif
    return 0;
}

// Init battery PMIC, returns battery_init_t status
int battery_init(bool wired_override)
{
    _wired_override = wired_override;
    if(wired_override) return BATTERY_INIT_WIRED_OVERRIDE;

    if(battery_config->battery_config_version != CFG_BLOCK_BATTERY_VERSION)
    {
        battery_config->battery_config_version = CFG_BLOCK_BATTERY_VERSION;
        battery_config->charge_level_percent = 100.0f;
    }

    #if defined(HOJA_BATTERY_CAPACITY_MAH)
    // Set battery percent status from loaded config value
    _charge_status.charge_level_ma = (float)HOJA_BATTERY_CAPACITY_MAH * (battery_config->charge_level_percent / 100.0f);
    #endif

    #if defined(HOJA_BATTERY_CONSUME_RATE)
    // Calculate discharge rate (HOJA_BATTERY_CONSUME_RATE is how much the device consumes in mA in an hour)
    _charge_status.discharge_rate_per_interval = (float)HOJA_BATTERY_CONSUME_RATE / INTERVALS_PER_HOUR;
    #endif

    #if defined(HOJA_BATTERY_INIT)
    if(HOJA_BATTERY_INIT())
    {
        // Set initial status
        _battery_status.val = HOJA_BATTERY_GET_STATUS();
        _new_battery_status.val = _battery_status.val;

        _battery_update_charge();

        return BATTERY_INIT_OK;
    }
    #endif 
    
    return BATTERY_INIT_NOT_SUPPORTED;
}

// Get current battery level. Returns -1 if unsupported.
int battery_get_level()
{
    #if defined(HOJA_BATTERY_GET_VOLTAGE)

    #else 
    return -1;
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
#define LOWBAT_TIMEOUT_US 5000 * 1000


// PMIC management task.
void battery_task(uint32_t timestamp)
{
    #if defined(HOJA_BATTERY_DRIVER)
    if(_wired_override) return;

    static interval_s interval = {0};
    static interval_s voltage_interval = {0};
    static int _shutdown_reset = 0;

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
        _battery_update_charge();

        if(_battery_status.charge_percent < 5)
        {
            _lowbat_timeout_shutdown = true;
            hoja_set_notification_status(COLOR_RED);
            if(!_shutdown_reset)
                _shutdown_reset = 1;
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
    _charge_status.charge_rate_ma = rate_ma;

    // Calculate charge rate per interval
    _charge_status.charge_rate_per_interval = (float)rate_ma / INTERVALS_PER_HOUR;

    return HOJA_BATTERY_SET_CHARGE_RATE(rate_ma);
    #else 
    return false;
    #endif 
}

// Enable PMIC ship mode (power off with power conservation).
void battery_set_ship_mode()
{
    #if defined(HOJA_BATTERY_SET_SHIP_MODE)
    // Save battery charge percent 
    battery_config->charge_level_percent = _battery_status.charge_percent;
    settings_commit_blocks();
    HOJA_BATTERY_SET_SHIP_MODE();
    #endif
    return;
}

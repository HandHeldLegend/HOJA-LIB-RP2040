#include "devices/battery.h"
#include "utilities/interval.h"
#include "board_config.h"

#if defined(HOJA_BATTERY_DRIVER) && (HOJA_BATTERY_DRIVER==BATTERY_DRIVER_BQ25180)
    #include "drivers/battery/bq25180.h"
#endif

battery_status_s _battery_status = {
    .battery_status = BATTERY_STATUS_UNAVAILABLE,
    .charge_status  = BATTERY_CHARGE_UNAVAILABLE,
    .plug_status    = BATTERY_PLUG_UNAVAILABLE
    };

battery_status_s _new_battery_status;

// Battery event handler
void _event_handler(battery_event_t event)
{
    switch(event)
    {
        case BATTERY_EVENT_CABLE_PLUGGED:
        break;

        case BATTERY_EVENT_CABLE_UNPLUGGED:
        break;

        case BATTERY_EVENT_CHARGE_COMPLETE:
        break;

        case BATTERY_EVENT_CHARGE_START:
        break;
    }
}

// Init battery PMIC
bool battery_init()
{
    #if defined(HOJA_BATTERY_INIT)
    if(HOJA_BATTERY_INIT())
    {
        // Update status
        HOJA_BATTERY_UPDATE_STATUS();
        // Set initial status
        _battery_status = HOJA_BATTERY_GET_STATUS();
        _new_battery_status = _battery_status;
    }
    #else 
    return false;
    #endif 
}

// Get current battery level. Returns -1 if unsupported.
int battery_get_level()
{
    #if defined(HOJA_BATTERY_GET_LEVEL)
    return HOJA_BATTERY_GET_LEVEL();
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

// Get the PMIC plugged status.
battery_plug_t battery_get_plug()
{
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

#define BATTERY_TASK_INTERVAL 1 * 1000 * 1000 // 1 second (1000ms)
// PMIC management task.
void battery_task(uint32_t timestamp)
{
    #if defined(HOJA_BATTERY_DRIVER)
    interval_s interval = {0};

    if(interval_run(timestamp, BATTERY_TASK_INTERVAL, &interval))
    {
        if(_battery_status.plug_status==BATTERY_PLUG_OVERRIDE) return;

        _new_battery_status = HOJA_BATTERY_GET_STATUS();

        // Check change to battery status
        if( _battery_status.charge_status       !=  BATTERY_CHARGE_UNAVAILABLE &&
            _new_battery_status.charge_status   !=  _battery_status.charge_status)
        {
            switch(_new_battery_status.charge_status)
            {
                case BATTERY_CHARGE_IDLE:
                    _event_handler(BATTERY_EVENT_CHARGE_STOP);
                break;

                case BATTERY_CHARGE_CHARGING:
                    _event_handler(BATTERY_EVENT_CHARGE_START);
                break;

                case BATTERY_CHARGE_DONE:
                    _event_handler(BATTERY_EVENT_CHARGE_COMPLETE);
                break;

                case BATTERY_CHARGE_DISCHARGING:
                    _event_handler(BATTERY_EVENT_CHARGE_STOP);
                break;

                default:
                case BATTERY_CHARGE_UNAVAILABLE:
                break;
            }
        }

        // Check change to plug status
        if(_battery_status.plug_status      !=  BATTERY_PLUG_UNAVAILABLE &&
           _new_battery_status.plug_status  != _battery_status.plug_status)
        {
            switch(_new_battery_status.plug_status)
            {
                case BATTERY_PLUG_PLUGGED:
                    _event_handler(BATTERY_EVENT_CABLE_PLUGGED);
                break;

                case BATTERY_PLUG_UNPLUGGED:
                    _event_handler(BATTERY_EVENT_CABLE_UNPLUGGED);
                break;

                default:
                case BATTERY_PLUG_UNAVAILABLE:
                break;
            }
        }

        // Update battery status to new status
        _battery_status = _new_battery_status;
    }
    
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
bool battery_set_ship_mode()
{
    #if defined(HOJA_BATTERY_SET_SHIP_MODE)
    return HOJA_BATTERY_SET_SHIP_MODE();
    #else 
    return false;
    #endif
}

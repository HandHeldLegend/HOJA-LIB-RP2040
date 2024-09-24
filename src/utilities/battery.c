#include "battery.h"
#include "interval.h"

/**
 * We are implementing a new Battery general activity task.
 * This will allow us to perform general functions such as
 * fuel gauge processing and updating
*/
#define BATTERY_CHARGE_TIME_DEFAULT_MINUTES 180
#define TIME_MINUTE_SECONDS (60)
#define TIME_MINUTE_US (TIME_MINUTE_SECONDS * 1000 * 1000) // 60 * 1000ms * 1000us

typedef enum
{
    CHARGE_STATUS_UNAVAILABLE = -2,
    CHARGE_STATUS_DISCHARGING = -1,
    CHARGE_STATUS_NEUTRAL = 0,
    CHARGE_STATUS_CHARGING = 1,
    CHARGE_STATUS_FULL = 2,
} charge_status_t;

typedef enum 
{
    PLUG_STATUS_UNAVAILABLE = -1,
    PLUG_STATUS_UNPLUGGED = 0,
    PLUG_STATUS_PLUGGED = 1,
} plug_status_t;

charge_status_t charging_status = CHARGE_STATUS_NEUTRAL;
bool charging_disabled = false;

plug_status_t last_plug_status = PLUG_STATUS_UNPLUGGED;
plug_status_t plug_status = PLUG_STATUS_UNPLUGGED;

uint16_t battery_get_level()
{
    return global_loaded_battery_storage.charge_level;
}

// Run less often
void battery_update_status()
{
    BQ25180_status_s raw_status = {0};

    // We can poll the PMIC to get our plug status
    uint8_t _getstatus[1] = {0x00};
    uint8_t _readstatus[1] = {0x00};
    
    int readcheck = i2c_safe_write_read_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _getstatus, 1, _readstatus, 1, 10000);

    if ((readcheck == PICO_ERROR_GENERIC) || (readcheck == PICO_ERROR_TIMEOUT))
    {
        
    }
    else
    {
        raw_status.status = _readstatus[0];
        plug_status = raw_status.plug_status ? PLUG_STATUS_PLUGGED : PLUG_STATUS_UNPLUGGED;
    }

    switch(raw_status.charge_status)
    {
        default:
        // Not charging while charging is enabled
        case 0b00:  
            
            if(plug_status == PLUG_STATUS_UNPLUGGED)
                charging_status = CHARGE_STATUS_DISCHARGING;
            else charging_status = CHARGE_STATUS_NEUTRAL;
            break;

        // Constant Current Charging
        case 0b01:
        case 0b10:
            charging_status = CHARGE_STATUS_CHARGING;
            break; 

        // Charge Done or charging is disabled by the host
        case 0b11:
            if(charging_disabled)
            {
                if(plug_status == PLUG_STATUS_UNPLUGGED)
                    charging_status = CHARGE_STATUS_DISCHARGING;
                else charging_status = CHARGE_STATUS_NEUTRAL;
            }
            else
            {
                charging_status = CHARGE_STATUS_FULL;
            }
            break;
    }
}

void battery_monitor_task(uint32_t timestamp)
{
    static bool battery_monitor_init = false;

    static interval_s battery_charge_interval = {0};
    static interval_s battery_status_interval = {0};

    static float    power_per_minute = 10.0f;
    static float    charge_level_fine = 1000.0f;
    static bool     controller_idle_state = false;

    if(!battery_monitor_init)
    {
        battery_monitor_init = true;

        charge_level_fine = (float) global_loaded_battery_storage.charge_level;
        
        // Get depletion rate in minutes
        float depletion_rate = (HOJA_POWER_CONSUMPTION_SOURCE/HOJA_POWER_CONSUMPTION_RATE) * 60;
        power_per_minute = 1000.0f / depletion_rate;

        battery_update_status(); // Get our initial status
        battery_update_status();
        battery_update_status();
        last_plug_status = plug_status;
    }

    // Run once per second
    if(interval_run(timestamp, 1000000, &battery_status_interval))
    {
        battery_update_status();

        if(last_plug_status != plug_status)
        {
            switch(plug_status)
            {
                case PLUG_STATUS_UNPLUGGED:
                    // Unplug during use, shut down
                    if(hoja_get_input_method() == INPUT_METHOD_WIRED) return;
                    hoja_deinit(hoja_shutdown);
                    return;
                break;

                case PLUG_STATUS_PLUGGED:
                    // We are now charging or just plugged. Enjoy
                break;
            }

            last_plug_status = plug_status;
        }
    
        // Idle animation handle
        if(!controller_idle_state && hoja_get_idle_state())
        {
            switch(charging_status)
            {
                default:
                case CHARGE_STATUS_UNAVAILABLE:
                case CHARGE_STATUS_NEUTRAL:
                case CHARGE_STATUS_DISCHARGING:
                break;

                case CHARGE_STATUS_CHARGING:
                    controller_idle_state = true;
                    rgb_flash(COLOR_YELLOW.color, 100);
                    break;

                case CHARGE_STATUS_FULL:
                    controller_idle_state = true;
                    rgb_flash(COLOR_BLUE.color, 100);
                    break;

                //case CHARGE_STATUS_ERROR:
                //    controller_idle_state = true;
                //    rgb_flash(COLOR_RED.color, 100);
                //    break;
            }
        }
        else if(controller_idle_state && !hoja_get_idle_state())
        {
            controller_idle_state = false;
            rgb_init(global_loaded_settings.rgb_mode, BRIGHTNESS_RELOAD);
        }
    
    }

    // Update charge status once per min
    if(interval_run(timestamp, TIME_MINUTE_US, &battery_charge_interval))
    {
        battery_update_status();

        switch(charging_status)
        {
            default:
            case CHARGE_STATUS_NEUTRAL:
            case CHARGE_STATUS_UNAVAILABLE:
            break;

            case CHARGE_STATUS_DISCHARGING:
            {
                charge_level_fine -= power_per_minute;
                charge_level_fine = (charge_level_fine < 0) ? 0 : charge_level_fine;
                settings_set_charge_level((uint16_t) charge_level_fine);
            }
            break;

            case CHARGE_STATUS_CHARGING:
                // Increase our charge level
                charge_level_fine += power_per_minute;
                charge_level_fine = (charge_level_fine > 1000.0f) ? 1000.0f : charge_level_fine;
                settings_set_charge_level((uint16_t) charge_level_fine);
            break;

            case CHARGE_STATUS_FULL:
                settings_set_charge_level(1000); // Fully charged at charging done
            break;
        }
    }
}

bool battery_comms_check()
{
    #if (HOJA_CAPABILITY_BATTERY == 1)
        if(plug_status == PLUG_STATUS_UNAVAILABLE) return false;

        // We can poll the PMIC to get our plug status
        uint8_t _getstatus[1] = {0x00};
        uint8_t _readstatus[1] = {0x00};
        BQ25180_status_s _status_converted;
        int readcheck = i2c_safe_write_read_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _getstatus, 1, _readstatus, 1, 10000);
        if(readcheck < 1) return false;
        return true;
    #else
        return false;
    #endif
}

void battery_enable_ship_mode()
{
    #if (HOJA_CAPABILITY_BATTERY == 1)
    if(plug_status == PLUG_STATUS_UNAVAILABLE) return;

    // Try to save our battery status
    if(!get_core_num())
        settings_save_battery_from_core0();
    else
    {   
        settings_save_battery_from_core1();
    }   

    int s2 = 0;
    int s1 = 0;
    uint32_t attempts = 30;
    while(!s2 && attempts > 0)
    {

        //const uint8_t _data1[2] = {0x0A, 0b01001100};
        //s1 = i2c_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _data1, 2, false, 10000);

        const uint8_t _data[2] = {0x09, 0b11000001};
        s2 = i2c_safe_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _data, 2, false, 100000);

        if(s2 == PICO_ERROR_GENERIC)
        {
            //hoja_shutdown_instant();
        }
        else if (s2== PICO_ERROR_TIMEOUT)
        {
            //hoja_shutdown_instant();
        }
        else if (s2==2)
        {
        }

        sleep_ms(50);
        watchdog_update();

        attempts--;
    }
    #endif

    hoja_shutdown_instant();

}

uint16_t charge_rate = 0;
void battery_set_charge_rate(uint16_t rate_ma)
{
    #if (HOJA_CAPABILITY_BATTERY == 1)
    if(plug_status == PLUG_STATUS_UNAVAILABLE) return;

    // Default is 0x5
    uint8_t code = 0x5;
    uint16_t rate = rate_ma;

    // Handle charge rate disabling
    if(!rate_ma)
    {
        charging_disabled = true;
        code = 0b10000101;
    }
    else
    {
        charging_disabled = false;
        charge_rate = rate_ma;
        if (rate_ma <= 35)
        {
            // CODE + 5 = rate
            code = rate - 5;
        }
        else
        {
            if (rate > 350)
            {
                rate = 350;
            }
            // 40 + ((CODE-31)*10) = rate
            code = ((rate - 40) / 10) + 31;
        }

        // Mask off the code to ensure charging isn't disabled
        code &= 0x7F;

        uint8_t _write[2] = {0x04, code};
        int readcheck = i2c_safe_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _write, 2, false, 10000);
    }
    #endif
}

// This disables the PMIC regulation by default. We already have a regulator
void battery_set_source(BQ25180_battery_source_t source)
{
    #if (HOJA_CAPABILITY_BATTERY == 1)
    if(plug_status == PLUG_STATUS_UNAVAILABLE) return;

    uint8_t source_packet = 0b11100000;
    switch(source)
    {
        default:
        case PMIC_SOURCE_AUTO:
        case PMIC_SOURCE_EXT:
        break;
        case PMIC_SOURCE_BAT:
            source_packet |= 0b00000100;
        break;
    }
    const uint8_t _data[2] = {0x0A, source_packet};
    bool wrote_ok = false;

    while(!wrote_ok)
    {
        int success = i2c_safe_write_timeout_us(HOJA_I2C_BUS, BATTYPE_BQ25180, _data, 2, false, 10000);
        if(success==2)
        {
            wrote_ok = true;
        }
    }
    #endif 
}

int8_t battery_get_plugged_status()
{
    battery_update_status();
    return plug_status;
}

int8_t battery_get_charging_status()
{
    battery_update_status();
    return charging_status;
}
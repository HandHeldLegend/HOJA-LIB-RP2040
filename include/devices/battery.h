#ifndef DEVICES_BATTERY_H
#define DEVICES_BATTERY_H

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"

#if defined(HOJA_BATTERY_DRIVER)
    //#ifndef HOJA_BATTERY_CAPACITY_MAH
    //    #error "HOJA_BATTERY_CAPACITY_MAH undefined in board_config.h" 
    //#endif 
    //#ifndef HOJA_BATTERY_DRAWRATE_MA
    //    #error "HOJA_BATTERY_DRAWRATE_MA undefined in board_config.h" 
    //#endif
#endif

typedef enum 
{
    BATTERY_INIT_OK,
    BATTERY_INIT_NOT_SUPPORTED,
    BATTERY_INIT_LOW_VOLTAGE,
    BATTERY_INIT_WIRED_OVERRIDE
} battery_init_t;

typedef enum
{
    BATTERY_CHARGE_UNAVAILABLE = -1,
    BATTERY_CHARGE_IDLE,
    BATTERY_CHARGE_DISCHARGING,
    BATTERY_CHARGE_CHARGING,
    BATTERY_CHARGE_DONE,
} battery_charge_t;

typedef enum
{
    BATTERY_PLUG_UNAVAILABLE = -1,
    BATTERY_PLUG_UNPLUGGED,
    BATTERY_PLUG_PLUGGED,
} battery_plug_t;

typedef enum
{
    BATTERY_STATUS_UNAVAILABLE = -1,
    BATTERY_STATUS_DISCHARGED,
    BATTERY_STATUS_CONNECTED
} battery_status_t;

typedef enum 
{
    BATTERY_LEVEL_UNAVAILABLE = -1,
    BATTERY_LEVEL_CRITICAL,
    BATTERY_LEVEL_LOW,
    BATTERY_LEVEL_MID,
    BATTERY_LEVEL_HIGH
} battery_level_t;

#define VOLTAGE_LEVEL_CRITICAL  3.125f
#define VOLTAGE_LEVEL_LOW       3.3f
#define VOLTAGE_LEVEL_MID       3.975f

typedef struct
{
    union 
    {
        struct 
        {
            int8_t plug_status; // Plug Status Format (0 = Unplugged, 1 = Plugged)
            int8_t charge_status; 
            int8_t battery_status; 
            int8_t battery_level; 
        };
        uint32_t val;
    }; 
} battery_status_s;

typedef enum
{
    BATTERY_SOURCE_AUTO,
    BATTERY_SOURCE_EXTERNAL,
    BATTERY_SOURCE_BATTERY,
} battery_source_t;

typedef enum
{
    BATTERY_EVENT_CABLE_PLUGGED,
    BATTERY_EVENT_CABLE_UNPLUGGED,
    BATTERY_EVENT_BATTERY_DEAD,
    BATTERY_EVENT_CHARGE_START, 
    BATTERY_EVENT_CHARGE_STOP,
    BATTERY_EVENT_CHARGE_COMPLETE,
} battery_event_t;

void                battery_set_critical_shutdown();
int                 battery_init(bool wired_override); 
battery_level_t     battery_get_level(); 
bool                battery_set_source(battery_source_t source); 
void                battery_set_plug(battery_plug_t plug);
battery_status_s    battery_get_status();
battery_plug_t      battery_get_plug();   
battery_charge_t    battery_get_charge();  
battery_status_t    battery_get_battery(); 
void                battery_task(uint64_t timestamp); 
bool                battery_set_charge_rate(uint16_t rate_ma); 
void                battery_set_ship_mode(); 

#endif
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

typedef struct 
{
    bool connected; // If we have a battery management IC connected (can also be used to disable function)
    bool charging;  // If we are charging or not
    bool plugged;   // If we are plugged in to good power or not
} battery_status_s;

battery_status_s battery_get_status(void);
void battery_set_connected(bool connected);
void battery_set_charging(bool charging);
void battery_set_plugged(bool plugged);

void battery_cmd_shutdown();

typedef struct
{
    union 
    {
        struct 
        {
            int8_t plug_status; // Plug Status Format (0 = Unplugged, 1 = Plugged)
            int8_t charge_status;  // battery_charge_t
            int8_t battery_status; // battery_status_t
            int8_t battery_level;  // battery_level_t
        };
        uint32_t val;
    }; 
} battery_status_old_s;

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
void                battery_task(uint64_t timestamp); 
bool                battery_set_charge_rate(uint16_t rate_ma); 
void                battery_set_ship_mode(); 

#endif
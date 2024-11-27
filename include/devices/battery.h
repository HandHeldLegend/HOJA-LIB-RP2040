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
    BATTERY_PLUG_OVERRIDE, // For modes where we don't want to rely on drivers
} battery_plug_t;

typedef enum
{
    BATTERY_STATUS_UNAVAILABLE = -1,
    BATTERY_STATUS_DISCHARGED,
    BATTERY_STATUS_CONNECTED
} battery_status_t;

typedef struct
{
    battery_plug_t      plug_status;
    battery_charge_t    charge_status;
    battery_status_t    battery_status;
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

bool                battery_init(); // Init battery PMIC
int                 battery_get_level();    // Get current battery level.
bool                battery_set_source(battery_source_t source); // Set the PMIC power source.
battery_plug_t      battery_get_plug();     // Get the PMIC plugged status.
battery_charge_t    battery_get_charge();   // Get the PMIC charging status.
battery_status_t    battery_get_battery();  // Get the PMIC battery status.
void                battery_task(uint32_t timestamp); // PMIC management task.
bool                battery_set_charge_rate(uint16_t rate_ma); // Set PMIC charge rate.
bool                battery_set_ship_mode(); // Enable PMIC ship mode (power off).
void                battery_event_handler(battery_event_t event);

#endif
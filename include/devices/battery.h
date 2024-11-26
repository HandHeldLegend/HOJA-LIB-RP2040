#ifndef DEVICES_BATTERY_H
#define DEVICES_BATTERY_H

#include <stdint.h>
#include <stdbool.h>

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
    BATTERY_PLUG_UNAVAILALBE = -1,
    BATTERY_PLUG_UNPLUGGED,
    BATTERY_PLUG_PLUGGED
} battery_plug_t;

typedef enum
{
    BATTERY_SOURCE_AUTO,
    BATTERY_SOURCE_EXTERNAL,
    BATTERY_SOURCE_BATTERY,
} battery_source_t;

bool                battery_init(); // Init battery PMIC
int                 battery_get_level();    // Get current battery level.
bool                battery_set_source(battery_source_t source); // Set the PMIC power source.
battery_plug_t      battery_get_plug();     // Get the PMIC plugged status.
battery_charge_t    battery_get_charge();   // Get the PMIC charging status.
void                battery_task(uint32_t timestamp); // PMIC management task.
bool                battery_set_charge_rate(uint16_t rate_ma); // Set PMIC charge rate.
bool                battery_set_ship_mode(); // Enable PMIC ship mode (power off).

#endif
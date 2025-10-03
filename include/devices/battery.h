#ifndef DEVICES_BATTERY_H
#define DEVICES_BATTERY_H

#include <stdint.h>
#include <stdbool.h>

typedef struct 
{
    bool connected; // If we have a battery management IC connected (can also be used to disable function if left as false)
    bool charging;  // If we are charging or not
    bool charging_done; // Sets to true when charging is completed
    bool plugged;   // If we are plugged in to good power or not
} battery_status_s;

typedef enum
{
    BATTERY_SOURCE_AUTO,
    BATTERY_SOURCE_EXTERNAL,
    BATTERY_SOURCE_BATTERY,
} battery_source_t;

void battery_update_status(void);
void battery_get_status(battery_status_s *out); 
void battery_set_critical_shutdown(void); 
bool battery_init(void); 
bool battery_set_charge_rate(uint16_t rate_ma); 
void battery_set_ship_mode(void); 

#endif
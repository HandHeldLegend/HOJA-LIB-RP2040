#ifndef HOJA_DRIVER_BATTERY_H
#define HOJA_DRIVER_BATTERY_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

void driver_battery_enable_shipmode();

bool driver_battery_comms_check();

void driver_battery_update_status();

void driver_battery_current_status();

uint16_t driver_battery_get_level();

#endif
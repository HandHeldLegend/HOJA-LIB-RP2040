#ifndef HOJA_ANALOG_H
#define HOJA_ANALOG_H

#include <stdint.h>

#include "utilities/interval.h"
#include "input/button.h"
#include "settings_shared_types.h"

#define ANALOG_POLL_INTERVAL 500 

#define ANALOG_DATA_UNSET   0xFFFF

typedef enum
{
    ANALOG_ACCESS_RAW_DATA,     // Access raw analog data
    ANALOG_ACCESS_SCALED_DATA,  // Access scaled analog data
    ANALOG_ACCESS_SNAPBACK_DATA, // Access analog data post-snapback filter
    ANALOG_ACCESS_DEADZONE_DATA, // Access analog data post-deadzone application
} analog_access_t;

void analog_angle_distance_to_coordinate(float angle, float distance, int16_t *out);
void analog_init();
void analog_access_blocking(analog_data_s *out, analog_access_t type);
bool analog_access_try(analog_data_s *out, analog_access_t type);

void analog_config_command(analog_cmd_t cmd, webreport_cmd_confirm_t cb);
void analog_task(uint32_t timestamp);

#endif
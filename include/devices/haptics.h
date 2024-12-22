#ifndef DEVICES_HAPTICS_H
#define DEVICES_HAPTICS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    RUMBLE_TYPE_ERM = 0,
    RUMBLE_TYPE_LRA = 1,
    RUMBLE_TYPE_MAX,
} rumble_type_t;

typedef enum
{
  RUMBLE_OFF,
  RUMBLE_BRAKE,
  RUMBLE_ON,
} rumble_t;

void haptics_set_basic(float amplitude);

bool haptics_init();
void haptics_task(uint32_t timestamp);

#endif
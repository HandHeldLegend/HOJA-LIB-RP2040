#ifndef DEVICES_HAPTICS_H
#define DEVICES_HAPTICS_H

#include <stdint.h>
#include <stdbool.h>
#include "settings_shared_types.h"
#include "devices_shared_types.h"

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

void haptic_config_cmd(haptic_cmd_t cmd, webreport_cmd_confirm_t cb);
void haptics_set_hd(haptic_packet_s *packet);
void haptics_set_std(uint8_t amplitude, bool brake);
void haptics_stop();
bool haptics_init();
void haptics_task(uint64_t timestamp);

#endif
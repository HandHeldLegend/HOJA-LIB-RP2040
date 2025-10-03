#ifndef HOJA_TRANSPORT_H
#define HOJA_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>

#include "utilities/crosscore_snapshot.h"

typedef enum
{
    TRANSPORT_MODE_UNDEFINED = -2,
    TRANSPORT_MODE_LOAD     = -1, // Firmware load (bluetooth)
    TRANSPORT_MODE_SWPRO    = 0,
    TRANSPORT_MODE_XINPUT   = 1,
    TRANSPORT_MODE_GCUSB    = 2,
    TRANSPORT_MODE_GAMECUBE = 3,
    TRANSPORT_MODE_N64      = 4,
    TRANSPORT_MODE_SNES     = 5,
    TRANSPORT_MODE_SINPUT   = 6,
    TRANSPORT_MODE_MAX,
} transport_mode_t;

typedef struct 
{
    transport_mode_t mode;
    bool gyro_supported;
    bool haptics_supported;
    bool bluetooth_supported;
    bool bluetooth_pair;
} transport_profile_s;

typedef struct
{
    bool connected; // If gamepad is connected or not to a HOST
    uint8_t player_number; // Player number 0-8
    uint32_t polling_rate_us; // Polling rate of the gamepad
    bool wireless; // Whether or not the gamepad has wireless active
} transport_status_s;

transport_status_s transport_get_status(void);

void transport_set_connected(bool connected);
void transport_set_player_number(uint8_t player_number);
void transport_set_polling_rate_us(uint32_t polling_rate_us);

void transport_init(transport_profile_s profile);
transport_profile_s* transport_active_profile();

void transport_task(uint64_t timestamp);

#endif
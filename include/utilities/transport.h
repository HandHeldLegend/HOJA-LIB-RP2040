#ifndef HOJA_TRANSPORT_H
#define HOJA_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    bool connected; // If gamepad is connected or not to a HOST
    uint8_t player_number; // Player number 0-8
    uint32_t polling_rate_us; // Polling rate of the gamepad
    bool wireless; // Whether or not the gamepad has wireless active
} transport_status_s;

void transport_get_status(transport_status_s *out);

void transport_set_connected(bool connected);
void transport_set_player_number(uint8_t player_number);
void transport_set_polling_rate_us(uint32_t polling_rate_us);

#endif
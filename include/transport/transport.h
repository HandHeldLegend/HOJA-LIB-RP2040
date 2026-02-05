#ifndef TRANSPORT_TRANSPORT_H
#define TRANSPORT_TRANSPORT_H 

#include <stdbool.h>
#include <stdint.h>
#include "hoja_shared_types.h"

#include "cores/cores.h"

typedef void (*transport_task_t)(uint64_t timestamp);

typedef enum
{
    TP_EVT_PLAYERLED,
    TP_EVT_CONNECTIONCHANGE, 
    TP_EVT_ERMRUMBLE,
    TP_EVT_POWERCOMMAND,
} tp_evt_t;

typedef enum 
{
    TP_CONNECTION_NONE,
    TP_CONNECTION_CONNECTING,
    TP_CONNECTION_CONNECTED,
    TP_CONNECTION_DISCONNECTED,
} tp_connectionchange_t;

typedef enum
{
    TP_POWERCOMMAND_SHUTDOWN,
    TP_POWERCOMMAND_REBOOT,
} tp_powercommand_t;

typedef struct
{
    uint8_t player_number;
} tp_evt_playerled_s;

typedef struct
{
    tp_connectionchange_t connection;
} tp_evt_connectionchange_s;

typedef struct
{
    uint8_t left;
    uint8_t right;
    uint8_t leftbrake;
    uint8_t rightbrake;
} tp_evt_ermrumble_s;

typedef struct 
{
    tp_powercommand_t power_command;
} tp_evt_powercommand_s;

typedef struct
{
    tp_evt_t evt;
    union 
    {
        tp_evt_playerled_s          evt_playernumber;
        tp_evt_connectionchange_s   evt_connectionchange;
        tp_evt_ermrumble_s          evt_ermrumble;
        tp_evt_powercommand_s       evt_powercommand;
    };
} tp_evt_s;

void transport_evt_cb(tp_evt_s evt);
bool transport_init(core_params_s *params);
void transport_task(uint64_t timestamp);

#endif
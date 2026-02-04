#include "transport/transport.h"
#include "cores/cores.h"

#include "transport/transport_usb.h"
#include "transport/transport_bt.h"
#include "transport/transport_joybus64.h"
#include "transport/transport_joybusgc.h"
#include "transport/transport_nesbus.h"
#include "transport/transport_wlan.h"

typedef void (*transport_task_t)(uint64_t timestamp);

typedef enum
{
    TP_EVT_PLAYERASSIGN,
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

void _transport_playerassign(uint8_t player)
{

}

void _transport_connectionchange(uint8_t status)
{
    switch(status)
    {
        case TP_CONNECTION_NONE:
        break;

        case TP_CONNECTION_CONNECTING:
        break;

        case TP_CONNECTION_CONNECTED:
        break;

        case TP_CONNECTION_DISCONNECTED:
        break;
    }
}

void _transport_ermrumble(uint8_t left, uint8_t right)
{

}

void _transport_powercommand(uint8_t command)
{

}

void transport_evt_cb(uint8_t *data, uint16_t len)
{
    tp_evt_t evt = (tp_evt_t)data[0];

    switch(evt)
    {
        case TP_EVT_PLAYERASSIGN:
        _transport_playerassign(data[1]);
        break;

        case TP_EVT_CONNECTIONCHANGE:
        _transport_connectionchange(data[1]);
        break;

        case TP_EVT_ERMRUMBLE:
        _transport_ermrumble(data[1], data[2]);
        break;

        case TP_EVT_POWERCOMMAND:
        _transport_powercommand(data[1]);
        break;
    }
}

bool transport_init(core_params_s *params)
{
    switch(params->gamepad_transport)
    {
        case GAMEPAD_TRANSPORT_USB:
        if(transport_usb_init())
        {
            params->transport_task = transport_usb_task;
            return true;
        }
        else return false;
        break;
        
        case GAMEPAD_TRANSPORT_JOYBUS64:
        if(transport_jb64_init())
        {
            params->transport_task = transport_jb64_task;
            return true;
        }
        else return false;
        break;

        case GAMEPAD_TRANSPORT_JOYBUSGC:
        if(transport_jbgc_init())
        {
            params->transport_task = transport_jbgc_task;
            return true;
        }
        else return false;
        break;

        case GAMEPAD_TRANSPORT_BLUETOOTH:
        if(transport_bt_init())
        {
            params->transport_task = transport_bt_task;
            return true;
        }
        else return false;
        break;

        case GAMEPAD_TRANSPORT_WLAN:
        if(transport_wlan_init())
        {
            params->transport_task = transport_wlan_task;
            return true;
        }
        else return false;
        break;

        default:
        return false;
    }

    // Fallthrough
    return false;
}
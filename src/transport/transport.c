#include "transport/transport.h"
#include "cores/cores.h"

#include <stdlib.h>

#include "board_config.h"

#include "transport/transport_usb.h"
#include "transport/transport_bt.h"
#include "transport/transport_joybus64.h"
#include "transport/transport_joybusgc.h"
#include "transport/transport_nesbus.h"
#include "transport/transport_wlan.h"

void _transport_playerled(uint8_t led)
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

void _transport_ermrumble(uint8_t left, uint8_t right, uint8_t leftbrake, uint8_t rightbrake)
{

}

void _transport_powercommand(uint8_t command)
{

}

void transport_evt_cb(tp_evt_s evt)
{
    tp_evt_t evt_name = evt.evt;

    switch(evt_name)
    {
        case TP_EVT_PLAYERLED:
        _transport_playerled(evt.evt_playernumber.player_number);
        break;

        case TP_EVT_CONNECTIONCHANGE:
        _transport_connectionchange(evt.evt_connectionchange.connection);
        break;

        case TP_EVT_ERMRUMBLE:
        _transport_ermrumble(
            evt.evt_ermrumble.left, evt.evt_ermrumble.right, 
            evt.evt_ermrumble.leftbrake, evt.evt_ermrumble.rightbrake);
        break;

        case TP_EVT_POWERCOMMAND:
        _transport_powercommand(evt.evt_powercommand.power_command);
        break;
    }
}

typedef void (*transport_stop_cb_t)(void);

transport_stop_cb_t _tp_stop_cb = NULL;

void transport_stop()
{
    if(_tp_stop_cb)
    {
        _tp_stop_cb();
        _tp_stop_cb = NULL;
    }
}

bool transport_init(core_params_s *params)
{
    switch(params->transport_type)
    {   
        #if defined(HOJA_TRANSPORT_USB_DRIVER)
        case GAMEPAD_TRANSPORT_USB:
        if(transport_usb_init(params))
        {
            _tp_stop_cb = transport_usb_stop;
            params->transport_task = transport_usb_task;
            return true;
        }
        else return false;
        break;
        #endif
        
        #if defined(HOJA_TRANSPORT_JOYBUS64_DRIVER)
        case GAMEPAD_TRANSPORT_JOYBUS64:
        if(transport_jb64_init(params))
        {
            _tp_stop_cb = NULL;
            params->transport_task = transport_jb64_task;
            return true;
        }
        else return false;
        #endif

        #if defined(HOJA_TRANSPORT_JOYBUSGC_DRIVER)
        case GAMEPAD_TRANSPORT_JOYBUSGC:
        if(transport_jbgc_init(params))
        {
            _tp_stop_cb = NULL;
            params->transport_task = transport_jbgc_task;
            return true;
        }
        else return false;
        #endif

        #if defined(HOJA_TRANSPORT_NESBUS_DRIVER)
        case GAMEPAD_TRANSPORT_NESBUS:
        if(transport_nesbus_init(params))
        {
            _tp_stop_cb = NULL;
            params->transport_task = transport_nesbus_task;
            return true;
        }
        else return false;
        #endif

        #if defined(HOJA_TRANSPORT_BT_DRIVER)
        case GAMEPAD_TRANSPORT_BLUETOOTH:
        if(transport_bt_init(params))
        {
            _tp_stop_cb = transport_bt_stop;
            params->transport_task = transport_bt_task;
            return true;
        }
        else return false;
        #endif

        #if defined(HOJA_TRANSPORT_WLAN_DRIVER)
        case GAMEPAD_TRANSPORT_WLAN:
        if(transport_wlan_init(params))
        {
            _tp_stop_cb = transport_wlan_stop;
            params->transport_task = transport_wlan_task;
            return true;
        }
        else return false;
        #endif

        default:
        return false;
    }

    // Fallthrough
    return false;
}

#include "transport/transport.h"
#include "cores/cores.h"

#include <hoja.h>
#include <stdlib.h>
#include <string.h>

#include "board_config.h"

#include "transport/transport_usb.h"
#include "transport/transport_bt.h"
#include "transport/transport_joybus64.h"
#include "transport/transport_joybusgc.h"
#include "transport/transport_nesbus.h"
#include "transport/transport_wlan.h"

#include "devices/haptics.h"
#include "utilities/settings.h"
#include "utilities/tasks.h"

void _transport_playerled(uint8_t led)
{
    hoja_set_player_number(led);
}

void _transport_connectionchange(uint8_t status)
{
    switch(status)
    {
        case TP_CONNECTION_NONE:
        hoja_set_connected_status(CONNECTION_STATUS_DOWN);
        break;

        case TP_CONNECTION_CONNECTED:
        hoja_set_connected_status(CONNECTION_STATUS_CONNECTED);
        break;

        case TP_CONNECTION_DISCONNECTED:
        hoja_set_connected_status(CONNECTION_STATUS_DISCONNECTED);
        break;
    }
}

void _transport_ermrumble(uint8_t left, uint8_t right, uint8_t leftbrake, uint8_t rightbrake)
{
    haptics_set_std(left>right?left:right, (!left&&!right)?true:false);
}

void _transport_powercommand(uint8_t command)
{
    switch(command)
    {
        case TP_POWERCOMMAND_REBOOT:
        break;

        case TP_POWERCOMMAND_SHUTDOWN:
        hoja_deinit(hoja_shutdown);

        case TP_POWERCOMMAND_LOWPOWER:
        // TODO
        break;
    }
}

void _transport_imucommand(uint8_t command)
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

        case TP_EVT_IMUCOMMAND:
        _transport_imucommand(evt.evt_imucommand.imu_command);
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

// Forward declaration
void _transport_autoinit_cb();
transport_autoinit_sm_s _tp_autoinit_sm = {.result_ready_cb = _transport_autoinit_cb, .state = TP_AUTOINIT_IDLE};

void _transport_autoinit_cb()
{
    switch(_tp_autoinit_sm.phase)
    {
        
    }
}

void transport_autoinit(transport_autoinit_state_t *sm, core_params_s *params)
{

}

void _transport_set_mac(uint8_t *out, core_reportformat_t reportformat)
{
    memcpy(out, gamepad_config->gamepad_mac_address, 6);
    out[5] += reportformat;
}

typedef enum
{
    IMU_MODE_OFF = 0,
    IMU_MODE_RAW = 1,
    IMU_MODE_QUATERNION = 2,
} imu_mode_t;

typedef struct 
{
    int player_number;
    int connection_status;
    imu_mode_t imu_mode;
    core_gyro_task_t imu_task;
    uint16_t boot_flags;
} core_sm_s;

// A transport is supported when the board declares its driver in board_config.h
// (HOJA_TRANSPORT_*_DRIVER). That same gate compiles the transport's platform
// HAL, so support is inferred directly from the driver's existence rather than
// a separate runtime list. Transports without a declared driver are rejected.
static bool _transport_supported(gamepad_transport_t type)
{
    switch(type)
    {
#if defined(HOJA_TRANSPORT_USB_DRIVER)
        case GAMEPAD_TRANSPORT_USB:       return true;
#endif
#if defined(HOJA_TRANSPORT_BT_DRIVER)
        case GAMEPAD_TRANSPORT_BLUETOOTH: return true;
#endif
#if defined(HOJA_TRANSPORT_WLAN_DRIVER)
        case GAMEPAD_TRANSPORT_WLAN:      return true;
#endif
#if defined(HOJA_TRANSPORT_NESBUS_DRIVER)
        case GAMEPAD_TRANSPORT_NESBUS:    return true;
#endif
#if defined(HOJA_TRANSPORT_JOYBUS64_DRIVER)
        case GAMEPAD_TRANSPORT_JOYBUS64:  return true;
#endif
#if defined(HOJA_TRANSPORT_JOYBUSGC_DRIVER)
        case GAMEPAD_TRANSPORT_JOYBUSGC:  return true;
#endif
        default: break;
    }
    return false;
}

bool transport_init(core_params_s *params)
{
    switch(params->core_report_format)
    {
        case CORE_REPORTFORMAT_SWPRO:
        memcpy(params->transport_host_mac, gamepad_config->host_mac_switch, 6);
        tasks_set_motion_interval(params->core_pollrate_us);
        break;

        case CORE_REPORTFORMAT_SINPUT:
        memcpy(params->transport_host_mac, gamepad_config->host_mac_sinput, 6);
        tasks_set_motion_interval(params->core_pollrate_us);
        break;
    }

    _transport_set_mac(params->transport_dev_mac, params->core_report_format);

    if(!_transport_supported(params->transport_type))
        return false;

    switch(params->transport_type)
    {
        case GAMEPAD_TRANSPORT_USB:
        if(transport_usb_init(params))
        {
            _tp_stop_cb = transport_usb_stop;
            params->transport_task = transport_usb_task;
            return true;
        }
        else return false;

        case GAMEPAD_TRANSPORT_JOYBUS64:
        if(transport_jb64_init(params))
        {
            _tp_stop_cb = NULL;
            params->transport_task = transport_jb64_task;
            return true;
        }
        else return false;

        case GAMEPAD_TRANSPORT_JOYBUSGC:
        if(transport_jbgc_init(params))
        {
            _tp_stop_cb = NULL;
            params->transport_task = transport_jbgc_task;
            return true;
        }
        else return false;

        case GAMEPAD_TRANSPORT_NESBUS:
        if(transport_nesbus_init(params))
        {
            _tp_stop_cb = NULL;
            params->transport_task = transport_nesbus_task;
            return true;
        }
        else return false;

        case GAMEPAD_TRANSPORT_BLUETOOTH:
        if(transport_bt_init(params))
        {
            _tp_stop_cb = transport_bt_stop;
            params->transport_task = transport_bt_task;
            return true;
        }
        else return false;

        case GAMEPAD_TRANSPORT_WLAN:
        if(transport_wlan_init(params))
        {
            _tp_stop_cb = transport_wlan_stop;
            params->transport_task = transport_wlan_task;
            return true;
        }
        else return false;

        default:
        return false;
    }

    // Fallthrough
    return false;
}

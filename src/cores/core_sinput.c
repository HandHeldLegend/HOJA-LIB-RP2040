#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cores/cores.h"
#include "transport/transport.h"

#include "hoja.h"
#include "board_config.h"

#include "utilities/static_config.h"
#include "utilities/settings.h"

#include "input/mapper.h"
#include "input/dpad.h"
#include "input/imu.h"

#include "devices/battery.h"
#include "devices/fuelgauge.h"
#include "utilities/pcm.h"

#include "sinput_lib.h"
#include "sinput_lib_hid.h"

#if defined(HOJA_USB_VID)
#define SINPUT_VID  HOJA_USB_VID
#else
#define SINPUT_VID 0x2E8A // Raspberry Pi
#endif

#if defined(HOJA_USB_PID)
#define SINPUT_PID  HOJA_USB_PID
#else
#define SINPUT_PID  0x10C6 // Hoja Gamepad
#endif

#if defined(HOJA_DEVICE_NAME)
#define SINPUT_NAME HOJA_DEVICE_NAME
#else
#define SINPUT_NAME "SInput Gamepad"
#endif

// Helper function for checking if an SINPUT button is enabled
bool _si_enabled(uint8_t code)
{
    return (input_static.input_info[code].input_type > 0);
}

void sinput_api_hook_set_rumble(sinput_stereo_rumble_s rumble)
{
    bool left_right = rumble.left.amplitude > rumble.right.amplitude ? false : true; 

    uint8_t amp = 0;
    bool brake = 0;

    if(left_right)
    {
        amp   = rumble.right.amplitude;
        brake = rumble.right.brake;
    }
    else 
    {
        amp   = rumble.left.amplitude;
        brake = rumble.left.brake;
    }

    pcm_erm_set(amp, brake);
}

void sinput_api_hook_set_haptics(sinput_stereo_haptics_s haptics)
{
    haptic_packet_s packet = {0};
    float a1_base = (float) (haptics.left.amplitude_1 > haptics.right.amplitude_1 ? haptics.left.amplitude_1 : haptics.right.amplitude_1);
    float a2_base = (float) (haptics.left.amplitude_2 > haptics.right.amplitude_2 ? haptics.left.amplitude_2 : haptics.right.amplitude_2);

    float amp_1 = a1_base > 0 ? (float) a1_base / (float) UINT16_MAX : 0;
    float amp_2 = a2_base > 0 ? (float) a2_base / (float) UINT16_MAX : 0;

    packet.count = 1;
    packet.pairs[0].hi_amplitude_fixed = pcm_amplitude_to_fixedpoint(amp_1);
    packet.pairs[0].lo_amplitude_fixed = pcm_amplitude_to_fixedpoint(amp_2);
    packet.pairs[0].hi_frequency_increment = pcm_frequency_to_fixedpoint_increment((float) haptics.left.frequency_1);
    packet.pairs[0].lo_frequency_increment = pcm_frequency_to_fixedpoint_increment((float) haptics.left.frequency_2);

    pcm_amfm_push(&packet);
}

void sinput_api_hook_set_player_leds(uint8_t player_number)
{
    tp_evt_s evt = {
        .evt = TP_EVT_PLAYERLED,
        .evt_playernumber = {.player_number=player_number}
    };
    transport_evt_cb(evt);

    tp_evt_s evt2 = {
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {.connection=TP_CONNECTION_CONNECTED}
    };
    transport_evt_cb(evt2);
}

void sinput_api_hook_set_joystick_rgb(uint32_t rgb_value)
{
    // Unused
}

bool sinput_api_hook_get_power(sinput_power_s *status)
{
    // Access all current data
    battery_status_s bstat = {0};
    battery_get_status(&bstat);

    fuelgauge_status_s fstat = {0};
    fuelgauge_get_status(&fstat);

    // Write battery/power info
    if(bstat.connected)
    {
        if(bstat.charging)
        {
            status->connection_status = SINPUT_CONNSTAT_PLUGGED_CHARGING;
        }
        else if (bstat.plugged)
        {
            status->connection_status = SINPUT_CONNSTAT_PLUGGED_CHARGED;
        }
        else 
        {
            status->connection_status = SINPUT_CONNSTAT_UNPLUGGED;
        }
    }
    else 
    {
        status->connection_status = SINPUT_CONNSTAT_PLUGGED_NO_BATTERY;
    }

    if(fstat.connected)
    {
        status->charge_percent = fstat.percent;
    }
    else 
    {
        status->charge_percent = 100; // 100 Percent
    }

    return true;
}

bool sinput_api_hook_get_input(sinput_input_s *out)
{
    mapper_input_s input = mapper_get_input();

    out->buttons.south  = input.presses[SINPUT_CODE_SOUTH];
    out->buttons.east   = input.presses[SINPUT_CODE_EAST];
    out->buttons.west   = input.presses[SINPUT_CODE_WEST];
    out->buttons.north  = input.presses[SINPUT_CODE_NORTH];

    out->buttons.stick_left  = input.presses[SINPUT_CODE_LS];
    out->buttons.stick_right = input.presses[SINPUT_CODE_RS];

    out->buttons.start  = input.presses[SINPUT_CODE_START];
    out->buttons.select = input.presses[SINPUT_CODE_SELECT];
    out->buttons.guide  = input.presses[SINPUT_CODE_GUIDE];
    out->buttons.share  = input.presses[SINPUT_CODE_SHARE];

    bool dpad[4] = {input.presses[SINPUT_CODE_DOWN], input.presses[SINPUT_CODE_RIGHT],
                        input.presses[SINPUT_CODE_LEFT], input.presses[SINPUT_CODE_UP]};
    dpad_translate_input(dpad);

    out->buttons.down  = dpad[0];
    out->buttons.right = dpad[1];
    out->buttons.left  = dpad[2];
    out->buttons.up    = dpad[3];

    out->buttons.l_bumper  = input.presses[SINPUT_CODE_LB];
    out->buttons.r_bumper  = input.presses[SINPUT_CODE_RB];
    out->buttons.l_trigger = input.presses[SINPUT_CODE_LT];
    out->buttons.r_trigger = input.presses[SINPUT_CODE_RT];

    out->buttons.l_grip_1 = input.presses[SINPUT_CODE_LP_1];
    out->buttons.r_grip_1 = input.presses[SINPUT_CODE_RP_1];
    out->buttons.l_grip_2 = input.presses[SINPUT_CODE_LP_2];
    out->buttons.r_grip_2 = input.presses[SINPUT_CODE_RP_2];

    out->buttons.power = input.presses[SINPUT_CODE_MISC_3];

    out->triggers.left  = input.inputs[SINPUT_CODE_LT_ANALOG];
    out->triggers.right = input.inputs[SINPUT_CODE_RT_ANALOG];

    out->joysticks.left.x  = mapper_joystick_concat(0,input.inputs[SINPUT_CODE_LX_LEFT], input.inputs[SINPUT_CODE_LX_RIGHT]); 
    out->joysticks.left.y  = mapper_joystick_concat(0,input.inputs[SINPUT_CODE_LY_UP]  , input.inputs[SINPUT_CODE_LY_DOWN] ); 
    out->joysticks.right.x = mapper_joystick_concat(0,input.inputs[SINPUT_CODE_RX_LEFT], input.inputs[SINPUT_CODE_RX_RIGHT]); 
    out->joysticks.right.y = mapper_joystick_concat(0,input.inputs[SINPUT_CODE_RY_UP]  , input.inputs[SINPUT_CODE_RY_DOWN] ); 
    
    return true;
}

bool sinput_api_hook_get_motion(sinput_motion_s *out)
{
    static imu_data_s imu = {0};
    imu_access_safe(&imu);

    out->accel.x = imu.ax; 
    out->accel.y = imu.ay;
    out->accel.z = imu.az;

    out->gyro.x  = imu.gx;
    out->gyro.y  = imu.gy;
    out->gyro.z  = imu.gz; 

    out->timestamp_us = imu.timestamp;

    return true;
}

bool _core_sinput_get_generated_report(core_report_s *out)
{
    out->reportformat=CORE_REPORTFORMAT_SINPUT;
    out->size=64; // 64 bytes including our report ID

    return sinput_api_generate_inputreport(out->data);
}

static core_hid_device_t _sinput_hid_device = {
    .config_descriptor = NULL,
    .config_descriptor_len = 0,
    .hid_report_descriptor = NULL,
    .hid_report_descriptor_len = 0,
    .device_descriptor = NULL,
    .name = SINPUT_NAME,
    .pid = SINPUT_PID,
    .vid = SINPUT_VID,
};

/*------------------------------------------------*/

// Public Functions
bool core_sinput_init(core_params_s *params)
{
    params->transport_dev_mac[5] += 2;
    
    switch(params->transport_type)
    {
        case GAMEPAD_TRANSPORT_USB:
        params->core_pollrate_us = 1000;
        break;

        case GAMEPAD_TRANSPORT_BLUETOOTH:
        params->core_pollrate_us = 8000;
        break;

        case GAMEPAD_TRANSPORT_WLAN:
        params->core_pollrate_us = 2000;
        break;

        // Unsupported transport methods
        default:
        return false;
    }

    params->hid_device = &_sinput_hid_device;

    params->sys_gyro_task = imu_forced_task_standard;

    params->core_report_format    = CORE_REPORTFORMAT_SINPUT;
    params->core_report_generator = _core_sinput_get_generated_report;
    params->core_report_tunnel    = sinput_api_output_tunnel;

    uint8_t *hid_descriptor;
    uint16_t hid_descriptor_len;
    uint8_t *config_descriptor;
    uint16_t config_descriptor_len;

    sinput_hid_get_descriptor_params(
        &_sinput_hid_device.hid_report_descriptor, &_sinput_hid_device.hid_report_descriptor_len,
        &_sinput_hid_device.config_descriptor, &_sinput_hid_device.config_descriptor_len, 
        NULL, NULL
    );

    _sinput_hid_device.device_descriptor = (hoja_usb_device_descriptor_t *)sinput_hid_get_device_descriptor();

    sinput_device_cfg_s cfg = 
    {
        .buttons = 
        {
            .sewn = (_si_enabled(INPUT_CODE_SOUTH) || _si_enabled(INPUT_CODE_EAST) || _si_enabled(INPUT_CODE_WEST) || _si_enabled(INPUT_CODE_NORTH)),
            .dpad = (_si_enabled(INPUT_CODE_UP) || _si_enabled(INPUT_CODE_DOWN) || _si_enabled(INPUT_CODE_LEFT) || _si_enabled(INPUT_CODE_RIGHT)),
            .bumpers = (_si_enabled(INPUT_CODE_LB) || _si_enabled(INPUT_CODE_RB)),
            .triggers = (_si_enabled(INPUT_CODE_LT) || _si_enabled(INPUT_CODE_RT)),
            .grips_upper = (_si_enabled(INPUT_CODE_LP1) || _si_enabled(INPUT_CODE_RP1)),
            .grips_lower = (_si_enabled(INPUT_CODE_LP2) || _si_enabled(INPUT_CODE_RP2)),
            .start_select = (_si_enabled(INPUT_CODE_START) || _si_enabled(INPUT_CODE_SELECT)),
            .home = (_si_enabled(INPUT_CODE_HOME)),
            .share = (_si_enabled(INPUT_CODE_SHARE)),
            .power = (_si_enabled(INPUT_CODE_MISC3)),
        },
        .joysticks = 
        {
            .left = analog_static.axis_lx,
            .right = analog_static.axis_rx,
        },
        .triggers = 
        {
            .left = analog_static.axis_lt,
            .right = analog_static.axis_rt,
        },
        .leds =
        {
            .player = true,
            .joystick = false,
        },
        .motion = 
        {
            .accelerometer = imu_static.axis_accel_a,
            .accelerometer_g_range = 8,
            .gyroscope = imu_static.axis_gyro_a,
            .gyroscope_dps_range = 2000,
        },
        .polling_rate_us = params->core_pollrate_us,
        
        .rumble = (haptic_static.haptic_hd | haptic_static.haptic_sd),
        .gamepad_type = HOJA_SINPUT_GAMEPAD_TYPE,
        .face_buttons_style = HOJA_SINPUT_GAMEPAD_FACESTYLE,
        .gamepad_format = SINPUT_GAMEPAD_FORMAT_JOYPAD,
    };

    memcpy(cfg.mac_address, params->transport_dev_mac, 6);

    sinput_api_init(&cfg);

    return transport_init(params);
}

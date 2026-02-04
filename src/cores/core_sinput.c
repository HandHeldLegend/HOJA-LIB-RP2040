#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "cores/cores.h"
#include "transport/transport.h"

#include "hoja.h"
#include "board_config.h"

#include "utilities/static_config.h"
#include "utilities/settings.h"

#include "input/mapper.h"
#include "input/dpad.h"

#include "devices/battery.h"
#include "devices/fuelgauge.h"

#define SINPUT_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

#define REPORT_ID_SINPUT_INPUT  0x01 // Input Report ID, used for SINPUT input data
#define REPORT_ID_SINPUT_INPUT_CMDDAT  0x02 // Input report ID for command replies
#define REPORT_ID_SINPUT_OUTPUT_CMDDAT 0x03 // Output Haptic Report ID, used for haptics and commands

#define SINPUT_COMMAND_HAPTIC       0x01
#define SINPUT_COMMAND_FEATURES     0x02
#define SINPUT_COMMAND_PLAYERLED    0x03

#define SINPUT_REPORT_LEN_COMMAND 48 
#define SINPUT_REPORT_LEN_INPUT   64

#pragma pack(push, 1) // Ensure byte alignment
// Input report (Report ID: 1)
typedef struct
{
    uint8_t plug_status;    // Plug Status Format
    uint8_t charge_percent; // 0-100

    union {
        struct {
            uint8_t button_south : 1;
            uint8_t button_east  : 1;
            uint8_t button_west  : 1;
            uint8_t button_north : 1;
            uint8_t dpad_up    : 1;
            uint8_t dpad_down  : 1;
            uint8_t dpad_left  : 1;
            uint8_t dpad_right : 1;
        };
        uint8_t buttons_1;
    };

    union
    {
        struct
        {
            uint8_t button_stick_left : 1;
            uint8_t button_stick_right : 1;
            uint8_t button_l_shoulder : 1;
            uint8_t button_r_shoulder : 1;
            uint8_t button_l_trigger : 1;
            uint8_t button_r_trigger : 1;
            uint8_t button_l_paddle_1 : 1;
            uint8_t button_r_paddle_1 : 1;
        };
        uint8_t buttons_2;
    };

    union
    {
        struct
        {
            uint8_t button_start  : 1;
            uint8_t button_select : 1;
            uint8_t button_guide  : 1;
            uint8_t button_share  : 1;
            uint8_t button_l_paddle_2 : 1;
            uint8_t button_r_paddle_2 : 1;
            uint8_t button_l_touchpad : 1;
            uint8_t button_r_touchpad : 1;
        };
        uint8_t buttons_3;
    };

    union
    {
        struct
        {
            uint8_t button_power   : 1;
            uint8_t button_misc_4  : 1;
            uint8_t button_misc_5  : 1;
            uint8_t button_misc_6  : 1;
            
            // Misc 7 through 10 is unused by
            // SDL currently!
            uint8_t button_misc_7  : 1; 
            uint8_t button_misc_8  : 1;
            uint8_t button_misc_9  : 1;
            uint8_t button_misc_10 : 1;
        };
        uint8_t buttons_4;
    };

    int16_t left_x;             // Left stick X
    int16_t left_y;             // Left stick Y
    int16_t right_x;            // Right stick X
    int16_t right_y;            // Right stick Y
    int16_t trigger_l;          // Left trigger
    int16_t trigger_r;          // Right trigger

    uint32_t imu_timestamp_us;  // IMU Timestamp
    int16_t accel_x;            // Accelerometer X
    int16_t accel_y;            // Accelerometer Y
    int16_t accel_z;            // Accelerometer Z
    int16_t gyro_x;             // Gyroscope X
    int16_t gyro_y;             // Gyroscope Y
    int16_t gyro_z;             // Gyroscope Z

    int16_t touchpad_1_x;       // Touchpad/trackpad
    int16_t touchpad_1_y;
    int16_t touchpad_1_pressure;

    int16_t touchpad_2_x;
    int16_t touchpad_2_y;
    int16_t touchpad_2_pressure;

    uint8_t reserved_bulk[17];  // Reserved for command data
} sinput_input_s;
#pragma pack(pop)

#define SINPUT_INPUT_SIZE sizeof(sinput_input_s)

#pragma pack(push, 1) // Ensure byte alignment
typedef struct 
{
    uint8_t type;

    union 
    {
        // Frequency Amplitude pairs
        struct 
        {
            struct
            {
                uint16_t frequency_1;
                uint16_t amplitude_1;
                uint16_t frequency_2;
                uint16_t amplitude_2;
            } left;

            struct
            {
                uint16_t frequency_1;
                uint16_t amplitude_1;
                uint16_t frequency_2;
                uint16_t amplitude_2;
            } right;
            
        } type_1;

        // Basic ERM simulation model
        struct 
        {
            struct 
            {
                uint8_t amplitude;
                bool    brake;
            } left;

            struct 
            {
                uint8_t amplitude;
                bool    brake;
            } right;
            
        } type_2; 
    };
} sinput_haptic_s;
#pragma pack(pop)

#define SINPUT_HAPTIC_SIZE sizeof(sinput_haptic_s)

#pragma pack(push, 1) // Ensure byte alignment
typedef union
{
    struct
    {
        uint8_t rumble_supported : 1;
        uint8_t player_leds_supported : 1;
        uint8_t accelerometer_supported : 1;
        uint8_t gyroscope_supported : 1;
        uint8_t left_analog_stick_supported : 1;
        uint8_t right_analog_stick_supported : 1;
        uint8_t left_analog_trigger_supported : 1;
        uint8_t right_analog_trigger_supported : 1;
    };
    uint8_t value;
} sinput_featureflags_1_u;
#pragma pack(pop)

#pragma pack(push, 1) // Ensure byte alignment
typedef union
{
    struct
    {
        uint8_t left_touchpad_supported  : 1;
        uint8_t right_touchpad_supported : 1;
        uint8_t reserved : 6;
    };
    uint8_t value;
} sinput_featureflags_2_u;
#pragma pack(pop)

void _sinput_generate_features(uint8_t *buffer)
{
    sinput_featureflags_1_u feature_flags = {0};

    uint16_t accel_g_range      = 8; // 8G 
    uint16_t gyro_dps_range     = 2000; // 2000 degrees per second

    const uint16_t sinput_protocol_version = 0x0001;
    memcpy(&buffer[0], &sinput_protocol_version, sizeof(sinput_protocol_version)); 

    feature_flags.value = 0x00; // Set default feature flags

    feature_flags.accelerometer_supported   = (imu_static.axis_accel_a) ? 1 : 0;
    feature_flags.gyroscope_supported       = (imu_static.axis_gyro_a) ? 1 : 0;

    feature_flags.left_analog_stick_supported       = (analog_static.axis_lx) ? 1 : 0;
    feature_flags.right_analog_stick_supported      = (analog_static.axis_rx) ? 1 : 0;
    feature_flags.left_analog_trigger_supported     = (analog_static.axis_lt) ? 1 : 0;
    feature_flags.right_analog_trigger_supported    = (analog_static.axis_rt) ? 1 : 0;

    feature_flags.rumble_supported      = (haptic_static.haptic_hd | haptic_static.haptic_sd) ? 1 : 0;
    feature_flags.player_leds_supported = 1;

    buffer[2] = feature_flags.value; // Feature flags value      
    buffer[3] = 0x00; // Reserved byte

    // Gamepad Type (Derived from SDL)
    /* 
    typedef enum SDL_GamepadType
    {
        SDL_GAMEPAD_TYPE_UNKNOWN = 0,
        SDL_GAMEPAD_TYPE_STANDARD,
        SDL_GAMEPAD_TYPE_XBOX360,
        SDL_GAMEPAD_TYPE_XBOXONE,
        SDL_GAMEPAD_TYPE_PS3,
        SDL_GAMEPAD_TYPE_PS4,
        SDL_GAMEPAD_TYPE_PS5,
        SDL_GAMEPAD_TYPE_NINTENDO_SINPUT_PRO,
        SDL_GAMEPAD_TYPE_NINTENDO_SINPUT_JOYCON_LEFT,
        SDL_GAMEPAD_TYPE_NINTENDO_SINPUT_JOYCON_RIGHT,
        SDL_GAMEPAD_TYPE_NINTENDO_SINPUT_JOYCON_PAIR,
        SDL_GAMEPAD_TYPE_GAMECUBE,
        SDL_GAMEPAD_TYPE_COUNT
    } SDL_GamepadType;
     */
    #if defined(HOJA_SINPUT_GAMEPAD_TYPE)
    buffer[4] = HOJA_SINPUT_GAMEPAD_TYPE;
    #else 
    buffer[4] = 0;
    #endif

    uint8_t sub_type = 0;
    uint8_t face_style = 0;

    #if defined(HOJA_SINPUT_GAMEPAD_SUBTYPE)
    sub_type = HOJA_SINPUT_GAMEPAD_SUBTYPE & 0x1F; // Gamepad Sub-type (leave as zero in most cases)
    #endif

    #if defined(HOJA_SINPUT_GAMEPAD_FACESTYLE)
    face_style = HOJA_SINPUT_GAMEPAD_FACESTYLE & 0x7;
    #endif 

    buffer[5] = (face_style << 5) | sub_type;

    uint16_t polling_rate_us = 1000;

    //switch(hoja_get_status().gamepad_method)
    //{
    //    case GAMEPAD_METHOD_BLUETOOTH:
    //    polling_rate_us = 8000;
    //    break;
    //    case GAMEPAD_METHOD_WLAN:
    //    polling_rate_us = 2000;
    //    break;
    //}
        
    memcpy(&buffer[6], &polling_rate_us, sizeof(polling_rate_us));

    memcpy(&buffer[8], &accel_g_range, sizeof(accel_g_range)); // Accelerometer G range
    memcpy(&buffer[10], &gyro_dps_range, sizeof(gyro_dps_range)); // Gyroscope DPS range

    _sinput_generate_input_masks(&buffer[12]);

    buffer[16] = 0; // Touchpad count
    buffer[17] = 0; // Touchpad finger count

    buffer[18] = gamepad_config->switch_mac_address[0];
    buffer[19] = gamepad_config->switch_mac_address[1];
    buffer[20] = gamepad_config->switch_mac_address[2];
    buffer[21] = gamepad_config->switch_mac_address[3];
    buffer[22] = gamepad_config->switch_mac_address[4];
    buffer[23] = gamepad_config->switch_mac_address[5] + (uint8_t) hoja_get_status().gamepad_mode;
}


int16_t _sinput_scale_trigger(uint16_t val)
{
    if (val > 4095) val = 4095; // Clamp just in case

    // Scale: map [0, 4095] → [INT16_MIN, INT16_MAX]
    // The range of INT16 is 65535, so multiply first to preserve precision
    return (int16_t)(((int32_t)val * 65535) / 4095 + INT16_MIN);
}


int16_t _sinput_scale_axis(int16_t input_axis)
{   
    return SINPUT_CLAMP(input_axis * 16, INT16_MIN, INT16_MAX);
}

volatile uint8_t _sinput_current_command = 0;
uint8_t _sinput_response_report[64] = {0};

void _core_sinput_report_tunnel_cb(uint8_t *data, uint16_t len)
{
    if(len<2) return;

    switch(data[0])
    {
        default:
        return;

        case SINPUT_COMMAND_HAPTIC:
        break;

        case SINPUT_COMMAND_FEATURES:
        _sinput_current_command = SINPUT_COMMAND_FEATURES;
        break;

        case SINPUT_COMMAND_PLAYERLED:
        // Do transport set function here
        break;
    }
}

void _core_sinput_get_generated_report(core_report_s *out)
{
    out->format=CORE_FORMAT_SINPUT;
    out->size=64; // 64 bytes including our report ID

    // Handle command reply
    if(_sinput_current_command)
    {
        switch(_sinput_current_command)
        {
            case SINPUT_COMMAND_FEATURES:
            out->data[0] = REPORT_ID_SINPUT_INPUT_CMDDAT;
            out->data[1] = SINPUT_COMMAND_FEATURES;
            _sinput_generate_features(&out->data[2]);
        }
    }
    // Standard input
    else 
    {
        out->data[0] = REPORT_ID_SINPUT_INPUT;
        sinput_input_s *data = &(out->data[1]);

        // Access all current data
        battery_status_s bstat = {0};
        battery_get_status(&bstat);

        fuelgauge_status_s fstat = {0};
        fuelgauge_get_status(&fstat);

        mapper_input_s input = mapper_get_input();

        static imu_data_s imu = {0};
        imu_access_safe(&imu);

        // Write battery/power info
        if(bstat.connected)
        {
            if(bstat.charging)
            {
                data->plug_status = 2; // Charging
            }
            else if (bstat.plugged)
            {
                data->plug_status = 3; // Not Charging/Charge Complete
            }
            else 
            {
                data->plug_status = 4; // On battery power
            }
        }
        else 
        {
            data->plug_status = 0; // Plugged
        }

        if(fstat.connected)
        {
            data->charge_percent = fstat.percent;
        }
        else 
        {
            data->charge_percent = 100; // 100 Percent
        }

        data->accel_x = imu.ax; 
        data->accel_y = imu.ay;
        data->accel_z = imu.az;

        data->gyro_x = imu.gx;
        data->gyro_y = imu.gy;
        data->gyro_z = imu.gz; 

        data->imu_timestamp_us = (uint32_t) (imu.timestamp & UINT32_MAX);

        // Buttons
        data->button_east   = input.presses[SINPUT_CODE_EAST];
        data->button_south  = input.presses[SINPUT_CODE_SOUTH];
        data->button_north  = input.presses[SINPUT_CODE_NORTH];
        data->button_west   = input.presses[SINPUT_CODE_WEST];

        data->button_stick_left  = input.presses[SINPUT_CODE_LS];
        data->button_stick_right = input.presses[SINPUT_CODE_RS];

        data->button_start  = input.presses[SINPUT_CODE_START];
        data->button_select = input.presses[SINPUT_CODE_SELECT];
        data->button_guide  = input.presses[SINPUT_CODE_GUIDE];
        data->button_share  = input.presses[SINPUT_CODE_SHARE];

        bool dpad[4] = {input.presses[SINPUT_CODE_DOWN], input.presses[SINPUT_CODE_RIGHT],
                        input.presses[SINPUT_CODE_LEFT], input.presses[SINPUT_CODE_UP]};

        dpad_translate_input(dpad);

        data->dpad_down  = dpad[0];
        data->dpad_right = dpad[1];
        data->dpad_left  = dpad[2];
        data->dpad_up    = dpad[3];

        data->button_l_shoulder = input.presses[SINPUT_CODE_LB];
        data->button_r_shoulder = input.presses[SINPUT_CODE_RB];

        data->button_l_trigger = input.presses[SINPUT_CODE_LT];
        data->button_r_trigger = input.presses[SINPUT_CODE_RT];

        data->button_l_paddle_1 = input.presses[SINPUT_CODE_LP_1];
        data->button_r_paddle_1 = input.presses[SINPUT_CODE_RP_1];

        data->button_l_paddle_2 = input.presses[SINPUT_CODE_LP_2];
        data->button_r_paddle_2 = input.presses[SINPUT_CODE_RP_2];

        data->button_power = input.presses[SINPUT_CODE_MISC_3];

        data->trigger_l = sinput_scale_trigger(input.inputs[SINPUT_CODE_LT_ANALOG]);
        data->trigger_r = sinput_scale_trigger(input.inputs[SINPUT_CODE_RT_ANALOG]);
    }
}

/*------------------------------------------------*/

// Public Functions
bool core_sinput_init(core_params_s *params)
{
    switch(params->gamepad_transport)
    {
        case GAMEPAD_TRANSPORT_USB:
        break;

        case GAMEPAD_TRANSPORT_BLUETOOTH:
        break;

        case GAMEPAD_TRANSPORT_WLAN:
        break;

        // Unsupported transport methods
        default:
        return false;
    }

    params->report_generator = _core_sinput_get_generated_report;
    params->report_tunnel = _core_sinput_report_tunnel_cb;

    return transport_init(params->gamepad_transport);
}

void core_sinput_task(uint64_t timestamp)
{

}
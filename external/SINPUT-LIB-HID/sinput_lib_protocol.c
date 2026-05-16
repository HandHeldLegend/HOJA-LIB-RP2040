#include "sinput_lib.h"
#include "sinput_lib_protocol.h"

#define SINPUT_COMMAND_HAPTIC       0x01
#define SINPUT_COMMAND_FEATURES     0x02
#define SINPUT_COMMAND_PLAYERLED    0x03
#define SINPUT_COMMAND_JOYSTICKRGB  0x04

typedef enum
{
    SINPUT_FEATURE_STATE_IDLE,
    SINPUT_FEATURE_STATE_REQUESTED,
    SINPUT_FEATURE_STATE_GENERATING,
} sinput_feature_state_t;

static volatile sinput_feature_state_t _feature_state = SINPUT_FEATURE_STATE_IDLE;

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

void _sinput_protocol_generate_features(uint8_t out[64])
{

}

bool sinput_protocol_generate_inputreport(uint8_t out[64])
{
    if(_feature_state == SINPUT_FEATURE_STATE_REQUESTED)
    {
        // Generate features response
        _feature_state = SINPUT_FEATURE_STATE_GENERATING;
        _sinput_protocol_generate_features(out);
        _feature_state = SINPUT_FEATURE_STATE_IDLE;
        return true;
    }

    static sinput_input_s input = 
    {
        .charge_percent = 100,
        .trigger_l = (-0x7fff - 1),
        .trigger_r = (-0x7fff - 1)
    };

    static sinput_power_s status;
    static sinput_buttons_s buttons;
    static sinput_joysticks_s joysticks;
    static sinput_triggers_s triggers;
    static sinput_motion_s motion;
    static sinput_touchpads_s touchpads;
    
    if(sinput_api_hook_get_power(&status))
    {

    }

    if(sinput_api_hook_get_buttons(&buttons))
    {

    }

    if(sinput_api_hook_get_joysticks(&joysticks))
    {

    }

    if(sinput_api_hook_get_triggers(&triggers))
    {

    }

    if(sinput_api_hook_get_motion(&motion))
    {

    }

    if(sinput_api_hook_get_touchpads(&touchpads))
    {

    }
}

static inline void _sinput_protocol_haptic_process(uint8_t *data)
{

}

static inline void _sinput_protocol_joystick_rgb_process(uint8_t *data)
{
    uint32_t rgb;
    memcpy(&rgb, data, 4);
    sinput_api_hook_set_joystick_rgb(rgb);
}

void sinput_protocol_output_tunnel(const uint8_t *data, uint16_t len)
{
    if(len<2 || data[0] != 0x03) return;

    // Command ID
    switch(data[1])
    {
        case SINPUT_COMMAND_HAPTIC:
        _sinput_protocol_haptic_process(&data[2]);
        break;

        case SINPUT_COMMAND_FEATURES:
        if(_feature_state == SINPUT_FEATURE_STATE_IDLE)
        {
            _feature_state = SINPUT_FEATURE_STATE_REQUESTED;
        }
        break;

        case SINPUT_COMMAND_PLAYERLED:
        sinput_api_hook_set_player_leds(data[2]);
        break;

        case SINPUT_COMMAND_JOYSTICKRGB:
        _sinput_protocol_joystick_rgb_process(&data[2]);
        break;

        default:
        return;
    }
}
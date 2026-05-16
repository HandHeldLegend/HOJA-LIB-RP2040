#include "sinput_lib.h"
#include "sinput_lib_protocol.h"
#include "sinput_lib_config.h"

#define REPORT_ID_SINPUT_INPUT  0x01 // Input Report ID, used for SINPUT input data
#define REPORT_ID_SINPUT_INPUT_CMDDAT  0x02 // Input report ID for command replies
#define REPORT_ID_SINPUT_OUTPUT_CMDDAT 0x03 // Output Haptic Report ID, used for haptics and commands

#define SINPUT_COMMAND_HAPTIC       0x01
#define SINPUT_COMMAND_FEATURES     0x02
#define SINPUT_COMMAND_PLAYERLED    0x03
#define SINPUT_COMMAND_JOYSTICKRGB  0x04

#define SINPUT_REPORT_LEN_COMMAND 48 
#define SINPUT_REPORT_LEN_INPUT   64

#define SINPUT_MASK_SEWN        0x0F
#define SINPUT_MASK_DPAD        0xF0
#define SINPUT_MASK_BUMPERS     0x0C // Bumpers (L1, R1)
#define SINPUT_MASK_TRIGGERS    0x30 // Digital Triggers (L2, R2, ZL, ZR)
#define SINPUT_MASK_STARTSELECT 0x03 // Start + Select
#define SINPUT_MASK_HOME        0x04
#define SINPUT_MASK_CAPTURE     0x08 // Capture
#define SINPUT_MASK_LSTICK      0x01 // Stick Click: L3
#define SINPUT_MASK_RSTICK      0x02 // Stick Click: R3
#define SINPUT_MASK_UPPERGRIPS  0xC0 // Upper Grips (L4, R4)
#define SINPUT_MASK_LOWERGRIPS  0x30 // Lower Grips (L5, R5)
#define SINPUT_MASK_TOUCHPAD_1  0x40
#define SINPUT_MASK_TOUCHPAD_2  0x80
#define SINPUT_MASK_POWER       0x01
#define SINPUT_MASK_MISC1       0x02
#define SINPUT_MASK_MISC2       0x04
#define SINPUT_MASK_MISC3       0x08

#define SINPUT_MASK_0(sewn, dpad) ((sewn ? SINPUT_MASK_SEWN : 0) | (dpad ? SINPUT_MASK_DPAD : 0))
#define SINPUT_MASK_1(left_stick_btn, right_stick_btn,  bumpers, triggers_digital, upper_grips) \
    (\
        (left_stick_btn ? SINPUT_MASK_LSTICK : 0) | (right_stick_btn ? SINPUT_MASK_RSTICK : 0) | \
        (bumpers ? SINPUT_MASK_BUMPERS : 0) | (triggers_digital ? SINPUT_MASK_TRIGGERS : 0) | \
        (upper_grips ? SINPUT_MASK_UPPERGRIPS : 0) \
    )
#define SINPUT_MASK_2(start_select, home, capture, lower_grips, touchpad_1, touchpad_2) \
    (\
        (start_select ? SINPUT_MASK_STARTSELECT : 0) | (home ? SINPUT_MASK_HOME : 0) | (capture ? SINPUT_MASK_CAPTURE : 0) | \
        (lower_grips ? SINPUT_MASK_LOWERGRIPS : 0) | \
        (touchpad_1 ? SINPUT_MASK_TOUCHPAD_1 : 0) | (touchpad_2 ? SINPUT_MASK_TOUCHPAD_2 : 0) \
    )
#define SINPUT_MASK_3(power, misc1, misc2, misc3) \
    (\
        (power ? SINPUT_MASK_POWER : 0) | \
        (misc1 ? SINPUT_MASK_MISC1 : 0) | (misc2 ? SINPUT_MASK_MISC2 : 0) | (misc3 ? SINPUT_MASK_MISC3 : 0) \
    )

#define SINPUT_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

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
        uint8_t touchpad_supported  : 1;
        uint8_t joystick_rgb_supported : 1;
        uint8_t handheld_joypad_flag : 1;
        uint8_t reserved : 5;
    };
    uint8_t value;
} sinput_featureflags_2_u;
#pragma pack(pop)

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

void _sinput_protocol_generate_features(uint8_t out[64])
{
    // Clear report
    memset(out, 0, 64);
    // Set Report ID
    out[0] = REPORT_ID_SINPUT_INPUT_CMDDAT;
    // Set output command 
    out[1] = SINPUT_COMMAND_FEATURES;

    // Copy config data
    sinput_device_cfg_s cfg;
    sinput_config_get(&cfg);

    // Setup features report data
    const uint16_t protocol_version = 0x0001;
    memcpy(&out[2], &protocol_version, 2);

    out[6] = (uint8_t) cfg.gamepad_type;

    sinput_featureflags_1_u ff1 = {0};
    sinput_featureflags_2_u ff2 = {0};

    uint8_t sub_type = 0;
    uint8_t face_style = 0;
    face_style = (uint8_t) cfg.face_buttons_style & 0x7;
    out[7] = (face_style << 5) | sub_type;

    // Polling rate microseconds
    memcpy(&out[8], cfg.polling_rate_us, 2);

    // Accelerometer
    if(cfg.motion.accelerometer)
    {
        ff1.accelerometer_supported = true;
        // Set accelerometer G range
        memcpy(&out[10], &cfg.motion.accelerometer_g_range, 2);
    }

    // Gyroscope
    if(cfg.motion.gyroscope)
    {
        ff1.gyroscope_supported = true;
        // Set gyroscope dps range
        memcpy(&out[12], &cfg.motion.gyroscope_dps_range, 2);
    }

    // Player LEDs
    if(cfg.leds.player)
    {
        ff1.player_leds_supported = true;
    }

    // Joystick RGB
    if(cfg.leds.joystick)
    {
        ff2.joystick_rgb_supported = true;
    }

    // Left Joystick
    if(cfg.joysticks.left)
    {
        ff1.left_analog_stick_supported = true;
    }

    // Right Joystick
    if(cfg.joysticks.right)
    {
        ff1.right_analog_stick_supported = true;
    }

    // Left Analog Trigger
    if(cfg.triggers.left)
    {
        ff1.left_analog_trigger_supported = true;
    }

    // Right Analog Trigger
    if(cfg.triggers.right)
    {
        ff1.right_analog_trigger_supported = true;
    }

    // Touchpads
    if(cfg.touchpads.touchpad_left)
    {
        uint8_t touchpad_count = 1;
        uint8_t touchpad_fingers = 2;
        if(cfg.touchpads.touchpad_right)
        {
            touchpad_count = 2;
        }

        ff2.touchpad_supported = true;
        out[18] = touchpad_count;
        out[19] = touchpad_fingers;
    }

    // Gamepad format
    ff2.handheld_joypad_flag = cfg.gamepad_format;
    
    out[14] = SINPUT_MASK_0(cfg.buttons.sewn, cfg.buttons.dpad);
    out[15] = SINPUT_MASK_1(cfg.joysticks.left, cfg.joysticks.right, cfg.buttons.bumpers, cfg.buttons.triggers, cfg.buttons.grips_upper);
    out[16] = SINPUT_MASK_2(cfg.buttons.start_select, cfg.buttons.home, cfg.buttons.share, cfg.buttons.grips_lower, cfg.touchpads.touchpad_left, cfg.touchpads.touchpad_right);
    out[17] = SINPUT_MASK_3(cfg.buttons.power, cfg.buttons.misc1, cfg.buttons.misc2, cfg.buttons.misc3);

    // MAC Address OR Serial Number
    memcpy(&out[20], cfg.mac_address, 6);
    
}

int16_t _sinput_scale_trigger(uint16_t val)
{
    if (val > 4095) val = 4095; // Clamp just in case

    // Scale: map [0, 4095] → [INT16_MIN, INT16_MAX]
    // The range of INT16 is 65535, so multiply first to preserve precision
    return (int16_t)(((int32_t)val * 65535) / 4095 + INT16_MIN);
}

bool sinput_protocol_generate_inputreport(uint8_t out[64])
{
    if(!out) return false;

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
        input.charge_percent = SINPUT_CLAMP(status.charge_percent, 0, 100);
        input.plug_status = (uint8_t) status.connection_status;
    }

    if(sinput_api_hook_get_buttons(&buttons))
    {
        input.buttons_1 = buttons.buttons_1;
        input.buttons_2 = buttons.buttons_2;
        input.buttons_3 = buttons.buttons_3;
        input.buttons_4 = buttons.buttons_4;
    }

    if(sinput_api_hook_get_joysticks(&joysticks))
    {
        input.left_x = joysticks.left.x;
        input.left_y = joysticks.left.y;
        input.right_x = joysticks.right.x;
        input.right_y = joysticks.right.y;
    }

    if(sinput_api_hook_get_triggers(&triggers))
    {
        input.trigger_l = _sinput_scale_trigger(triggers.left);
        input.trigger_r = _sinput_scale_trigger(triggers.right);
    }

    if(sinput_api_hook_get_motion(&motion))
    {
        input.accel_x = motion.accel.x;
        input.accel_y = motion.accel.y;
        input.accel_z = motion.accel.z;

        input.gyro_x = motion.gyro.x;
        input.gyro_y = motion.gyro.y;
        input.gyro_z = motion.gyro.z;

        input.imu_timestamp_us = motion.timestamp_us;
    }

    if(sinput_api_hook_get_touchpads(&touchpads))
    {
        input.touchpad_1_x = touchpads.left.x;
        input.touchpad_1_y = touchpads.left.y;
        input.touchpad_1_pressure = touchpads.left.pressure;

        input.touchpad_2_x = touchpads.right.x;
        input.touchpad_2_y = touchpads.right.y;
        input.touchpad_2_pressure = touchpads.right.pressure;
    }

    // Clear report
    memset(out, 0, 64);

    // Set report ID
    out[0] = REPORT_ID_SINPUT_INPUT;

    // Copy input data
    memcpy(&out[1], &input, SINPUT_INPUT_SIZE);

    return true;
}

static inline void _sinput_protocol_haptic_process(uint8_t *data)
{
    sinput_haptic_s tmp;
    memcpy(&tmp, data, sizeof(tmp));

    sinput_stereo_haptics_s haptic;
    sinput_stereo_rumble_s rumble;

    // Haptics types
    switch(tmp.type)
    {
        // Invalid
        default:
        break;

        case 1:
        haptic.left.amplitude_1 = tmp.type_1.left.amplitude_1;
        haptic.left.frequency_1 = tmp.type_1.left.frequency_1;
        haptic.left.amplitude_2 = tmp.type_1.left.amplitude_2;
        haptic.left.frequency_2 = tmp.type_1.left.frequency_2;
        
        haptic.right.amplitude_1 = tmp.type_1.right.amplitude_1;
        haptic.right.frequency_1 = tmp.type_1.right.frequency_1;
        haptic.right.amplitude_2 = tmp.type_1.right.amplitude_2;
        haptic.right.frequency_2 = tmp.type_1.right.frequency_2;

        sinput_api_hook_set_haptics(haptic);
        break;

        case 2:
        rumble.left.amplitude = tmp.type_2.left.amplitude;
        rumble.left.brake = tmp.type_2.left.brake;

        rumble.right.amplitude = tmp.type_2.right.amplitude;
        rumble.right.brake = tmp.type_2.right.brake;

        sinput_api_hook_set_rumble(rumble);
        break;
    }
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

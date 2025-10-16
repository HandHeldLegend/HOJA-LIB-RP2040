#include "usb/sinput.h"

#include <stdbool.h>
#include <string.h>
#include "tusb.h"
#include "hoja.h"
#include "input/button.h"
#include "input/analog.h"
#include "input/imu.h"
#include "input/trigger.h"
#include "input/mapper.h"

#include "utilities/pcm.h"
#include "utilities/static_config.h"
#include "utilities/settings.h"

#include "devices/battery.h"
#include "devices/fuelgauge.h"

#include "board_config.h"

// SEWN (South, East, West, North)
#if (defined(HOJA_INPUT_ENABLE_SEWN) && HOJA_INPUT_ENABLE_SEWN == 1)
    #define SINPUT_MASK_SEWN 0x0F
#else
    #define SINPUT_MASK_SEWN 0
#endif

// DPAD (Up, Down, Left, Right)
#if (defined(HOJA_INPUT_ENABLE_DPAD) && HOJA_INPUT_ENABLE_DPAD == 1)
    #define SINPUT_MASK_DPAD 0xF0
#else
    #define SINPUT_MASK_DPAD 0
#endif

// Bumpers (L1, R1)
#if (defined(HOJA_INPUT_ENABLE_BUMPERS) && HOJA_INPUT_ENABLE_BUMPERS == 1)
    #define SINPUT_MASK_BUMPERS 0x0C
#else
    #define SINPUT_MASK_BUMPERS 0
#endif

// Triggers (L2, R2)
#if (defined(HOJA_INPUT_ENABLE_TRIGGERS) && HOJA_INPUT_ENABLE_TRIGGERS == 1)
    #define SINPUT_MASK_TRIGGERS 0x30
#else
    #define SINPUT_MASK_TRIGGERS 0
#endif

// Start + Select
#if (defined(HOJA_INPUT_ENABLE_STARTSELECT) && HOJA_INPUT_ENABLE_STARTSELECT == 1)
    #define SINPUT_MASK_STARTSELECT 0x03
#else
    #define SINPUT_MASK_STARTSELECT 0
#endif

// Home
#if (defined(HOJA_INPUT_ENABLE_HOME) && HOJA_INPUT_ENABLE_HOME == 1)
    #define SINPUT_MASK_HOME 0x04
#else
    #define SINPUT_MASK_HOME 0
#endif

// Capture
#if (defined(HOJA_INPUT_ENABLE_CAPTURE) && HOJA_INPUT_ENABLE_CAPTURE == 1)
    #define SINPUT_MASK_CAPTURE 0x08
#else
    #define SINPUT_MASK_CAPTURE 0
#endif

// Stick Click: L3
#if defined(HOJA_ADC_LX_CFG)
    #define SINPUT_MASK_LSTICK 0x01
#else
    #define SINPUT_MASK_LSTICK 0
#endif

// Stick Click: R3
#if defined(HOJA_ADC_RX_CFG)
    #define SINPUT_MASK_RSTICK 0x02
#else
    #define SINPUT_MASK_RSTICK 0
#endif

// Upper Grips (L4, R4)
#if (defined(HOJA_INPUT_ENABLE_UPPERGRIPS) && HOJA_INPUT_ENABLE_UPPERGRIPS == 1)
    #define SINPUT_MASK_UPPERGRIPS 0xC0
#else
    #define SINPUT_MASK_UPPERGRIPS 0
#endif

// Lower Grips (L5, R5)
#if (defined(HOJA_INPUT_ENABLE_LOWERGRIPS) && HOJA_INPUT_ENABLE_LOWERGRIPS == 1)
    #define SINPUT_MASK_LOWERGRIPS 0x30
#else
    #define SINPUT_MASK_LOWERGRIPS 0
#endif

// Power
#if (defined(HOJA_INPUT_ENABLE_POWER) && HOJA_INPUT_ENABLE_POWER == 1)
    #define SINPUT_MASK_POWER 0x01
#else 
    #define SINPUT_MASK_POWER 0
#endif

#define SINPUT_MASK_0 ( SINPUT_MASK_SEWN | SINPUT_MASK_DPAD )
#define SINPUT_MASK_1 ( SINPUT_MASK_LSTICK | SINPUT_MASK_RSTICK | SINPUT_MASK_TRIGGERS | SINPUT_MASK_BUMPERS | SINPUT_MASK_UPPERGRIPS )
#define SINPUT_MASK_2 ( SINPUT_MASK_STARTSELECT | SINPUT_MASK_HOME | SINPUT_MASK_CAPTURE | SINPUT_MASK_LOWERGRIPS )
#define SINPUT_MASK_3 ( SINPUT_MASK_POWER )

const ext_tusb_desc_device_t sinput_device_descriptor = {
    .bLength = 18,
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0210, // Changed from 0x0200 to 2.1 for BOS & WebUSB
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,

    .bMaxPacketSize0 = 64,

    #if defined(HOJA_USB_VID)
    .idVendor = HOJA_USB_VID,
    #else
    .idVendor = 0x2E8A, // Raspberry Pi
    #endif
    
    #if defined(HOJA_USB_PID)
    .idProduct = HOJA_USB_PID, // board_config PID
    #else
    .idProduct = 0x10C6, // Hoja Gamepad
    #endif

    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
    };

const uint8_t sinput_hid_report_descriptor[139] = {
    0x05, 0x01,                    // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,                    // Usage (Gamepad)
    0xA1, 0x01,                    // Collection (Application)
    
    // INPUT REPORT ID 0x01 - Main gamepad data
    0x85, 0x01,                    //   Report ID (1)
    
    // Padding bytes (bytes 2-3) - Plug status and Charge Percent (0-100)
    0x06, 0x00, 0xFF,              //   Usage Page (Vendor Defined)
    0x09, 0x01,                    //   Usage (Vendor Usage 1)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0xFF,                    //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8)
    0x95, 0x02,                    //   Report Count (2)
    0x81, 0x02,                    //   Input (Data,Var,Abs)

    // --- 32 buttons ---
    0x05, 0x09,        // Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (Button 1)
    0x29, 0x20,        //   Usage Maximum (Button 32)
    0x15, 0x00,        //   Logical Min (0)
    0x25, 0x01,        //   Logical Max (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x20,        //   Report Count (32)
    0x81, 0x02,        //   Input (Data,Var,Abs)
    
    // Analog Sticks and Triggers
    0x05, 0x01,                    // Usage Page (Generic Desktop)
    // Left Stick X (bytes 8-9)
    0x09, 0x30,                    //   Usage (X)
    // Left Stick Y (bytes 10-11)
    0x09, 0x31,                    //   Usage (Y)
    // Right Stick X (bytes 12-13)
    0x09, 0x32,                    //   Usage (Z)
    // Right Stick Y (bytes 14-15)
    0x09, 0x35,                    //   Usage (Rz)
    // Right Trigger (bytes 18-19)
    0x09, 0x33,                    //   Usage (Rx)
    // Left Trigger  (bytes 16-17)
    0x09, 0x34,                     //  Usage (Ry)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x06,                    //   Report Count (6)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // Padding/Reserved data (bytes 20-63) - 44 bytes
    // This includes gyro/accel data and counter that apps can use if supported
    0x06, 0x00, 0xFF,              // Usage Page (Vendor Defined)
    
    // Motion Input Timestamp (Microseconds)
    0x09, 0x20,                    //   Usage (Vendor Usage 0x20)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0xFF,              //   Logical Maximum (655535)
    0x75, 0x20,                    //   Report Size (32)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x02,                    //   Input (Data,Var,Abs)

    // Motion Input Accelerometer XYZ (Gs) and Gyroscope XYZ (Degrees Per Second)
    0x09, 0x21,                    //   Usage (Vendor Usage 0x21)
    0x16, 0x00, 0x80,              //   Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,              //   Logical Maximum (32767)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x06,                    //   Report Count (6)
    0x81, 0x02,                    //   Input (Data,Var,Abs)

    // Reserved padding
    0x09, 0x22,                    //   Usage (Vendor Usage 0x22)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8)
    0x95, 0x1D,                    //   Report Count (29)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    
    // INPUT REPORT ID 0x02 - Vendor COMMAND data
    0x85, 0x02,                    //   Report ID (2)
    0x09, 0x23,                    //   Usage (Vendor Usage 0x23)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8 bits)
    0x95, 0x3F,                    //   Report Count (63) - 64 bytes minus report ID
    0x81, 0x02,                    //   Input (Data,Var,Abs)

    // OUTPUT REPORT ID 0x03 - Vendor COMMAND data
    0x85, 0x03,                    //   Report ID (3)
    0x09, 0x24,                    //   Usage (Vendor Usage 0x24)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8 bits)
    0x95, 0x2F,                    //   Report Count (47) - 48 bytes minus report ID
    0x91, 0x02,                    //   Output (Data,Var,Abs)

    0xC0                           // End Collection 
};

const uint8_t sinput_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, 41, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 350),

    // Interface
    9, TUSB_DESC_INTERFACE, 0x00, 0x00, 0x02, TUSB_CLASS_HID, 0x00, 0x00, 0x00,
    // HID Descriptor
    9, HID_DESC_TYPE_HID, U16_TO_U8S_LE(0x0111), 0, 1, HID_DESC_TYPE_REPORT, U16_TO_U8S_LE(sizeof(sinput_hid_report_descriptor)),
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 1,
    // Endpoint Descriptor
    7, TUSB_DESC_ENDPOINT, 0x01, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(64), 4
};

void sinput_cmd_haptics(const uint8_t *data)
{
    haptic_processed_s val = {0};

    sinput_haptic_s haptic = {0};

    memcpy(&haptic, data, sizeof(sinput_haptic_s));

    switch (haptic.type)
    {
        default:
        break;

        case 1:
            float a1_base = (float) (haptic.type_1.left.amplitude_1 > haptic.type_1.right.amplitude_1 ? haptic.type_1.left.amplitude_1 : haptic.type_1.right.amplitude_1);
            float a2_base = (float) (haptic.type_1.left.amplitude_2 > haptic.type_1.right.amplitude_2 ? haptic.type_1.left.amplitude_2 : haptic.type_1.right.amplitude_2);
            
            float amp_1 = a1_base > 0 ? (float) a1_base / (float) UINT16_MAX : 0;
            float amp_2 = a2_base > 0 ? (float) a2_base / (float) UINT16_MAX : 0;
        
            val.hi_amplitude_fixed = pcm_amplitude_to_fixedpoint(amp_1);
            val.lo_amplitude_fixed = pcm_amplitude_to_fixedpoint(amp_2);
        
            val.hi_frequency_increment = pcm_frequency_to_fixedpoint_increment((float) haptic.type_1.left.frequency_1);
            val.lo_frequency_increment = pcm_frequency_to_fixedpoint_increment((float) haptic.type_1.left.frequency_2);
        
            pcm_amfm_push(&val);
        break;

        case 2:
            bool left_right = haptic.type_2.left.amplitude > haptic.type_2.right.amplitude ? false : true; 

            uint8_t amp = 0;
            bool brake = 0;

            if(left_right)
            {
                amp = haptic.type_2.right.amplitude;
                brake = haptic.type_2.right.brake;
            }
            else 
            {
                amp = haptic.type_2.left.amplitude;
                brake = haptic.type_2.left.brake;
            }

            pcm_erm_set(amp, brake);
        break;
    } 
}

// Get Feature Flags
void _sinput_cmd_get_features(uint8_t *buffer)
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
        SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO,
        SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT,
        SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT,
        SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR,
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
    
    if(hoja_get_status().gamepad_method==GAMEPAD_METHOD_BLUETOOTH)
        polling_rate_us = 8000;
        
    memcpy(&buffer[6], &polling_rate_us, sizeof(polling_rate_us));

    memcpy(&buffer[8], &accel_g_range, sizeof(accel_g_range)); // Accelerometer G range
    memcpy(&buffer[10], &gyro_dps_range, sizeof(gyro_dps_range)); // Gyroscope DPS range

    buffer[12] = SINPUT_MASK_0;
    buffer[13] = SINPUT_MASK_1;
    buffer[14] = SINPUT_MASK_2;
    buffer[15] = SINPUT_MASK_3;

    buffer[16] = 0; // Touchpad count
    buffer[17] = 0; // Touchpad finger count

    buffer[18] = gamepad_config->switch_mac_address[0];
    buffer[19] = gamepad_config->switch_mac_address[1];
    buffer[20] = gamepad_config->switch_mac_address[2];
    buffer[21] = gamepad_config->switch_mac_address[3];
    buffer[22] = gamepad_config->switch_mac_address[4];
    buffer[23] = gamepad_config->switch_mac_address[5] + (uint8_t) hoja_get_status().gamepad_mode;
}

volatile uint8_t _sinput_current_command = 0;

void sinput_hid_handle_command_future(const uint8_t *data)
{
    switch(data[0])
    {
        default:
        break;

        case SINPUT_COMMAND_HAPTIC:
        sinput_cmd_haptics(&(data[1]));
        break;

        case SINPUT_COMMAND_FEATURES:
        _sinput_current_command = data[0];
        break;

        case SINPUT_COMMAND_PLAYERLED:
        uint8_t player_num = data[1];
        player_num = (player_num > 8) ? 8 : player_num; // Cap at 8
        hoja_set_connected_status(player_num);
        break;

    }
}

void _sinput_hid_handle_command(uint8_t command, const uint8_t *data, hid_report_tunnel_cb cb)
{
    uint8_t _sinput_current_command_reply[63] = {command};

    switch(command)
    {
        default:
        break;

        case SINPUT_COMMAND_FEATURES:
        _sinput_cmd_get_features(&_sinput_current_command_reply[1]);
        cb(REPORT_ID_SINPUT_INPUT_CMDDAT, _sinput_current_command_reply, 63);
        
        break;
    }

    // Clear command
    _sinput_current_command = 0;
}

int16_t sinput_scale_trigger(uint16_t val)
{
    if (val > 4095) val = 4095; // Clamp just in case

    // Scale: map [0, 4095] → [INT16_MIN, INT16_MAX]
    // The range of INT16 is 65535, so multiply first to preserve precision
    return (int16_t)(((int32_t)val * 65535) / 4095 + INT16_MIN);
}

#define SINPUT_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

int16_t sinput_scale_axis(int16_t input_axis)
{   
    return SINPUT_CLAMP(input_axis * 16, INT16_MIN, INT16_MAX);
}

void sinput_hid_report(uint64_t timestamp, hid_report_tunnel_cb cb)
{
    static uint8_t report_data[64] = {0};
    static sinput_input_s data;

    battery_status_s bstat = {0};
    battery_get_status(&bstat);

    fuelgauge_status_s fstat = {0};
    fuelgauge_get_status(&fstat);

    if(bstat.connected)
    {
        if(bstat.charging)
        {
            data.plug_status = 2; // Charging
        }
        else if (bstat.plugged)
        {
            data.plug_status = 3; // Not Charging/Charge Complete
        }
        else 
        {
            data.plug_status = 4; // On battery power
        }
    }
    else 
    {
        data.plug_status = 0; // Plugged
    }

    if(fstat.connected)
    {
        data.charge_percent = fstat.percent;
    }
    else 
    {
        data.charge_percent = 100; // 100 Percent
    }

    mapper_input_s *input = mapper_get_input();

    //#define HOJA_ANALOG_POLLING_TEST
    #if defined(HOJA_ANALOG_POLLING_TEST)
    // For testing purposes, use the analog test values
    static int16_t test = 0;
    data.left_x = test;
    data.left_y = test;

    test+=100;
    if(test > -8000 && test < 8000) test +=16000;
    data.right_x = 0;
    data.right_y = 0;
    #else
    data.left_x = sinput_scale_axis(input->joysticks_combined[0]);
    data.left_y = sinput_scale_axis(-input->joysticks_combined[1]);
    data.right_x = sinput_scale_axis(input->joysticks_combined[2]);
    data.right_y = sinput_scale_axis(-input->joysticks_combined[3]);
    #endif

    static imu_data_s imu = {0};
    imu_access_safe(&imu);

    data.accel_x = imu.ax; 
    data.accel_y = imu.ay;
    data.accel_z = imu.az;

    data.gyro_x = imu.gx;
    data.gyro_y = imu.gy;
    data.gyro_z = imu.gz; 

    data.imu_timestamp_us = (uint32_t) (imu.timestamp & UINT32_MAX);

    // Buttons
    data.button_east   = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_EAST);
    data.button_south  = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_SOUTH);
    data.button_north  = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_NORTH);
    data.button_west   = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_WEST);

    data.button_stick_left  = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_LS);
    data.button_stick_right = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_RS);

    data.button_start  = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_START);
    data.button_select = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_SELECT);
    data.button_guide  = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_HOME);
    data.button_share  = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_CAPTURE);

    data.dpad_down  = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_DOWN);
    data.dpad_up    = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_UP);
    data.dpad_left  = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_LEFT);
    data.dpad_right = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_RIGHT);

    data.button_l_shoulder = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_LB);
    data.button_r_shoulder = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_RB);

    data.button_l_paddle_1 = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_LG_UPPER);
    data.button_r_paddle_1 = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_RG_UPPER);

    data.button_power = input->button_shipping;

    int16_t l_analog = sinput_scale_trigger(input->triggers[0]);
    int16_t r_analog = sinput_scale_trigger(input->triggers[1]);
    bool l_digital = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_LT);
    bool r_digital = MAPPER_BUTTON_DOWN(input->digital_inputs, MAPPER_CODE_RT);

    if(trigger_config->left_disabled == 1)
    {
        if(l_digital)
            data.trigger_l = INT16_MAX;
        else 
            data.trigger_l = INT16_MIN;
    }
    else 
    {
        data.trigger_l = l_analog;
        data.button_l_trigger = l_digital;
    }

    if(trigger_config->right_disabled == 1)
    {
        if(r_digital)
            data.trigger_r = INT16_MAX;
        else 
            data.trigger_r = INT16_MIN;
    }
    else 
    {
        data.trigger_r = r_analog;
        data.button_r_trigger = r_digital;
    }

    memcpy(report_data, &data, sizeof(sinput_input_s));

    if(_sinput_current_command != 0)
    {
        _sinput_hid_handle_command(_sinput_current_command, NULL, cb);
    }
    else cb(0x01, report_data, sizeof(sinput_input_s));
}
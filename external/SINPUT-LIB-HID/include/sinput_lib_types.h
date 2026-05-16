#ifndef SINPUT_LIB_TYPES_H
#define SINPUT_LIB_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result of configuration validation or operations that depend on a configured device.
 */
typedef enum
{
    SINPUT_CONFIG_OK = 0,
    /** Device identity / transport / output hook still default or incomplete. */
    SINPUT_CONFIG_NOT_CONFIGURED,
    /** Bad pointer or inconsistent arguments. */
    SINPUT_CONFIG_INVALID_ARG,
} sinput_config_status_t;

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

typedef struct
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
} sinput_stereo_haptics_s;

typedef struct
{
    struct
    {
        uint8_t amplitude;
        bool brake;
    } left;
    struct
    {
        uint8_t amplitude;
        bool brake;
    } right;
} sinput_stereo_rumble_s;

typedef enum
{
    SINPUT_CONNSTAT_PLUGGED_NO_BATTERY = 0, // Connected to a power source, no battery supported or available
    SINPUT_CONNSTAT_PLUGGED_CHARGING = 1, // Connected to a power source, and the battery is charging
    SINPUT_CONNSTAT_PLUGGED_CHARGED = 2, // Connected to a power source, and battery charging is done
    SINPUT_CONNSTAT_UNPLUGGED = 3, // Device is unplugged from a power source (Running on battery power)
} sinput_connstat_t;

typedef struct
{
    uint8_t charge_percent;
    sinput_connstat_t connection_status;
} sinput_power_s;

typedef enum
{
    SINPUT_SDL_GAMEPAD_TYPE_UNKNOWN = 0,
    SINPUT_SDL_GAMEPAD_TYPE_STANDARD,
    SINPUT_SDL_GAMEPAD_TYPE_XBOX360,
    SINPUT_SDL_GAMEPAD_TYPE_XBOXONE,
    SINPUT_SDL_GAMEPAD_TYPE_PS3,
    SINPUT_SDL_GAMEPAD_TYPE_PS4,
    SINPUT_SDL_GAMEPAD_TYPE_PS5,
    SINPUT_SDL_GAMEPAD_TYPE_NINTENDO_PRO,
    SINPUT_SDL_GAMEPAD_TYPE_NINTENDO_JOYCON_LEFT,
    SINPUT_SDL_GAMEPAD_TYPE_NINTENDO_JOYCON_RIGHT,
    SINPUT_SDL_GAMEPAD_TYPE_NINTENDO_JOYCON_PAIR,
    SINPUT_SDL_GAMEPAD_TYPE_GAMECUBE,
    SINPUT_SDL_GAMEPAD_TYPE_COUNT
} sinput_sdl_gamepad_type_t;

typedef enum
{
    SINPUT_SDL_GAMEPAD_FACE_STYLE_UNKNOWN, // Xbox default
    SINPUT_SDL_GAMEPAD_FACE_STYLE_ABXY, // Xbox Style
    SINPUT_SDL_GAMEPAD_FACE_STYLE_AXBY, // GameCube Style
    SINPUT_SDL_GAMEPAD_FACE_STYLE_BAYX, // Nintendo Style
    SINPUT_SDL_GAMEPAD_FACE_STYLE_SONY, // PS4/PS5 Style
} sinput_sdl_face_style_t;

typedef enum
{
    SINPUT_GAMEPAD_FORMAT_JOYPAD, // Separate gamepad body
    SINPUT_GAMEPAD_FORMAT_HANDHELD, // Controls are attached to console (Steam Deck etc)
} sinput_gamepad_format_t;

typedef struct
{
    const char device_name[64];
    uint16_t polling_rate_us; // Polling rate in microseconds
    uint8_t mac_address[6]; // Device MAC address OR serial number
    struct
    {
        bool sewn; // ABXY buttons, PS Style face buttons (grouping of 4)
        bool dpad; // Up, Down, Left, Right dpad
        bool bumpers; // Left and Right bumpers
        bool grips_upper; // Upper grouping of 2 grip buttons
        bool grips_lower; // Lower grouping of 2 grip buttons
        bool start_select; // Has start+select buttons
        bool home; // Has Home/Guide button
        bool share; // Has Capture/Share button
        bool power; // Has a power button
        bool misc1; // Extra button 1
        bool misc2; // Extra button 2
        bool misc3; // Extra button 3
    } buttons;
    struct
    {
        bool touchpad_left;
        bool touchpad_right;
    } touchpads;
    struct
    {
        bool accelerometer;
        bool gyroscope;
        uint16_t accelerometer_g_range;
        uint16_t gyroscope_dps_range;
    } motion;
    struct
    {
        bool left; // Right analog joystick supported
        bool right; // Left analog joystick supported
    } joysticks;
    struct
    {
        bool left; // Left analog trigger supported
        bool right; // Right analog trigger supported
    } triggers; // Analog triggers
    bool rumble;
    struct
    {
        bool player; // Player LEDs supported
        bool joystick; // Joystick RGB LED supported
    } leds;
    sinput_gamepad_format_t gamepad_format;
    sinput_sdl_gamepad_type_t gamepad_type;
    sinput_sdl_face_style_t face_buttons_style;
} sinput_device_cfg_s;

typedef struct 
{
    uint8_t south   : 1; // LSB
    uint8_t east    : 1;
    uint8_t west    : 1;
    uint8_t north   : 1;
    uint8_t dpad_up    : 1;
    uint8_t dpad_down  : 1;
    uint8_t dpad_left  : 1;
    uint8_t dpad_right : 1;

    uint8_t stick_left : 1; // LSB
    uint8_t stick_right : 1;
    uint8_t l_shoulder : 1;
    uint8_t r_shoulder : 1;
    uint8_t l_trigger : 1;
    uint8_t r_trigger : 1;
    uint8_t l_grip_1 : 1;
    uint8_t r_grip_1 : 1;

    uint8_t start  : 1; // LSB
    uint8_t select : 1;
    uint8_t guide  : 1;
    uint8_t share  : 1;
    uint8_t l_grip_2 : 1;
    uint8_t r_grip_2 : 1;
    uint8_t l_touchpad : 1;
    uint8_t r_touchpad : 1;

    uint8_t power   : 1; // LSB
    uint8_t misc_1  : 1;
    uint8_t misc_2  : 1;
    uint8_t misc_3  : 1;
    
    uint8_t reserved : 4;
} sinput_buttons_s;

typedef struct
{
    struct
    {
        int16_t x;
        int16_t y;
    } left;
    struct
    {
        int16_t x;
        int16_t y;
    } right;
} sinput_joysticks_s;

typedef struct
{
    uint32_t timestamp_us;
    struct
    {
        int16_t x;
        int16_t y;
        int16_t z;
    } gyro;

    struct
    {
        int16_t x;
        int16_t y;
        int16_t z;
    } accel;
} sinput_motion_s;

typedef struct
{
    struct
    {
        int16_t x;
        int16_t y;
        int16_t pressure;
    } left;
    struct
    {
        int16_t x;
        int16_t y;
        int16_t pressure;
    } right;
} sinput_touchpads_s;

typedef struct
{
    uint16_t left;
    uint16_t right;
} sinput_triggers_s;

/**
 * @brief USB device descriptor in wire layout (little-endian multi-byte fields as @c uint16_t).
 *
 * @note @c bLength must equal @c NS_USB_DEVICE_DESCRIPTOR_LEN (see @c ns_lib_hid.h). This struct is not a HID report descriptor.
 */
#pragma pack(push, 1)
typedef struct{
    uint8_t  bLength            ; ///< Size of this descriptor in bytes.
    uint8_t  bDescriptorType    ; ///< DEVICE Descriptor Type.
    uint16_t bcdUSB             ; ///< BUSB Specification Release Number in Binary-Coded Decimal (i.e., 2.10 is 210H).

    uint8_t  bDeviceClass       ; ///< Class code (assigned by the USB-IF).
    uint8_t  bDeviceSubClass    ; ///< Subclass code (assigned by the USB-IF).
    uint8_t  bDeviceProtocol    ; ///< Protocol code (assigned by the USB-IF).
    uint8_t  bMaxPacketSize0    ; ///< Maximum packet size for endpoint zero (only 8, 16, 32, or 64 are valid). For HS devices is fixed to 64.

    uint16_t idVendor           ; ///< Vendor ID (assigned by the USB-IF).
    uint16_t idProduct          ; ///< Product ID (assigned by the manufacturer).
    uint16_t bcdDevice          ; ///< Device release number in binary-coded decimal.
    uint8_t  iManufacturer      ; ///< Index of string descriptor describing manufacturer.
    uint8_t  iProduct           ; ///< Index of string descriptor describing product.
    uint8_t  iSerialNumber      ; ///< Index of string descriptor describing the device's serial number.

    uint8_t  bNumConfigurations ; ///< Number of possible configurations.
} sinput_usb_device_descriptor_t;
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif
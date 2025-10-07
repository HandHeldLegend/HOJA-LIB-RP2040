#include "utilities/static_config.h"
#include "hoja.h"
#include "board_config.h"
#include "usb/webusb.h"

// Bluetooth driver nonsense
#include "devices/bluetooth.h"

#include <string.h>

#if !defined(HOJA_DEVICE_NAME)
 #warning "HOJA_DEVICE_NAME undefined in board_config.h"
 #define DEVICE_NAME "HOJA GamePad"
#else 
    #define DEVICE_NAME HOJA_DEVICE_NAME 
#endif

#if !defined(HOJA_DEVICE_MAKER)
 #warning "HOJA_DEVICE_MAKER undefined in board_config.h"
 #define DEVICE_MAKER "HHL"
#else 
    #define DEVICE_MAKER HOJA_DEVICE_MAKER 
#endif

#if !defined(HOJA_DEVICE_SNES_SUPPORTED)
 #warning "HOJA_DEVICE_SNES_SUPPORTED undefined. SNES/NES disabled."
 #define SNES_SUPPORT 0
#else 
 #define SNES_SUPPORT HOJA_DEVICE_SNES_SUPPORTED
#endif

#if !defined(HOJA_DEVICE_JOYBUS_SUPPORTED)
 #warning "HOJA_DEVICE_JOYBUS_SUPPORTED undefined. N64/GameCube disabled."
 #define JOYBUS_SUPPORT 0
#else 
 #define JOYBUS_SUPPORT HOJA_DEVICE_JOYBUS_SUPPORTED
#endif

#if !defined(HOJA_DEVICE_MANIFEST_URL) 
 #warning "HOJA_DEVICE_MANIFEST_URL undefined. Update notifications disabled."
 #define MANIFEST_URL "~"
#else 
 #define MANIFEST_URL HOJA_DEVICE_MANIFEST_URL
#endif

#if defined(HOJA_DEVICE_MANIFEST_URL)
    #if !defined(HOJA_DEVICE_FIRMWARE_URL) 
        #error "HOJA_DEVICE_FIRMWARE_URL must be defined for firmware update notifications." 
    #else 
        #define FIRMWARE_URL HOJA_DEVICE_FIRMWARE_URL
    #endif
#else 
    #define FIRMWARE_URL "~"
#endif

#if !defined(HOJA_DEVICE_FCC_ID_TEXT) 
    #define FCC_ID_TEXT "~"
#else 
    #define FCC_ID_TEXT HOJA_DEVICE_FCC_ID_TEXT
#endif

#if defined(HOJA_DEVICE_MANUAL_URL)
    #define MANUAL_URL HOJA_DEVICE_MANUAL_URL
#else 
    #define MANUAL_URL "~"
    #warning "HOJA_DEVICE_MANUAL_URL undefined. Include to enable documentation URL in app."
#endif

const deviceInfoStatic_s    device_static = {
    .fw_version     = FIRMWARE_VERSION_TIMESTAMP,
    .maker          = DEVICE_MAKER, 
    .name           = DEVICE_NAME,
    .firmware_url   = FIRMWARE_URL,
    .manifest_url   = MANIFEST_URL,
    .fcc_id         = FCC_ID_TEXT,
    .manual_url     = MANUAL_URL,
    .snes_supported = SNES_SUPPORT,
    .joybus_supported = JOYBUS_SUPPORT
};

#if defined(HOJA_ADC_LX_CFG) 
 #define ALX (1)
#else 
 #define ALX (0)
#endif

#if defined(HOJA_ADC_LY_CFG) 
 #define ALY (1)
#else 
 #define ALY (0)
#endif

#if defined(HOJA_ADC_RX_CFG) 
 #define ARX (1)
#else 
 #define ARX (0)
#endif

#if defined(HOJA_ADC_RY_CFG) 
 #define ARY (1)
#else 
 #define ARY (0)
#endif

#if defined(HOJA_ADC_LT_CFG) 
 #define ALT (1)
#else 
 #define ALT (0)
#endif

#if defined(HOJA_ADC_RT_CFG) 
 #define ART (1)
#else 
 #define ART (0)
#endif

#define ANALOG_SUPPORTED (ALX | ALY | ARX | ARY | ALT | ART)

const analogInfoStatic_s    analog_static = {
    .axis_lx = ALX,
    .axis_ly = ALY,
    .axis_rx = ARX,
    .axis_ry = ARY,
    .axis_lt = ALT,
    .axis_rt = ART,
};

#if defined(HOJA_IMU_CHAN_A_DRIVER)
    #define IMU_AVAILABLE 1
#else 
    #define IMU_AVAILABLE 0
#endif

const imuInfoStatic_s       imu_static = {
    .axis_gyro_a  = IMU_AVAILABLE,
    .axis_gyro_b  = IMU_AVAILABLE,
    .axis_accel_a = IMU_AVAILABLE,
    .axis_accel_b = IMU_AVAILABLE,
};

#if !defined(HOJA_BATTERY_CAPACITY_MAH)
 #warning "HOJA_BATTERY_CAPACITY_MAH undefined in board_config.h. Battery features will be disabled."
 #define HOJA_BATTERY_CAPACITY_MAH 0
#endif

#if !defined(HOJA_BATTERY_PART_CODE)
 #warning "HOJA_BATTERY_PART_CODE undefined in board_config.h."
 #define HOJA_BATTERY_PART_CODE "N/A"
#endif

const batteryInfoStatic_s   battery_static = {
    .capacity_mah = HOJA_BATTERY_CAPACITY_MAH,
    .part_number  = HOJA_BATTERY_PART_CODE,
};

#if !defined(HOJA_HAPTICS_DRIVER)
    #warning "HOJA_HAPTICS_DRIVER is not defined in board_config.h. Rumble features will be disabled."
    #define HAPTICS_HD_EN 0
    #define HAPTICS_SD_EN 0
#else 
    #if (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_LRA_HAL)
        #define HAPTICS_HD_EN 1
    #else 
        #define HAPTICS_HD_EN 0
    #endif

    #if (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_ERM_HAL)
        #define HAPTICS_SD_EN 1
    #else
        #define HAPTICS_SD_EN 0
    #endif
#endif

const hapticInfoStatic_s    haptic_static = {
    .haptic_hd = HAPTICS_HD_EN,
    .haptic_sd = HAPTICS_SD_EN,
};


// SEWN (South, East, West, North)
#if (defined(HOJA_INPUT_ENABLE_SEWN) && HOJA_INPUT_ENABLE_SEWN == 1)
    #define STATIC_REMAP_SEWN 0x0000000F  // bits 0–3
#else
    #define STATIC_REMAP_SEWN 0
#endif

// DPAD (Up, Down, Left, Right)
#if (defined(HOJA_INPUT_ENABLE_DPAD) && HOJA_INPUT_ENABLE_DPAD == 1)
    #define STATIC_REMAP_DPAD 0x000000F0  // bits 4–7
#else
    #define STATIC_REMAP_DPAD 0
#endif

// Bumpers (L1, R1)
#if (defined(HOJA_INPUT_ENABLE_BUMPERS) && HOJA_INPUT_ENABLE_BUMPERS == 1)
    #define STATIC_REMAP_BUMPERS 0x00000300  // bits 8–9
#else
    #define STATIC_REMAP_BUMPERS 0
#endif

// Triggers (L2, R2)
#if (defined(HOJA_INPUT_ENABLE_TRIGGERS) && HOJA_INPUT_ENABLE_TRIGGERS == 1)
    #define STATIC_REMAP_TRIGGERS 0x00000C00  // bits 10–11
#else
    #define STATIC_REMAP_TRIGGERS 0
#endif

// Start + Select
#if (defined(HOJA_INPUT_ENABLE_STARTSELECT) && HOJA_INPUT_ENABLE_STARTSELECT == 1)
    #define STATIC_REMAP_STARTSELECT 0x00003000  // bits 12–13
#else
    #define STATIC_REMAP_STARTSELECT 0
#endif

// Home
#if (defined(HOJA_INPUT_ENABLE_HOME) && HOJA_INPUT_ENABLE_HOME == 1)
    #define STATIC_REMAP_HOME 0x00004000  // bit 14
#else
    #define STATIC_REMAP_HOME 0
#endif

// Capture
#if (defined(HOJA_INPUT_ENABLE_CAPTURE) && HOJA_INPUT_ENABLE_CAPTURE == 1)
    #define STATIC_REMAP_CAPTURE 0x00008000  // bit 15
#else
    #define STATIC_REMAP_CAPTURE 0
#endif

// Stick Click: L3
#if (ALX == 1)
    #define STATIC_REMAP_L3 0x00010000  // bit 16
#else
    #define STATIC_REMAP_L3 0
#endif

// Stick Click: R3
#if (ARX == 1)
    #define STATIC_REMAP_R3 0x00020000  // bit 17
#else
    #define STATIC_REMAP_R3 0
#endif

// Upper Grips (L4, R4)
#if (defined(HOJA_INPUT_ENABLE_UPPERGRIPS) && HOJA_INPUT_ENABLE_UPPERGRIPS == 1)
    #define STATIC_REMAP_UPPERGRIPS 0x000C0000  // bits 18–19
#else
    #define STATIC_REMAP_UPPERGRIPS 0
#endif

// Lower Grips (L5, R5)
#if (defined(HOJA_INPUT_ENABLE_LOWERGRIPS) && HOJA_INPUT_ENABLE_LOWERGRIPS == 1)
    #define STATIC_REMAP_LOWERGRIPS 0x00300000  // bits 20–21
#else
    #define STATIC_REMAP_LOWERGRIPS 0
#endif

// Analog Trigger Axes (L2 Analog, R2 Analog)
#if (ALT == 1 || ART == 1)
    #define STATIC_REMAP_TRIGGERAXES 0x00C00000  // bits 22–23
#else
    #define STATIC_REMAP_TRIGGERAXES 0
#endif

// Left Stick Axes (LX+, LX-, LY+, LY-)
#if (ALX == 1)
    #define STATIC_REMAP_LSTICK 0x0F000000  // bits 24–27
#else
    #define STATIC_REMAP_LSTICK 0
#endif

// Right Stick Axes (RX+, RX-, RY+, RY-)
#if (ARX == 1)
    #define STATIC_REMAP_RSTICK 0xF0000000  // bits 28–31
#else
    #define STATIC_REMAP_RSTICK 0
#endif

#define STATIC_REMAP_ALL ( \
    STATIC_REMAP_SEWN        | \
    STATIC_REMAP_DPAD        | \
    STATIC_REMAP_BUMPERS     | \
    STATIC_REMAP_TRIGGERS    | \
    STATIC_REMAP_STARTSELECT | \
    STATIC_REMAP_HOME        | \
    STATIC_REMAP_CAPTURE     | \
    STATIC_REMAP_L3          | \
    STATIC_REMAP_R3          | \
    STATIC_REMAP_UPPERGRIPS  | \
    STATIC_REMAP_LOWERGRIPS  | \
    STATIC_REMAP_TRIGGERAXES | \
    STATIC_REMAP_LSTICK      | \
    STATIC_REMAP_RSTICK )

const buttonInfoStatic_s buttons_static = {
    .remap_mask = STATIC_REMAP_ALL
};

#if defined(HOJA_BLUETOOTH_DRIVER)
    #define BTSUPPORT 1 
    #define BT_BASEBAND_TYPE HOJA_BLUETOOTH_DRIVER
    #define BT_BASEBAND_VERSION 0
#else 
    #warning "HOJA_BLUETOOTH_DRIVER undefined. Bluetooth features will be disabled."
    #define BT_BASEBAND_TYPE 0
    #define BT_BASEBAND_VERSION 0
    #define BTSUPPORT 0
#endif

// Dynamic BT versoin
bluetoothInfoStatic_s bluetooth_static = {
    .bluetooth_bdr = BTSUPPORT,
    .bluetooth_ble = BTSUPPORT,
    .baseband_type = BT_BASEBAND_TYPE,
    .baseband_version = BT_BASEBAND_VERSION,
};

#if !defined(HOJA_RGB_GROUPS_NUM)
    #warning "HOJA_RGB_GROUPS_NUM undefined in board_config.h. RGB features will be disabled"
    #define RGB_GROUPS 0
    #define RGB_GROUP_NAMES {0}
#else
    #if (HOJA_RGB_GROUPS_NUM > 32)
        #error "HOJA_RGB_GROUPS_NUM must be 32 or less!"
    #endif 
        #define RGB_GROUPS HOJA_RGB_GROUPS_NUM

    #if !defined(HOJA_RGB_GROUP_NAMES) 
        #error "You must provide the names as a 2d array as HOJA_RGB_GROUP_NAMES. 8 ASCII characters, 32 groups max." 
    #else
        #define RGB_GROUP_NAMES HOJA_RGB_GROUP_NAMES 
    #endif
#endif  

#if !defined(HOJA_RGB_PLAYER_GROUP_IDX)
    #warning "HOJA_RGB_PLAYER_GROUP_IDX is undefined. Player number indicator will be unused."
    #define PLAYER_GROUP -1
#else 
    #define PLAYER_GROUP HOJA_RGB_PLAYER_GROUP_IDX 
#endif

uint8_t _rgb_names[32][8] = RGB_GROUP_NAMES;

rgbInfoStatic_s rgb_static = {
    .rgb_groups = RGB_GROUPS,
    .rgb_player_group = PLAYER_GROUP
};

void _rgb_static_set_names() 
{
    for(int i = 0; i < RGB_GROUPS; i++)
    {
        memcpy(&rgb_static.rgb_group_names[i].rgb_group_name[0], 
        &_rgb_names[i][0], 8);
    }
}

#define BLOCK_CHUNK_MAX 32
#define BLOCK_REPORT_ID_IDX 0 // We typically don't use this
#define BLOCK_TYPE_IDX 1
#define BLOCK_CHUNK_SIZE_IDX 2
#define BLOCK_CHUNK_PART_IDX 3
#define BLOCK_CHUNK_BEGIN_IDX 4

#define BLOCK_CHUNK_HEADER_SIZE 4

uint8_t _serdata[64] = {0};
void _serialize_static_block(static_block_t block, uint8_t *data , uint32_t size, setting_callback_t cb)
{
    memset(_serdata, 0, 64);
    _serdata[0] = WEBUSB_ID_READ_STATIC_BLOCK;
    _serdata[BLOCK_TYPE_IDX] = (uint8_t) block;

    if(size <= BLOCK_CHUNK_MAX)
    {
        _serdata[BLOCK_CHUNK_SIZE_IDX] = (uint8_t) size;
        _serdata[BLOCK_CHUNK_PART_IDX] = 0;
        memcpy(&(_serdata[BLOCK_CHUNK_BEGIN_IDX]), data, size);
        cb(_serdata, size+BLOCK_CHUNK_HEADER_SIZE);
    }
    else 
    {
        uint32_t remaining = size;
        uint8_t idx = 0;
        while(remaining>0 && idx<200)
        {
            uint32_t loop_size = remaining <= BLOCK_CHUNK_MAX ? remaining : BLOCK_CHUNK_MAX;
            _serdata[BLOCK_CHUNK_SIZE_IDX] = loop_size;
            _serdata[BLOCK_CHUNK_PART_IDX] = idx;
            memcpy(&(_serdata[BLOCK_CHUNK_BEGIN_IDX]), &(data[idx*BLOCK_CHUNK_MAX]), loop_size);
            cb(_serdata, loop_size+BLOCK_CHUNK_HEADER_SIZE);
            
            remaining -= loop_size;
            idx += 1;
        }
    }

    // Here we send the chunk completion byte
    _serdata[BLOCK_CHUNK_SIZE_IDX] = 0;
    _serdata[BLOCK_CHUNK_PART_IDX] = 0xFF;
    cb(_serdata, BLOCK_CHUNK_HEADER_SIZE);
}

void static_config_read_all_blocks(setting_callback_t cb)
{
    
}

void static_config_read_block(static_block_t block, setting_callback_t cb)
{
    switch(block)
    {
        default:
        break;

        case STATIC_BLOCK_DEVICE:
            _serialize_static_block(block, (uint8_t *) &device_static, STATINFO_DEVICE_BLOCK_SIZE, cb);
        break;

        case STATIC_BLOCK_BUTTONS:
            _serialize_static_block(block, (uint8_t *) &buttons_static, STATINFO_DEVICE_BUTTON_SIZE, cb);
        break;

        case STATIC_BLOCK_ANALOG:
            _serialize_static_block(block, (uint8_t *) &analog_static, STATINFO_ANALOG_SIZE, cb);
        break;

        case STATIC_BLOCK_HAPTIC:
            _serialize_static_block(block, (uint8_t *) &haptic_static, STATINFO_HAPTIC_SIZE, cb);
        break;

        case STATIC_BLOCK_IMU:
            _serialize_static_block(block, (uint8_t *) &imu_static, STATINFO_IMU_SIZE, cb);
        break;

        case STATIC_BLOCK_BATTERY:
            _serialize_static_block(block, (uint8_t *) &battery_static, STATINFO_BATTERY_SIZE, cb);
        break;

        case STATIC_BLOCK_BLUETOOTH:
            bluetooth_static.baseband_type = BT_BASEBAND_TYPE;
            // Set our Bluetooth baseband version
            #if defined(HOJA_BLUETOOTH_GET_FWVERSION)
                bluetooth_static.baseband_version = HOJA_BLUETOOTH_GET_FWVERSION();
            #else 
                bluetooth_static.baseband_version = 0;
            #endif
            _serialize_static_block(block, (uint8_t *) &bluetooth_static, STATINFO_BLUETOOTH_SIZE, cb);
        break;

        case STATIC_BLOCK_RGB:
            _rgb_static_set_names();
            _serialize_static_block(block, (uint8_t *) &rgb_static, STATINFO_RGB_SIZE, cb);
        break;
    }
}
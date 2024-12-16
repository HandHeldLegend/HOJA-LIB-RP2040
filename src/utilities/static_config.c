#include "utilities/static_config.h"
#include "hoja.h"
#include "board_config.h"
#include "usb/webusb.h"

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

const deviceInfoStatic_s    device_static = {
    .fw_version = HOJA_FW_VERSION,
    .maker      = DEVICE_MAKER, 
    .name       = DEVICE_NAME
};

#if !defined(HOJA_BUTTONS_SUPPORTED_MAIN)
 #error "HOJA_BUTTONS_SUPPORTED_MAIN undefined in board_config.h"
#endif

#if !defined(HOJA_BUTTONS_SUPPORTED_SYSTEM)
 #error "HOJA_BUTTONS_SUPPORTED_SYSTEM undefined in board_config.h"
#endif

const buttonInfoStatic_s   buttons_static = {
    .main_buttons   = HOJA_BUTTONS_SUPPORTED_MAIN,
    .system_buttons = HOJA_BUTTONS_SUPPORTED_SYSTEM
};

#if defined(HOJA_ADC_LX_DRIVER) 
 #define ALX (1)
#else 
 #define ALX (0)
#endif

#if defined(HOJA_ADC_LY_DRIVER) 
 #define ALY (1)
#else 
 #define ALY (0)
#endif

#if defined(HOJA_ADC_RX_DRIVER) 
 #define ARX (1)
#else 
 #define ARX (0)
#endif

#if defined(HOJA_ADC_RY_DRIVER) 
 #define ARY (1)
#else 
 #define ARY (0)
#endif

#if defined(HOJA_ADC_LT_DRIVER) 
 #define ALT (1)
#else 
 #define ALT (0)
#endif

#if defined(HOJA_ADC_RT_DRIVER) 
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
    #define IMU_AVAILABLE 0b1111
#else 
    #define IMU_AVAILABLE 0
#endif

const imuInfoStatic_s       imu_static = {
    .axis_gyro_a = IMU_AVAILABLE,
    .axis_gyro_b = IMU_AVAILABLE,
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

const batteryInfoStatic   battery_static = {
    .capacity_mah = HOJA_BATTERY_CAPACITY_MAH,
    .part_number  = HOJA_BATTERY_PART_CODE,
};

#if defined(HOJA_CONFIG_HDRUMBLE)
    #define HDRUMBLE 1 
#else 
    #define HDRUMBLE 0
#endif 

#if defined(HOJA_CONFIG_SDRUMBLE)
    #define SDRUMBLE 1 
#else 
    #define SDRUMBLE 0 
#endif 

#if !defined(HOJA_CONFIG_HDRUMBLE) && !defined(HOJA_CONFIG_SDRUMBLE)
 #warning "Neither HOJA_CONFIG_HDRUMBLE or HOJA_CONFIG_SDRUMBLE defined in board_config.h. Rumble features will be disabled."
#endif 

#define RUMBLE_SUPPORT (HDRUMBLE | SDRUMBLE)

const hapticInfoStatic_s    haptic_static = {
    .haptic_hd = HDRUMBLE,
    .haptic_sd = SDRUMBLE,
};

#if defined(HOJA_BLUETOOTH_DRIVER)
    #define BTSUPPORT 0b11 
#else 
    #warning "HOJA_BLUETOOTH_DRIVER undefined. Bluetooth features will be disabled."
    #define BTSUPPORT 0
#endif

const bluetoothInfoStatic_s bluetooth_static = {
    .bluetooth_bdr = BTSUPPORT,
    .bluetooth_ble = BTSUPPORT
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

const rgbInfoStatic_s rgb_static = {
    .rgb_groups = RGB_GROUPS,
    .rgb_group_names = RGB_GROUP_NAMES,
    .rgb_player_group = PLAYER_GROUP
};

#define BLOCK_CHUNK_MAX 32
#define BLOCK_REPORT_ID_IDX 0 // We typically don't use this
#define BLOCK_TYPE_IDX 1
#define BLOCK_CHUNK_SIZE_IDX 2
#define BLOCK_CHUNK_PART_IDX 3
#define BLOCK_CHUNK_BEGIN_IDX 4

#define BLOCK_CHUNK_HEADER_SIZE 4

uint8_t _sdata[64] = {0};
void _serialize_block(static_block_t block, uint8_t *data , uint32_t size, setting_callback_t cb)
{
    memset(_sdata, 0, 64);
    _sdata[0] = WEBUSB_ID_READ_STATIC_BLOCK;
    _sdata[BLOCK_TYPE_IDX] = (uint8_t) block;

    if(size <= BLOCK_CHUNK_MAX)
    {
        _sdata[BLOCK_CHUNK_SIZE_IDX] = (uint8_t) size;
        _sdata[BLOCK_CHUNK_PART_IDX] = 0;
        memcpy(&(_sdata[BLOCK_CHUNK_BEGIN_IDX]), data, size);
        cb(_sdata, size+BLOCK_CHUNK_HEADER_SIZE);
    }
    else 
    {
        uint32_t remaining = size;
        uint8_t idx = 0;
        while(remaining>0 && idx<200)
        {
            uint32_t loop_size = remaining <= BLOCK_CHUNK_MAX ? remaining : BLOCK_CHUNK_MAX;
            _sdata[BLOCK_CHUNK_SIZE_IDX] = loop_size;
            _sdata[BLOCK_CHUNK_PART_IDX] = idx;
            memcpy(&(_sdata[BLOCK_CHUNK_BEGIN_IDX]), &(data[idx*BLOCK_CHUNK_MAX]), loop_size);
            cb(_sdata, loop_size+BLOCK_CHUNK_HEADER_SIZE);
            
            remaining -= loop_size;
            idx += 1;
        }
    }

    // Here we send the chunk completion byte
    _sdata[BLOCK_CHUNK_SIZE_IDX] = 0;
    _sdata[BLOCK_CHUNK_PART_IDX] = 0xFF;
    cb(_sdata, BLOCK_CHUNK_HEADER_SIZE);
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
            _serialize_block(block, &device_static, STATINFO_DEVICE_BLOCK_SIZE, cb);
        break;

        case STATIC_BLOCK_BUTTONS:
            _serialize_block(block, &buttons_static, STATINFO_DEVICE_BUTTON_SIZE, cb);
        break;

        case STATIC_BLOCK_ANALOG:
            _serialize_block(block, &analog_static, STATINFO_ANALOG_SIZE, cb);
        break;

        case STATIC_BLOCK_HAPTIC:
            _serialize_block(block, &haptic_static, STATINFO_HAPTIC_SIZE, cb);
        break;

        case STATIC_BLOCK_IMU:
            _serialize_block(block, &imu_static, STATINFO_IMU_SIZE, cb);
        break;

        case STATIC_BLOCK_BATTERY:
            _serialize_block(block, &battery_static, STATINFO_BATTERY_SIZE, cb);
        break;

        case STATIC_BLOCK_BLUETOOTH:
            _serialize_block(block, &bluetooth_static, STATINFO_BLUETOOTH_SIZE, cb);
        break;

        case STATIC_BLOCK_RGB:
            _serialize_block(block, &rgb_static, STATINFO_RGB_SIZE, cb);
        break;
    }
}
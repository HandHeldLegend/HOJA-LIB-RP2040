#include "utilities/static_config.h"
#include "hoja.h"
#include "board_config.h"

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

const device_static_u    device_static = {
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

const buttons_static_u   buttons_static = {
    .main_buttons   = HOJA_BUTTONS_SUPPORTED_MAIN,
    .system_buttons = HOJA_BUTTONS_SUPPORTED_SYSTEM
};

#if defined(HOJA_ADC_LX_DRIVER) 
 #define ALX (1)
#else 
 #define ALX (0)
#endif

#if defined(HOJA_ADC_LY_DRIVER) 
 #define ALY (1<<1)
#else 
 #define ALY (0)
#endif

#if defined(HOJA_ADC_RX_DRIVER) 
 #define ARX (1<<2)
#else 
 #define ARX (0)
#endif

#if defined(HOJA_ADC_RY_DRIVER) 
 #define ARY (1<<3)
#else 
 #define ARY (0)
#endif

#if defined(HOJA_ADC_LT_DRIVER) 
 #define ALT (1<<4)
#else 
 #define ALT (0)
#endif

#if defined(HOJA_ADC_RT_DRIVER) 
 #define ART (1<<5)
#else 
 #define ART (0)
#endif

#define ANALOG_SUPPORTED (ALX | ALY | ARX | ARY | ALT | ART)

const analog_static_u    analog_static = {
    .available_axes = ANALOG_SUPPORTED,
};

#if defined(HOJA_IMU_CHAN_A_DRIVER)
    #define IMU_AVAILABLE 0b1111
#else 
    #define IMU_AVAILABLE 0
#endif

const imu_static_u       imu_static = {
    .available_axes = IMU_AVAILABLE,
};

#if !defined(HOJA_BATTERY_CAPACITY_MAH)
 #warning "HOJA_BATTERY_CAPACITY_MAH undefined in board_config.h. Battery features will be disabled."
 #define HOJA_BATTERY_CAPACITY_MAH 0
#endif

#if !defined(HOJA_BATTERY_PART_CODE)
 #warning "HOJA_BATTERY_PART_CODE undefined in board_config.h."
 #define HOJA_BATTERY_PART_CODE "N/A"
#endif

const battery_static_u   battery_static = {
    .capacity_mah = HOJA_BATTERY_CAPACITY_MAH,
    .part_number  = HOJA_BATTERY_PART_CODE,
};

#if defined(HOJA_CONFIG_HDRUMBLE)
    #define HDRUMBLE 0b1 
#else 
    #define HDRUMBLE 0
#endif 

#if defined(HOJA_CONFIG_SDRUMBLE)
    #define SDRUMBLE 0b10 
#else 
    #define SDRUMBLE 0 
#endif 

#if !defined(HOJA_CONFIG_HDRUMBLE) && !defined(HOJA_CONFIG_SDRUMBLE)
 #warning "Neither HOJA_CONFIG_HDRUMBLE or HOJA_CONFIG_SDRUMBLE defined in board_config.h. Rumble features will be disabled."
#endif 

#define RUMBLE_SUPPORT (HDRUMBLE | SDRUMBLE)

const haptic_static_u    haptic_static = {
    .available_haptics = RUMBLE_SUPPORT
};

#if defined(HOJA_BLUETOOTH_DRIVER)
    #define BTSUPPORT 0b11 
#else 
    #warning "HOJA_BLUETOOTH_DRIVER undefined. Bluetooth features will be disabled."
    #define BTSUPPORT 0
#endif

const bluetooth_static_u bluetooth_static = {
    .available_bluetooth = BTSUPPORT
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

const rgb_static_u       rgb_static = {
    .rgb_groups = RGB_GROUPS,
    .rgb_group_names = RGB_GROUP_NAMES,
    .rgb_player_group = PLAYER_GROUP
};

void static_config_read_all_blocks(setting_callback_t cb)
{
    
}

void static_config_read_block(static_block_t block, setting_callback_t cb)
{

}
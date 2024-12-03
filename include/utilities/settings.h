#ifndef UTILITIES_SETTINGS_H
#define UTILITIES_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>
#include "devices_shared_types.h"
#include "settings_shared_types.h"

#include "devices/devices.h"

#include "input/remap.h"

#include "devices/haptics.h"



typedef union
{
    struct
    {
        int center : 31;
        bool invert : 1;
    };
    int value;
    
} analog_calibration_u;

typedef union 
{
    struct
    {
        bool     disabled : 1;
        uint16_t lower : 15;
        uint16_t upper : 16;
    };
    uint32_t value;
} trigger_calibration_u;

// We have a STRICT budget of 4096 bytes for configuration data
typedef struct 
{
    uint16_t settings_version; // 2 bytes

    // We can set our budgets for each configuration area
    // To allow future changes by only changing these 'blocks'

    // Gamepad configuration block
    uint8_t gamepad_configuration_block[GAMEPAD_CFB_SIZE]; 

    // Remap configuration block
    uint8_t remap_configuration_block[REMAP_CFB_SIZE]; 

    // RGB configuration block
    uint8_t rgb_configuration_block[RGB_CFB_SIZE]; 

    // Analog configuration block
    uint8_t analog_configuration_block[ANALOG_CFB_SIZE]; 

    // Trigger configuration block
    uint8_t trigger_configuration_block[TRIGGER_CFB_SIZE]; 

    // IMU configuration block
    uint8_t imu_configuration_block[IMU_CFB_SIZE]; 

    // Haptic configuration block
    uint8_t haptic_configuration_block[HAPTIC_CFB_SIZE]; 

    // REMAINING 2550 bytes
    uint8_t reserved[4096-TOTAL_CFB_SIZE]; 
} settings_data_s;

#endif

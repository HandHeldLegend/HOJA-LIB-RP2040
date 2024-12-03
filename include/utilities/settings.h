#ifndef UTILITIES_SETTINGS_H
#define UTILITIES_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>
#include "devices_shared_types.h"
#include "settings_shared_types.h"

#include "devices/devices.h"

#include "input/remap.h"

#include "devices/haptics.h"

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
} settings_flash_s;

// Duplicate struct, but this is designed to be mapped into RAM
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
} settings_live_s;

extern settings_live_s      live_settings;
extern gamepad_config_u     *gamepad_cfg;
extern remap_config_u       *remap_config;
extern rgb_config_u         *rgb_config;
extern analog_config_u      *analog_config;
extern trigger_config_u     *trigger_config;
extern imu_config_u         *imu_cfg;
extern haptic_config_u      *haptic_cfg;

void settings_init();
void settings_commit_blocks();
void settings_commit_task();

#endif

#ifndef UTILITIES_SETTINGS_H
#define UTILITIES_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>
#include "devices_shared_types.h"
#include "settings_shared_types.h"

#include "utilities/static_config.h"

#include "devices/devices.h"

#include "devices/haptics.h"

#pragma pack(push, 1)
// We have a STRICT budget of 4096 bytes for configuration data
typedef struct 
{
    // We can set our budgets for each configuration area
    // To allow future changes by only changing these 'blocks'

    // Gamepad configuration block
    uint8_t gamepad_configuration_block[GAMEPAD_CFB_SIZE]; 

    // Hover configuration block
    uint8_t hover_configuration_block[HOVER_CFB_SIZE]; 

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

    // User data configuration block
    uint8_t user_configuration_block[USER_CFB_SIZE];

    // Input data configuration block
    uint8_t input_configuration_block[INPUT_CFB_SIZE];

    // REMAINING bytes
    uint8_t reserved[4096-TOTAL_CFB_SIZE]; 
} settings_flash_s;

// Duplicate struct, but this is designed to be mapped into RAM
typedef struct 
{
    // We can set our budgets for each configuration area
    // To allow future changes by only changing these 'blocks'

    // Gamepad configuration block
    uint8_t gamepad_configuration_block[GAMEPAD_CFB_SIZE]; 

    // Hover configuration block
    uint8_t hover_configuration_block[HOVER_CFB_SIZE]; 

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

    // User data configuration block
    uint8_t user_configuration_block[USER_CFB_SIZE];

    // Input data configuration block
    uint8_t input_configuration_block[INPUT_CFB_SIZE];
} settings_live_s;
#pragma pack(pop)

extern settings_live_s      live_settings;
extern gamepadConfig_s     *gamepad_config;
extern hoverConfig_s       *hover_config;
extern rgbConfig_s         *rgb_config;
extern analogConfig_s      *analog_config;
extern triggerConfig_s     *trigger_config;
extern imuConfig_s         *imu_config;
extern hapticConfig_s      *haptic_config;
extern userConfig_s        *user_config;
extern inputConfig_s       *input_config;

void settings_init();
void settings_commit_blocks();

void settings_config_command(cfg_block_t block, uint8_t command);
void settings_return_static_block(static_block_t block, setting_callback_t cb);
void settings_return_config_block(cfg_block_t block, setting_callback_t cb);
void settings_write_config_block(cfg_block_t block, const uint8_t *data);

#endif

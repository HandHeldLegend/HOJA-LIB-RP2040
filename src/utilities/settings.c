#include "utilities/settings.h"
#include "hardware/flash.h"
#include "hoja.h"

#include "hal/mutex_hal.h"

#include <stdbool.h>

#define FLASH_TARGET_OFFSET (1200 * 1024)
#define SETTINGS_BANK_B_OFFSET (FLASH_TARGET_OFFSET)
#define SETTINGS_BANK_A_OFFSET (FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE)
#define SETTINGS_BANK_C_OFFSET (FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE + FLASH_SECTOR_SIZE)
#define BANK_A_NUM 0
#define BANK_B_NUM 1
#define BANK_C_NUM 2 // Bank for power or battery configuration


settings_live_s      live_settings = {0};
gamepad_config_u     *gamepad_cfg   = NULL;
remap_config_u       *remap_config  = NULL;
rgb_config_u         *rgb_config    = NULL;
analog_config_u      *analog_config = NULL;
trigger_config_u     *trigger_config= NULL;
imu_config_u         *imu_cfg       = NULL;
haptic_config_u      *haptic_cfg    = NULL;

void settings_init()
{
    gamepad_cfg     = (gamepad_config_u *)  live_settings.gamepad_configuration_block;
    remap_config    = (remap_config_u *)    live_settings.remap_configuration_block;
    rgb_config      = (rgb_config_u *)      live_settings.rgb_configuration_block;
    analog_config   = (analog_config_u *)   live_settings.analog_configuration_block;
    trigger_config  = (trigger_config_u *)  live_settings.trigger_configuration_block;
    imu_cfg         = (imu_config_u *)      live_settings.imu_configuration_block;
    haptic_cfg      = (haptic_config_u *)   live_settings.haptic_configuration_block;
}

MUTEX_HAL_INIT(_settings_mutex);
volatile bool _save_go = false;

void settings_commit_blocks()
{
    // Check which core we're on
    // Get mutex
    MUTEX_HAL_ENTER_BLOCKING(&_settings_mutex);
    _save_go = true;
    
    MUTEX_HAL_EXIT(&_settings_mutex);
}

void settings_commit_task()
{
    if(_save_go)
    {
        // Checks if we must commit our save settings
        MUTEX_HAL_ENTER_BLOCKING(&_settings_mutex);

        MUTEX_HAL_EXIT(&_settings_mutex);
        _save_go = false;
    }
    
}

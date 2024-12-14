#include "utilities/settings.h"
#include "hardware/flash.h"
#include "hoja.h"

#include "hal/mutex_hal.h"
#include "hal/sys_hal.h"
#include "usb/webusb.h"

#include <stdbool.h>
#include <stdbool.h>
#include <string.h>

#include "utilities/static_config.h"

#include "input/stick_scaling.h"

#define FLASH_TARGET_OFFSET (1200 * 1024)
#define SETTINGS_BANK_B_OFFSET (FLASH_TARGET_OFFSET)
#define SETTINGS_BANK_A_OFFSET (FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE)
#define SETTINGS_BANK_C_OFFSET (FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE + FLASH_SECTOR_SIZE)
#define BANK_A_NUM 0
#define BANK_B_NUM 1
#define BANK_C_NUM 2 // Bank for power or battery configuration

settings_live_s     live_settings = {0};
gamepadConfig_s     *gamepad_config    = NULL;
remapConfig_s       *remap_config      = NULL;
rgbConfig_s         *rgb_config        = NULL;
analogConfig_s      *analog_config     = NULL;
triggerConfig_s     *trigger_config    = NULL;
imuConfig_s         *imu_config        = NULL;
hapticConfig_s      *haptic_config     = NULL;
userConfig_s        *user_config       = NULL;
batteryConfig_s     *battery_config    = NULL;

void settings_init()
{
    gamepad_config     = (gamepadConfig_s *)  live_settings.gamepad_configuration_block;
    remap_config       = (remapConfig_s *)    live_settings.remap_configuration_block;
    rgb_config         = (rgbConfig_s *)      live_settings.rgb_configuration_block;
    analog_config      = (analogConfig_s *)   live_settings.analog_configuration_block;
    trigger_config     = (triggerConfig_s *)  live_settings.trigger_configuration_block;
    imu_config         = (imuConfig_s *)      live_settings.imu_configuration_block;
    haptic_config      = (hapticConfig_s *)   live_settings.haptic_configuration_block;
    user_config        = (userConfig_s *)     live_settings.user_configuration_block;
    battery_config     = (batteryConfig_s *)  live_settings.battery_configuration_block;

    // Debug mac address if zero
    if(!gamepad_config->switch_mac_address[0])
    {
        for(uint8_t i = 0; i < 6; i++)
        {
            uint8_t rand = sys_hal_random() & 0xFF;
            if(!rand) rand++;
            gamepad_config->switch_mac_address[i] = rand;
        }
    }
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

// Input only commands that don't involve writing data
void settings_config_command(cfg_block_t block, uint8_t command)
{
    switch(block)
    {
        default:
        return;

        case CFG_BLOCK_GAMEPAD:
            
        break;

        case CFG_BLOCK_REMAP:
            
        break;

        case CFG_BLOCK_ANALOG:
            analog_config_command(command);
        break;

        case CFG_BLOCK_RGB:
            
        break;

        case CFG_BLOCK_TRIGGER:
            
        break;

        case CFG_BLOCK_IMU:
            
        break;

        case CFG_BLOCK_HAPTIC:
            
        break;

        case CFG_BLOCK_USER:
            
        break;
    }
}

void settings_write_config_block(cfg_block_t block, const uint8_t *data)
{
    static uint8_t      tmp_dat[1024]   = {0};
    static cfg_block_t  last_block_type = 0;

    if(last_block_type != block)
    {
        // Clear tmp block
        memset(tmp_dat, 0, 1024);
        last_block_type = block;
    }

    // Commit data index value
    if(data[0] == 0xFF)
    {
        switch(block)
        {
            default:
            return;

            case CFG_BLOCK_GAMEPAD:
                memcpy(gamepad_config, tmp_dat, GAMEPAD_CFB_SIZE);
            break;

            case CFG_BLOCK_REMAP:
                memcpy(remap_config, tmp_dat, REMAP_CFB_SIZE);
            break;

            case CFG_BLOCK_ANALOG:
                memcpy(analog_config, tmp_dat, ANALOG_CFB_SIZE);
                // Reinit analog sections as needed 
                stick_scaling_init();
            break;

            case CFG_BLOCK_RGB:
                memcpy(rgb_config, tmp_dat, RGB_CFB_SIZE);
            break;

            case CFG_BLOCK_TRIGGER:
                memcpy(trigger_config, tmp_dat, TRIGGER_CFB_SIZE);
            break;

            case CFG_BLOCK_IMU:
                memcpy(imu_config, tmp_dat, IMU_CFB_SIZE);
            break;

            case CFG_BLOCK_HAPTIC:
                memcpy(haptic_config, tmp_dat, HAPTIC_CFB_SIZE);
            break;

            case CFG_BLOCK_USER:
                memcpy(user_config, tmp_dat, USER_CFB_SIZE);
            break;
        }
    }
    else 
    {
        uint32_t write_idx = data[0] * 32;
        if(write_idx<=991)
        {
            memcpy(&tmp_dat[write_idx], &data[1], 32);
        }
    }

    
}

#define BLOCK_TYPE_IDX 1
#define BLOCK_CHUNK_SIZE_IDX 2
#define BLOCK_CHUNK_PART_IDX 3
#define BLOCK_CHUNK_BEGIN_IDX 4

#define BLOCK_CHUNK_HEADER_SIZE 4

uint8_t _sdata[64] = {0};
void settings_return_config_block(cfg_block_t block, setting_callback_t cb)
{
    _sdata[0] = WEBUSB_ID_READ_CONFIG_BLOCK;
    _sdata[BLOCK_TYPE_IDX] = (uint8_t) block;
    
    switch(block)
    {
        default:
        break;

        case CFG_BLOCK_GAMEPAD:
            /* 
                Each chunk we send is 32 byte limited
                We have byte 0 which is our report ID
                byte 1 is the block type
                byte 2 is the data size we are sending
                byte 3 is our assembly index...
                a Byte 1 value of 255 means it's the end of a chunk
            */
            _sdata[BLOCK_CHUNK_SIZE_IDX] = 32; // Size of bytes being written to host
            _sdata[BLOCK_CHUNK_PART_IDX] = 0;
            memcpy(&_sdata[BLOCK_CHUNK_BEGIN_IDX], &live_settings.gamepad_configuration_block, GAMEPAD_CFB_SIZE);
            cb(_sdata, 32+BLOCK_CHUNK_HEADER_SIZE);
        break;

        case CFG_BLOCK_REMAP:
            _sdata[BLOCK_CHUNK_SIZE_IDX] = 32; 
            _sdata[BLOCK_CHUNK_PART_IDX] = 0; 
            memcpy(&_sdata[BLOCK_CHUNK_BEGIN_IDX], &live_settings.remap_configuration_block, REMAP_CFB_SIZE);
            cb(_sdata, 32+BLOCK_CHUNK_HEADER_SIZE);
        break;

        case CFG_BLOCK_ANALOG:
            _sdata[BLOCK_CHUNK_SIZE_IDX] = 32; 
            for(int i = 0; i < (ANALOG_CFB_SIZE/32); i++)
            {
                _sdata[BLOCK_CHUNK_PART_IDX] = i;
                memcpy(&_sdata[BLOCK_CHUNK_BEGIN_IDX], &live_settings.analog_configuration_block[i*32], 32);
                cb(_sdata, 32+BLOCK_CHUNK_HEADER_SIZE);
            }
        break;

        case CFG_BLOCK_RGB:
            _sdata[BLOCK_CHUNK_SIZE_IDX] = 32;
            for(int i = 0; i < (RGB_CFB_SIZE/32); i++)
            {
                _sdata[BLOCK_CHUNK_PART_IDX] = i;
                memcpy(&_sdata[BLOCK_CHUNK_BEGIN_IDX], &live_settings.rgb_configuration_block[i*32], 32);
                cb(_sdata, 32+BLOCK_CHUNK_HEADER_SIZE);
            }
        break;

        case CFG_BLOCK_TRIGGER:
            _sdata[BLOCK_CHUNK_SIZE_IDX] = 32;
            _sdata[BLOCK_CHUNK_PART_IDX] = 0;
            memcpy(&_sdata[BLOCK_CHUNK_BEGIN_IDX], &live_settings.trigger_configuration_block, TRIGGER_CFB_SIZE);
            cb(_sdata, 32+BLOCK_CHUNK_HEADER_SIZE);
        break;

        case CFG_BLOCK_IMU:
            _sdata[BLOCK_CHUNK_SIZE_IDX] = 32;
            _sdata[BLOCK_CHUNK_PART_IDX] = 0;
            memcpy(&_sdata[BLOCK_CHUNK_BEGIN_IDX], &live_settings.imu_configuration_block, IMU_CFB_SIZE);
            cb(_sdata, 32+BLOCK_CHUNK_HEADER_SIZE);
        break;

        case CFG_BLOCK_HAPTIC:
            _sdata[BLOCK_CHUNK_SIZE_IDX] = 8;
            _sdata[BLOCK_CHUNK_PART_IDX] = 0;
            memcpy(&_sdata[BLOCK_CHUNK_BEGIN_IDX], &live_settings.haptic_configuration_block, HAPTIC_CFB_SIZE);
            cb(_sdata, HAPTIC_CFB_SIZE+BLOCK_CHUNK_HEADER_SIZE);
        break;

        case CFG_BLOCK_USER:
            _sdata[BLOCK_CHUNK_SIZE_IDX] = 32;
            for(int i = 0; i < (USER_CFB_SIZE/32); i++)
            {
                _sdata[BLOCK_CHUNK_PART_IDX] = i;
                memcpy(&_sdata[BLOCK_CHUNK_BEGIN_IDX], &live_settings.user_configuration_block[i*32], 32);
                cb(_sdata, 32+BLOCK_CHUNK_HEADER_SIZE);
            }
        break;
    }

    // Here we send the chunk completion byte
    _sdata[BLOCK_CHUNK_SIZE_IDX] = 0;
    _sdata[BLOCK_CHUNK_PART_IDX] = 0xFF;
    cb(_sdata, BLOCK_CHUNK_HEADER_SIZE);
}

void settings_return_static_block(static_block_t block, setting_callback_t cb)
{
    switch(block)
    {
        default:
        break;

        case STATIC_BLOCK_DEVICE:
        break;

        case STATIC_BLOCK_BUTTONS:
        break;

        case STATIC_BLOCK_ANALOG:
        break;

        case STATIC_BLOCK_HAPTIC:
        break;

        case STATIC_BLOCK_IMU:
        break;

        case STATIC_BLOCK_BATTERY:
        break;

        case STATIC_BLOCK_BLUETOOTH:
        break;

        case STATIC_BLOCK_RGB:
        break;
    }
}

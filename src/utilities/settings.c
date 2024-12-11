#include "utilities/settings.h"
#include "hardware/flash.h"
#include "hoja.h"

#include "hal/mutex_hal.h"
#include "usb/webusb.h"

#include <stdbool.h>
#include <stdbool.h>
#include <string.h>

#define FLASH_TARGET_OFFSET (1200 * 1024)
#define SETTINGS_BANK_B_OFFSET (FLASH_TARGET_OFFSET)
#define SETTINGS_BANK_A_OFFSET (FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE)
#define SETTINGS_BANK_C_OFFSET (FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE + FLASH_SECTOR_SIZE)
#define BANK_A_NUM 0
#define BANK_B_NUM 1
#define BANK_C_NUM 2 // Bank for power or battery configuration


settings_live_s      live_settings = {0};
gamepad_config_u     *gamepad_config    = NULL;
remap_config_u       *remap_config      = NULL;
rgb_config_u         *rgb_config        = NULL;
analog_config_u      *analog_config     = NULL;
trigger_config_u     *trigger_config    = NULL;
imu_config_u         *imu_config        = NULL;
haptic_config_u      *haptic_config     = NULL;
user_config_u        *user_config          = NULL;

void settings_init()
{
    gamepad_config     = (gamepad_config_u *)  live_settings.gamepad_configuration_block;
    remap_config       = (remap_config_u *)    live_settings.remap_configuration_block;
    rgb_config         = (rgb_config_u *)      live_settings.rgb_configuration_block;
    analog_config      = (analog_config_u *)   live_settings.analog_configuration_block;
    trigger_config     = (trigger_config_u *)  live_settings.trigger_configuration_block;
    imu_config         = (imu_config_u *)      live_settings.imu_configuration_block;
    haptic_config      = (haptic_config_u *)   live_settings.haptic_configuration_block;
    user_config        = (user_config_u *)     live_settings.user_configuration_block;
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

void settings_write_config_block(cfg_block_t block, const uint8_t *data)
{
    static uint8_t tmp_dat[1024] = {0};
    static cfg_block_t last_block_type = 0;

    if(last_block_type != block)
    {
        // Clear tmp block
        memset(tmp_dat, 0, 1024);
        last_block_type = block;
    }

    // Commit data command
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

            case CFG_BLOCK_RGB:
                memcpy(rgb_config, tmp_dat, RGB_CFB_SIZE);
            break;

            case CFG_BLOCK_ANALOG:
                memcpy(analog_config, tmp_dat, ANALOG_CFB_SIZE);
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

void settings_return_config_block(cfg_block_t block)
{
    uint8_t data[64] = {0};

    data[0] = (uint8_t) block;
    
    switch(block)
    {
        default:
            return;
        break;

        case CFG_BLOCK_GAMEPAD:
            // Each chunk we send is 32 byte limited
            // We have byte 0 represent the type of setting chunk
            // and byte 1 represents the index for reassembly
            // a Byte 1 value of 255 means it's the end of a chunk
            data[1] = 0;
            memcpy(&data[2], &live_settings.gamepad_configuration_block, GAMEPAD_CFB_SIZE);
            webusb_send_bulk(data, 34);
        break;

        case CFG_BLOCK_REMAP:
            data[1] = 0;
            memcpy(&data[2], &live_settings.remap_configuration_block, REMAP_CFB_SIZE);
            webusb_send_bulk(data, 34);
        break;

        case CFG_BLOCK_ANALOG:
            for(int i = 0; i < (ANALOG_CFB_SIZE/32); i++)
            {
                data[1] = i;
                memcpy(&data[2], &live_settings.analog_configuration_block[i*32], 32);
                webusb_send_bulk(data, 34);
            }
        break;

        case CFG_BLOCK_RGB:
            for(int i = 0; i < (RGB_CFB_SIZE/32); i++)
            {
                data[1] = i;
                memcpy(&data[2], &live_settings.rgb_configuration_block[i*32], 32);
                webusb_send_bulk(data, 34);
            }
        break;

        case CFG_BLOCK_TRIGGER:
            data[1] = 0;
            memcpy(&data[2], &live_settings.trigger_configuration_block, TRIGGER_CFB_SIZE);
            webusb_send_bulk(data, 34);
        break;

        case CFG_BLOCK_IMU:
            data[1] = 0;
            memcpy(&data[2], &live_settings.imu_configuration_block, IMU_CFB_SIZE);
            webusb_send_bulk(data, 34);
        break;

        case CFG_BLOCK_HAPTIC:
            data[1] = 0;
            memcpy(&data[2], &live_settings.haptic_configuration_block, HAPTIC_CFB_SIZE);
            webusb_send_bulk(data, HAPTIC_CFB_SIZE+2);
        break;

        case CFG_BLOCK_USER:
            for(int i = 0; i < (USER_CFB_SIZE/32); i++)
            {
                data[1] = i;
                memcpy(&data[2], &live_settings.user_configuration_block[i*32], 32);
                webusb_send_bulk(data, 34);
            }
        break;
    }

    // Here we send the chunk completion byte
    data[1] = 255;
    webusb_send_bulk(data, 2);
}

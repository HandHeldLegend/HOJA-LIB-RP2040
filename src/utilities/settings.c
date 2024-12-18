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
#include "input/imu.h"

#include "devices/rgb.h"

#define BLOCK_CHUNK_MAX 32
#define BLOCK_REPORT_ID_IDX 0 // We typically don't use this
#define BLOCK_TYPE_IDX 1
#define BLOCK_CHUNK_SIZE_IDX 2
#define BLOCK_CHUNK_PART_IDX 3
#define BLOCK_CHUNK_BEGIN_IDX 4

#define BLOCK_CHUNK_HEADER_SIZE 4

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
    gamepad_config     = (gamepadConfig_s *)  &live_settings.gamepad_configuration_block;
    remap_config       = (remapConfig_s *)    &live_settings.remap_configuration_block;
    rgb_config         = (rgbConfig_s *)      &live_settings.rgb_configuration_block;
    analog_config      = (analogConfig_s *)   &live_settings.analog_configuration_block;
    trigger_config     = (triggerConfig_s *)  &live_settings.trigger_configuration_block;
    imu_config         = (imuConfig_s *)      &live_settings.imu_configuration_block;
    haptic_config      = (hapticConfig_s *)   &live_settings.haptic_configuration_block;
    user_config        = (userConfig_s *)     &live_settings.user_configuration_block;
    battery_config     = (batteryConfig_s *)  &live_settings.battery_configuration_block;

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
            analog_config_command(command, webusb_command_confirm_cb);
        break;

        case CFG_BLOCK_RGB:
            
        break;

        case CFG_BLOCK_TRIGGER:
            
        break;

        case CFG_BLOCK_IMU:
            imu_config_cmd(command, webusb_command_confirm_cb);
        break;

        case CFG_BLOCK_HAPTIC:
            
        break;

        case CFG_BLOCK_USER:
            
        break;

        case CFG_BLOCK_BATTERY:

        break;
    }
}

void settings_write_config_block(cfg_block_t block, const uint8_t *data)
{
    uint8_t write_size = data[BLOCK_CHUNK_SIZE_IDX];
    uint8_t write_idx = data[BLOCK_CHUNK_PART_IDX];

    bool completed = false;
    bool write = false;
    
    if(write_idx==0xFF)
        completed = true;

    if(write_size>0)
        write = true;

    uint8_t *write_to_ptr = NULL;

    switch(block)
    {
        default:
        return;

        case CFG_BLOCK_GAMEPAD:
            write_to_ptr = live_settings.gamepad_configuration_block;
        break;

        case CFG_BLOCK_REMAP:
            write_to_ptr = live_settings.remap_configuration_block;
        break;

        case CFG_BLOCK_ANALOG:
            write_to_ptr = live_settings.analog_configuration_block;
            // Reinit analog sections as needed 
            if(completed)
                stick_scaling_init();
        break;

        case CFG_BLOCK_RGB:
            write_to_ptr = live_settings.rgb_configuration_block;
            if(completed)
                rgb_init(-1, -1);
        break;

        case CFG_BLOCK_TRIGGER:
            write_to_ptr = live_settings.trigger_configuration_block;
        break;

        case CFG_BLOCK_IMU:
            write_to_ptr = live_settings.imu_configuration_block;
        break;

        case CFG_BLOCK_HAPTIC:
            write_to_ptr = live_settings.haptic_configuration_block;
        break;

        case CFG_BLOCK_USER:
            write_to_ptr = live_settings.user_configuration_block;
        break;

        case CFG_BLOCK_BATTERY:
            write_to_ptr = live_settings.battery_configuration_block;
        break;
    }

    if(write) memcpy(&(write_to_ptr[BLOCK_CHUNK_MAX*write_idx]), &(data[BLOCK_CHUNK_BEGIN_IDX]), write_size);

    // Send confirmation data 
    //uint8_t confirm_data[3] = {WEBUSB_ID_WRITE_CONFIG_BLOCK, (uint8_t) block, write_idx};
    //webusb_send_bulk(confirm_data, 3);
}

uint8_t _sdata[64] = {0};
void _serialize_block(cfg_block_t block, uint8_t *data , uint32_t size, setting_callback_t cb)
{
    memset(_sdata, 0, 64);
    _sdata[0] = WEBUSB_ID_READ_CONFIG_BLOCK;
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

void settings_return_config_block(cfg_block_t block, setting_callback_t cb)
{    
    switch(block)
    {
        default:
        break;

        case CFG_BLOCK_GAMEPAD:
            _serialize_block(block, live_settings.gamepad_configuration_block, GAMEPAD_CFB_SIZE, cb);
        break;

        case CFG_BLOCK_REMAP:
            _serialize_block(block, live_settings.remap_configuration_block, REMAP_CFB_SIZE, cb);
        break;

        case CFG_BLOCK_ANALOG:
            _serialize_block(block, live_settings.analog_configuration_block, ANALOG_CFB_SIZE, cb);
        break;

        case CFG_BLOCK_RGB:
            _serialize_block(block, live_settings.rgb_configuration_block, RGB_CFB_SIZE, cb);
        break;

        case CFG_BLOCK_TRIGGER:
            _serialize_block(block, live_settings.trigger_configuration_block, TRIGGER_CFB_SIZE, cb);
        break;

        case CFG_BLOCK_IMU:
            _serialize_block(block, live_settings.imu_configuration_block, IMU_CFB_SIZE, cb);
        break;

        case CFG_BLOCK_HAPTIC:
            _serialize_block(block, live_settings.haptic_configuration_block, HAPTIC_CFB_SIZE, cb);
        break;

        case CFG_BLOCK_USER:
            _serialize_block(block, live_settings.user_configuration_block, USER_CFB_SIZE, cb);
        break;

        case CFG_BLOCK_BATTERY:
            _serialize_block(block, live_settings.battery_configuration_block, BATTERY_CFB_SIZE, cb);
        break;
    }
}

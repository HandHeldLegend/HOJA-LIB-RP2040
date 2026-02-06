#include "utilities/settings.h"
#include "hardware/flash.h"
#include "hoja.h"

#include "hal/mutex_hal.h"
#include "hal/sys_hal.h"
#include "usb/webusb.h"

#include <stdbool.h>
#include <stdbool.h>
#include <string.h>

#include "devices/bluetooth.h"

#include "utilities/static_config.h"
#include "hal/flash_hal.h"

#include "input/stick_scaling.h"
#include "input/imu.h"
#include "input/mapper.h"
#include "input/hover.h"
#include "devices/haptics.h"

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
hoverConfig_s       *hover_config      = NULL;
rgbConfig_s         *rgb_config        = NULL;
analogConfig_s      *analog_config     = NULL;
triggerConfig_s     *trigger_config    = NULL;
imuConfig_s         *imu_config        = NULL;
hapticConfig_s      *haptic_config     = NULL;
userConfig_s        *user_config       = NULL;
inputConfig_s       *input_config      = NULL;

void settings_init()
{
    flash_hal_read((uint8_t *) &live_settings, sizeof(settings_live_s), 0);

    gamepad_config     = (gamepadConfig_s *)  &live_settings.gamepad_configuration_block;
    hover_config       = (hoverConfig_s *)    &live_settings.hover_configuration_block;
    rgb_config         = (rgbConfig_s *)      &live_settings.rgb_configuration_block;
    analog_config      = (analogConfig_s *)   &live_settings.analog_configuration_block;
    trigger_config     = (triggerConfig_s *)  &live_settings.trigger_configuration_block;
    imu_config         = (imuConfig_s *)      &live_settings.imu_configuration_block;
    haptic_config      = (hapticConfig_s *)   &live_settings.haptic_configuration_block;
    user_config        = (userConfig_s *)     &live_settings.user_configuration_block;
    input_config       = (inputConfig_s *)    &live_settings.input_configuration_block;

    // Debug mac address if zero
    if(gamepad_config->gamepad_config_version != CFG_BLOCK_GAMEPAD_VERSION)
    {
        gamepad_config->gamepad_config_version = CFG_BLOCK_GAMEPAD_VERSION;
        gamepad_config->gamepad_default_mode   = GAMEPAD_MODE_SWPRO;

        for(uint8_t i = 0; i < 6; i++)
        {
            uint8_t rand = sys_hal_random() & 0xFF;
            if(!rand) rand++;
            gamepad_config->switch_mac_address[i] = rand;
        }

        // Always unset the first bit in octet 0
        if((gamepad_config->switch_mac_address[0] & 0x01) != 0)
        {
            gamepad_config->switch_mac_address[0]-=1;
        }

        gamepad_config->gamepad_color_body = 0x2D2D2D;
        gamepad_config->gamepad_color_buttons = 0xE6E6E6;
        gamepad_config->gamepad_color_grip_left = 0xFFFFFF;
        gamepad_config->gamepad_color_grip_right = 0xFFFFFF;

        // Commit changes
        settings_commit_blocks();
    }

    if(gamepad_config->webusb_enable_popup==0xFF) gamepad_config->webusb_enable_popup = 0;

    // Always unset the first bit in octet 0
    if((gamepad_config->switch_mac_address[0] & 0x01) != 0)
    {
        gamepad_config->switch_mac_address[0]-=1;
        settings_commit_blocks();
    }

    // Init User config
    if(user_config->user_config_version != CFG_BLOCK_USER_VERSION)
    {
        user_config->user_config_version = CFG_BLOCK_USER_VERSION;
        memset(user_config->user_name, 0, sizeof(user_config->user_name));
    }
}

MUTEX_HAL_INIT(_settings_mutex);


void settings_commit_blocks()
{
    // Check which core we're on
    // Get mutex
    MUTEX_HAL_ENTER_BLOCKING(&_settings_mutex);
    
    flash_hal_write((uint8_t *) &live_settings, sizeof(settings_live_s), 0);
    
    MUTEX_HAL_EXIT(&_settings_mutex);
}

void _gamepad_config_command(uint8_t command, webreport_cmd_confirm_t cb)
{
    // CFG_BLOCK_GAMEPAD
    switch(command)
    {
        case GAMEPAD_CMD_RESET_TO_BOOTLOADER:
            sys_hal_bootloader();
        break;

        case GAMEPAD_CMD_ENABLE_BLUETOOTH_UPLOAD:
            // Enable bluetooth upload
            //bluetooth_mode_start(GAMEPAD_MODE_LOAD, false);
        break;

        case GAMEPAD_CMD_SAVE_ALL:
            settings_commit_blocks();
            cb(CFG_BLOCK_GAMEPAD, command, true, NULL, 0);
            
        break;
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
            _gamepad_config_command(command, webusb_command_confirm_cb);
        break;

        case CFG_BLOCK_HOVER:
            hover_config_command(command, webusb_command_confirm_cb);
        break;

        case CFG_BLOCK_ANALOG:
            analog_config_command(command, webusb_command_confirm_cb);
        break;

        case CFG_BLOCK_RGB:
            // NULL
        break;

        case CFG_BLOCK_IMU:
            imu_config_cmd(command, webusb_command_confirm_cb);
        break;

        case CFG_BLOCK_HAPTIC:
            haptic_config_cmd(command, webusb_command_confirm_cb);
        break;

        case CFG_BLOCK_USER:
            
        break;

        case CFG_BLOCK_INPUT:
            mapper_config_command(command, webusb_command_confirm_cb);
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

        case CFG_BLOCK_HOVER:
            write_to_ptr = live_settings.hover_configuration_block;

            if(completed)
                hover_init();
        break;

        case CFG_BLOCK_ANALOG:
            write_to_ptr = live_settings.analog_configuration_block;
            // Reinit analog sections as needed 
            if(completed)
                analog_init();
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

            if(completed)
                haptics_init();
        break;

        case CFG_BLOCK_USER:
            write_to_ptr = live_settings.user_configuration_block;
        break;

        case CFG_BLOCK_INPUT:
            write_to_ptr = live_settings.input_configuration_block;
            if(completed)
                mapper_init();
        break;
    }

    if(write) memcpy(&(write_to_ptr[BLOCK_CHUNK_MAX*write_idx]), &(data[BLOCK_CHUNK_BEGIN_IDX]), write_size);
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

        case CFG_BLOCK_HOVER:
            _serialize_block(block, live_settings.hover_configuration_block, HOVER_CFB_SIZE, cb);
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

        case CFG_BLOCK_INPUT:
            _serialize_block(block, live_settings.input_configuration_block, INPUT_CFB_SIZE, cb);
        break;
    }
}

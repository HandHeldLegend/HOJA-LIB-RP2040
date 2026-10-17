#include "utilities/settings.h"
#include "utilities/autodetect.h"
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
#include "utilities/boot.h"
#include "hal/flash_hal.h"

#include "input/stick_scaling.h"
#include "input/imu.h"
#include "input/mapper.h"
#include "input/hover.h"
#include "devices/haptics.h"

#include "devices/rgb.h"
#include "devices/battery.h"
#include "devices/animations/anm_authentic.h"

#define BLOCK_CHUNK_MAX 32
#define BLOCK_REPORT_ID_IDX 0 // We typically don't use this
#define BLOCK_TYPE_IDX 1
#define BLOCK_CHUNK_SIZE_IDX 2
#define BLOCK_CHUNK_PART_IDX 3
#define BLOCK_CHUNK_BEGIN_IDX 4

#define BLOCK_CHUNK_HEADER_SIZE 4

// GAMEPAD_CMD_REBOOT_* argument: 0xFF for none, else [mode given 0x80][pair 0x40][mode, 6 bits]
#define REBOOT_ARG_NONE      0xFFu
#define REBOOT_ARG_MODE_SET  0x80u
#define REBOOT_ARG_PAIR      0x40u
#define REBOOT_ARG_MODE_MASK 0x3Fu

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
switchpairConfig_s  *switchpair_config = NULL;

static void _settings_split_default_mode(void);

void settings_init()
{
    flash_hal_read((uint8_t *) &live_settings, sizeof(settings_live_s), 0);

    gamepad_config     = (gamepadConfig_s *)    &live_settings.gamepad_configuration_block;
    hover_config       = (hoverConfig_s *)      &live_settings.hover_configuration_block;
    rgb_config         = (rgbConfig_s *)        &live_settings.rgb_configuration_block;
    analog_config      = (analogConfig_s *)     &live_settings.analog_configuration_block;
    trigger_config     = (triggerConfig_s *)    &live_settings.trigger_configuration_block;
    imu_config         = (imuConfig_s *)        &live_settings.imu_configuration_block;
    haptic_config      = (hapticConfig_s *)     &live_settings.haptic_configuration_block;
    user_config        = (userConfig_s *)       &live_settings.user_configuration_block;
    input_config       = (inputConfig_s *)      &live_settings.input_configuration_block;
    switchpair_config  = (switchpairConfig_s *) &live_settings.switchpair_configuration_block;

    // Debug mac address if zero
    if(gamepad_config->gamepad_config_version != CFG_BLOCK_GAMEPAD_VERSION)
    {
        gamepad_config->gamepad_config_version   = CFG_BLOCK_GAMEPAD_VERSION;
        gamepad_config->gamepad_default_mode     = GAMEPAD_DEFAULT_MODE_AUTO;
        gamepad_config->gamepad_default_wireless = GAMEPAD_DEFAULT_MODE_AUTO;
        gamepad_config->gamepad_defaults_split   = GAMEPAD_DEFAULTS_SPLIT;

        for(uint8_t i = 0; i < 6; i++)
        {
            uint8_t rand = sys_hal_random() & 0xFF;
            if(!rand) rand++;
            gamepad_config->gamepad_mac_address[i] = rand;
        }

        // Always unset the first bit in octet 0
        if((gamepad_config->gamepad_mac_address[0] & 0x01) != 0)
        {
            gamepad_config->gamepad_mac_address[0]-=1;
        }

        gamepad_config->gamepad_color_body = 0x2D2D2D;
        gamepad_config->gamepad_color_buttons = 0xE6E6E6;
        gamepad_config->gamepad_color_grip_left = 0xFFFFFF;
        gamepad_config->gamepad_color_grip_right = 0xFFFFFF;

        // Unset paired device addresses and Switch link key
        memset(gamepad_config->host_mac_switch, 0, 6);
        memset(gamepad_config->host_mac_sinput, 0, 6);

        // Reset WebUSB popup Setting
        gamepad_config->webusb_enable_popup = 0;

        // Commit changes
        settings_commit_blocks();
    }

    // Always unset the first bit in octet 0
    if((gamepad_config->gamepad_mac_address[0] & 0x01) != 0)
    {
        gamepad_config->gamepad_mac_address[0]-=1;
        settings_commit_blocks();
    }

    // Once, after updating: split the one default mode into wired and wireless. Older firmware
    // ignores these bytes, so going back keeps everything.
    if(gamepad_config->gamepad_defaults_split != GAMEPAD_DEFAULTS_SPLIT)
    {
        _settings_split_default_mode();
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

// The factory default (Switch Pro) becomes Auto for both. A mode someone picked keeps doing what it
// did: wired as before, and on battery the same mode where it has a wireless version.
static void _settings_split_default_mode(void)
{
    const uint8_t old = gamepad_config->gamepad_default_mode;

    uint8_t wired = old;
    uint8_t wireless = GAMEPAD_DEFAULT_MODE_AUTO;
    switch(old)
    {
        case CORE_REPORTFORMAT_SINPUT:
            wireless = old;
            break;

        case CORE_REPORTFORMAT_WII:
            wired = GAMEPAD_DEFAULT_MODE_AUTO;
            wireless = old;
            break;

        case CORE_REPORTFORMAT_GAMECUBE:
        case CORE_REPORTFORMAT_N64:
        case CORE_REPORTFORMAT_SNES:
        case CORE_REPORTFORMAT_XINPUT:
        case CORE_REPORTFORMAT_SLIPPI:
            break;

        default: // Switch Pro, Auto, or anything unknown
            wired = GAMEPAD_DEFAULT_MODE_AUTO;
            break;
    }

    gamepad_config->gamepad_default_mode     = wired;
    gamepad_config->gamepad_default_wireless = wireless;
    gamepad_config->gamepad_defaults_split   = GAMEPAD_DEFAULTS_SPLIT;
}


void settings_commit_blocks()
{
    // Check which core we're on
    // Get mutex
    MUTEX_HAL_ENTER_BLOCKING(&_settings_mutex);
    
    flash_hal_write((uint8_t *) &live_settings, sizeof(settings_live_s), 0);
    
    MUTEX_HAL_EXIT(&_settings_mutex);
}

void _gamepad_config_command(uint8_t command, uint8_t arg, webreport_cmd_confirm_t cb)
{
    // CFG_BLOCK_GAMEPAD
    switch(command)
    {
        case GAMEPAD_CMD_RESET_TO_BOOTLOADER:
        {
            // Reached without USB (the dongle's tunnel, or BLE), the bootloader is out of reach unless
            // a cable is in
            battery_status_s power = {0};
            battery_get_status(&power);
            if (webusb_get_sink() && !power.plugged)
            {
                cb(CFG_BLOCK_GAMEPAD, command, false, NULL, 0);
                break;
            }
            sys_hal_bootloader();
        }
        break;

        // Come back up over another transport (boot memory), whatever powers us: in the mode given
        // (kept, as a boot combo's would be), else in the current one; pairing, as the pairing boot
        // combo does, when asked
        case GAMEPAD_CMD_REBOOT_DONGLE:
        case GAMEPAD_CMD_REBOOT_BLUETOOTH:
        {
            const bool given = (arg != REBOOT_ARG_NONE);
            const uint8_t mode = (uint8_t)(arg & REBOOT_ARG_MODE_MASK);
            const bool chosen = given && (arg & REBOOT_ARG_MODE_SET) && mode < (uint8_t)CORE_REPORTFORMAT_MAX;
            const bool dongle = (command == GAMEPAD_CMD_REBOOT_DONGLE);
            boot_memory_s mem = {
                .report_format = chosen ? mode : (uint8_t)core_current_reportformat(),
                .mode_chosen = chosen,
                .gamepad_pair = given && (arg & REBOOT_ARG_PAIR),
                .gamepad_method = dongle ? (uint8_t)GAMEPAD_METHOD_DONGLE : (uint8_t)GAMEPAD_METHOD_BLUETOOTH,
            };
            boot_set_memory(&mem);
            sys_hal_reboot();
        }
        break;

        // On USB, another mode now, in place (see hoja_request_usb_mode)
        case GAMEPAD_CMD_USB_MODE:
            cb(CFG_BLOCK_GAMEPAD, command, hoja_request_usb_mode((core_reportformat_t)arg), NULL, 0);
            break;

        case GAMEPAD_CMD_ENABLE_BLUETOOTH_UPLOAD:
        {
            // The update goes over the cable, so not through the dongle, nor over BLE without one
            battery_status_s power = {0};
            battery_get_status(&power);
            if (core_current_params()->transport_type == GAMEPAD_TRANSPORT_DONGLE ||
                (webusb_get_sink() && !power.plugged))
            {
                cb(CFG_BLOCK_GAMEPAD, command, false, NULL, 0);
                break;
            }

            // Persist a boot flag and reboot into ESP32 baseband firmware-update
            // mode (ALTFLASH). This runs inside the USB transport's own RX
            // callback, so we must NOT hoja_deinit() here (that tears down the
            // transport we're executing inside). Reboot immediately instead,
            // like GAMEPAD_CMD_RESET_TO_BOOTLOADER; the watchdog reboot preserves
            // the boot-memory scratch register, so we come up in ALTFLASH.
            boot_memory_s mem = {.baseband_update = true};
            boot_set_memory(&mem);
            sys_hal_reboot();
        }
        break;

        case GAMEPAD_CMD_SAVE_ALL:
            settings_commit_blocks();
            cb(CFG_BLOCK_GAMEPAD, command, true, NULL, 0);
            
        break;
    }
}

// Input only commands that don't involve writing data
void settings_config_command(cfg_block_t block, uint8_t command, uint8_t arg)
{
    switch(block)
    {
        default:
        return;

        case CFG_BLOCK_GAMEPAD:
            _gamepad_config_command(command, arg, webusb_command_confirm_cb);
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
            {
                mapper_init();
                anm_authentic_refresh();
            }
        break;
    }

    if(write) memcpy(&(write_to_ptr[BLOCK_CHUNK_MAX*write_idx]), &(data[BLOCK_CHUNK_BEGIN_IDX]), write_size);

    if(block == CFG_BLOCK_INPUT && write)
        anm_authentic_refresh();
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

bool settings_config_block_bytes(cfg_block_t block, uint8_t **data, uint32_t *size)
{
    switch(block)
    {
        default:
        return false;

        case CFG_BLOCK_GAMEPAD:
            *data = live_settings.gamepad_configuration_block; *size = GAMEPAD_CFB_SIZE;
        break;

        case CFG_BLOCK_HOVER:
            *data = live_settings.hover_configuration_block; *size = HOVER_CFB_SIZE;
        break;

        case CFG_BLOCK_ANALOG:
            *data = live_settings.analog_configuration_block; *size = ANALOG_CFB_SIZE;
        break;

        case CFG_BLOCK_RGB:
            *data = live_settings.rgb_configuration_block; *size = RGB_CFB_SIZE;
        break;

        case CFG_BLOCK_TRIGGER:
            *data = live_settings.trigger_configuration_block; *size = TRIGGER_CFB_SIZE;
        break;

        case CFG_BLOCK_IMU:
            *data = live_settings.imu_configuration_block; *size = IMU_CFB_SIZE;
        break;

        case CFG_BLOCK_HAPTIC:
            *data = live_settings.haptic_configuration_block; *size = HAPTIC_CFB_SIZE;
        break;

        case CFG_BLOCK_USER:
            *data = live_settings.user_configuration_block; *size = USER_CFB_SIZE;
        break;

        case CFG_BLOCK_INPUT:
            *data = live_settings.input_configuration_block; *size = INPUT_CFB_SIZE;
        break;
    }
    return true;
}

void settings_return_config_block(cfg_block_t block, setting_callback_t cb)
{
    uint8_t *data;
    uint32_t size;
    if (settings_config_block_bytes(block, &data, &size))
        _serialize_block(block, data, size, cb);
}

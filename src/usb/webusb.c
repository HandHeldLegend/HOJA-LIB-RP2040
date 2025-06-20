#include "usb/webusb.h"
#include "utilities/interval.h"
#include "input_shared_types.h"

#include "utilities/settings.h"
#include "hal/sys_hal.h"

#include "input/analog.h"
#include "input/button.h"
#include "input/imu.h"
#include "input/trigger.h"
#include "input/remap.h"

#include "bsp/board.h"
#include "tusb.h"

#define WEBUSB_ITF 0

bool _ready_to_go = false;
bool webusb_outputting_check()
{
    return _ready_to_go;
}

// Can be used to block until webUSB
// is clear and ready to send output data
// Set timeout to value greater than zero.
bool webusb_ready_blocking(int timeout)
{
    if (timeout > 0)
    {
        int internal = timeout;
        while (!tud_vendor_n_write_available(WEBUSB_ITF) && (internal > 0))
        {
            sys_hal_sleep_ms(1);
            tud_task();
            internal--;
        }

        if (!internal)
        {
            return false;
        }

        return true;
    }
    else
    {
        return false;
    }

    return true;
}


uint8_t _webusb_out_buffer[64] = {0x00};
void webusb_send_bulk(const uint8_t *data, uint16_t size)
{
    if(!_ready_to_go) return;

    memset(_webusb_out_buffer, 0, 64);
    memcpy(_webusb_out_buffer, data, size);

    if(webusb_ready_blocking(256))
    {
        tud_vendor_n_write(0, _webusb_out_buffer, 64);
        tud_vendor_n_flush(0);
    }
    else 
    {
        _ready_to_go = false;
    }
}

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))
void webusb_send_rawinput(uint64_t timestamp)
{
    static interval_s interval_ready = {0};
    static interval_s interval = {0};
    static button_data_s remapped_buttons = {0};
    static button_data_s buttons = {0};
    static trigger_data_s triggers = {0};
    static analog_data_s analog = {0};
    static imu_data_s imu = {0};

    static bool ready = false;

    if(!_ready_to_go) return;

    if(!ready)
    {
        if(webusb_ready_blocking(256))
        {
            ready = true;
        }
        else _ready_to_go = false;
    }

    if (interval_run(timestamp, 8000, &interval) && ready)
    {
        uint8_t webusb_input_report[64] = {0};

        analog_access_safe(&analog, ANALOG_ACCESS_SNAPBACK_DATA);
        trigger_access_safe(&triggers, TRIGGER_ACCESS_SCALED_DATA);
        button_access_safe(&buttons, BUTTON_ACCESS_RAW_DATA);
        imu_access_safe(&imu);

        webusb_input_report[0] = WEBUSB_INPUT_RAW;

        uint16_t lx = (uint16_t) (analog.lx + 2048);
        uint16_t ly = (uint16_t) (analog.ly + 2048);
        uint16_t rx = (uint16_t) (analog.rx + 2048);
        uint16_t ry = (uint16_t) (analog.ry + 2048);

        webusb_input_report[1] = (lx & 0xFF00) >> 8;
        webusb_input_report[2] = (lx & 0xFF);       
        webusb_input_report[3] = (ly & 0xFF00) >> 8;
        webusb_input_report[4] = (ly & 0xFF);       

        webusb_input_report[5] = (rx & 0xFF00) >> 8;
        webusb_input_report[6] = (rx & 0xFF);       
        webusb_input_report[7] = (ry & 0xFF00) >> 8;
        webusb_input_report[8] = (ry & 0xFF);       
        webusb_input_report[9] = (buttons.buttons_all >> 8) & 0xFF;
        webusb_input_report[10] = buttons.buttons_all & 0xFF;
        webusb_input_report[11] = buttons.buttons_system;
        webusb_input_report[12] = (triggers.left_analog & 0xFF00)  >> 8;
        webusb_input_report[13] = (triggers.left_analog & 0xFF);
        webusb_input_report[14] = (triggers.right_analog & 0xFF00)  >> 8;
        webusb_input_report[15] = (triggers.right_analog & 0xFF);

        memcpy(&webusb_input_report[16], &imu.ax, 2);
        memcpy(&webusb_input_report[18], &imu.ay, 2);
        memcpy(&webusb_input_report[20], &imu.az, 2);

        memcpy(&webusb_input_report[22], &imu.gx, 2);
        memcpy(&webusb_input_report[24], &imu.gy, 2);
        memcpy(&webusb_input_report[26], &imu.gz, 2);

        analog_access_safe(&analog, ANALOG_ACCESS_DEADZONE_DATA);
        lx = (uint16_t) (analog.lx + 2048);
        ly = (uint16_t) (analog.ly + 2048);
        rx = (uint16_t) (analog.rx + 2048);
        ry = (uint16_t) (analog.ry + 2048);
        webusb_input_report[28] = (lx & 0xFF00) >> 8;
        webusb_input_report[29] = (lx & 0xFF);
        webusb_input_report[30] = (ly & 0xFF00) >> 8;
        webusb_input_report[31] = (ly & 0xFF);
        webusb_input_report[32] = (rx & 0xFF00) >> 8;
        webusb_input_report[33] = (rx & 0xFF);
        webusb_input_report[34] = (ry & 0xFF00) >> 8;
        webusb_input_report[35] = (ry & 0xFF);

        tud_vendor_n_write(0, webusb_input_report, 64);
        tud_vendor_n_flush(0);

        remap_get_processed_input(&remapped_buttons, &triggers);

        ready = false;
    }
}

void webusb_command_confirm_cb(cfg_block_t config_block, uint8_t cmd, bool success, uint8_t *data, uint32_t size)
{
    uint8_t cmd_confirm_buffer[64] = {0x00};
    cmd_confirm_buffer[0] = WEBUSB_ID_CONFIG_COMMAND;
    cmd_confirm_buffer[1] = config_block;
    cmd_confirm_buffer[2] = cmd;
    cmd_confirm_buffer[3] = success;

    if(data) 
        memcpy(&cmd_confirm_buffer[4], data, size);

    webusb_send_bulk(cmd_confirm_buffer, 4+size);
}

void webusb_command_handler(uint8_t *data, uint32_t size)
{
    _ready_to_go = true;
    switch(data[0])
    {
        case WEBUSB_ID_READ_CONFIG_BLOCK:
            settings_return_config_block(data[1], webusb_send_bulk);
        break;

        case WEBUSB_ID_WRITE_CONFIG_BLOCK:
            settings_write_config_block(data[1], data);
        break;

        case WEBUSB_ID_READ_STATIC_BLOCK:
            static_config_read_block(data[1], webusb_send_bulk);
        break;

        case WEBUSB_ID_CONFIG_COMMAND:
            settings_config_command(data[1], data[2]);
        break;

        case WEBUSB_LEGACY_SET_BOOTLOADER:
            // Do nothing
        break;

        case WEBUSB_LEGACY_GET_FW_VERSION:
            // Do nothing
        break;
    }   
}

void webusb_version_read(uint8_t type)
{
    (void) type;
}

// Read out all configuration blocks
void webusb_settings_read()
{
    // Read all config blocks
    for(int i = 0; i < CFG_BLOCK_MAX; i++)
    {
        settings_return_config_block(i, webusb_send_bulk);
    }
}

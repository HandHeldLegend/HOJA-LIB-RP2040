#include "usb/webusb.h"
#include "utilities/interval.h"
#include "input_shared_types.h"

#include "utilities/settings.h"
#include "hal/sys_hal.h"

#include "input/analog.h"
#include "input/imu.h"
#include "input/hover.h"
#include "devices/battery.h"
#include "devices/fuelgauge.h"

#include "hoja.h"

#include "bsp/board.h"
#include "tusb.h"

#define WEBUSB_ITF 0

uint8_t _webusb_focused_hover = 0;
uint8_t _webusb_report_mode = WEBUSB_INPUT_RAW;

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
        static uint8_t webusb_input_report[64] = {0};

        // Charge status
        static battery_status_s batstat = {0};
        static fuelgauge_status_s fgstat = {0};

        battery_get_status(&batstat);
        fuelgauge_get_status(&fgstat);

        webusb_input_report[1] = (uint8_t)batstat.charging | ((uint8_t)batstat.charging_done << 1) | ((uint8_t)fgstat.connected << 2) | ((uint8_t) fgstat.discharge_only << 3) ;
        webusb_input_report[2] = fgstat.percent;

        static imu_data_s imu = {0};
        imu_access_safe(&imu);
        memcpy(&webusb_input_report[3], &imu.ax, 2);
        memcpy(&webusb_input_report[5], &imu.ay, 2);
        memcpy(&webusb_input_report[7], &imu.az, 2);

        memcpy(&webusb_input_report[9], &imu.gx, 2);
        memcpy(&webusb_input_report[11], &imu.gy, 2);
        memcpy(&webusb_input_report[13], &imu.gz, 2);

        analog_data_s joysticks;
        switch(_webusb_report_mode)
        {
            default:
            case WEBUSB_INPUT_RAW:
                webusb_input_report[0] = WEBUSB_INPUT_RAW;

                static mapper_input_s hover = {0};
                hover_access_safe(&hover);
                analog_access_safe(&joysticks, ANALOG_ACCESS_DEADZONE_DATA);

                hover.inputs[INPUT_CODE_LX_RIGHT] = (joysticks.lx > 0 ? joysticks.lx : 0);
                hover.inputs[INPUT_CODE_LX_LEFT] = joysticks.lx < 0 ? -joysticks.lx : 0;
                hover.inputs[INPUT_CODE_LY_UP] = joysticks.ly > 0 ? joysticks.ly : 0;
                hover.inputs[INPUT_CODE_LY_DOWN] = joysticks.ly < 0 ? -joysticks.ly : 0;

                hover.inputs[INPUT_CODE_RX_RIGHT] = joysticks.rx > 0 ? joysticks.rx : 0;
                hover.inputs[INPUT_CODE_RX_LEFT] = joysticks.rx < 0 ? -joysticks.rx : 0;
                hover.inputs[INPUT_CODE_RY_UP] = joysticks.ry > 0 ? joysticks.ry : 0;
                hover.inputs[INPUT_CODE_RY_DOWN] = joysticks.ry < 0 ? -joysticks.ry : 0;

                // One value is focused and we get the full uint16_t value
                uint16_t focused_val = hover.inputs[_webusb_focused_hover];
                webusb_input_report[14] = (focused_val & 0xFF00) >> 8;
                webusb_input_report[15] = (focused_val & 0xFF); 

                // Remainder of values are downsampled to 8 bits
                for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
                {
                    uint8_t scaled = (hover.inputs[i] >> 4) & 0xFF;
                    webusb_input_report[16+i] = (hover.inputs[i] > 0) & (!scaled) ? 1 : scaled;
                }
            break;

            case WEBUSB_INPUT_JOYSTICKS:
                webusb_input_report[0] = WEBUSB_INPUT_JOYSTICKS;

                analog_access_safe(&joysticks, ANALOG_ACCESS_SNAPBACK_DATA);
                uint16_t lx = (uint16_t) (joysticks.lx + 2048);
                uint16_t ly = (uint16_t) (joysticks.ly + 2048);
                uint16_t rx = (uint16_t) (joysticks.rx + 2048);
                uint16_t ry = (uint16_t) (joysticks.ry + 2048);
                webusb_input_report[14] = (lx & 0xFF00) >> 8;
                webusb_input_report[15] = (lx & 0xFF);       
                webusb_input_report[16] = (ly & 0xFF00) >> 8;
                webusb_input_report[17] = (ly & 0xFF);       
                webusb_input_report[18] = (rx & 0xFF00) >> 8;
                webusb_input_report[19] = (rx & 0xFF);       
                webusb_input_report[20] = (ry & 0xFF00) >> 8;
                webusb_input_report[21] = (ry & 0xFF);

                analog_access_safe(&joysticks, ANALOG_ACCESS_DEADZONE_DATA);
                lx = (uint16_t) (joysticks.lx + 2048);
                ly = (uint16_t) (joysticks.ly + 2048);
                rx = (uint16_t) (joysticks.rx + 2048);
                ry = (uint16_t) (joysticks.ry + 2048);
                webusb_input_report[22] = (lx & 0xFF00) >> 8;
                webusb_input_report[23] = (lx & 0xFF);
                webusb_input_report[24] = (ly & 0xFF00) >> 8;
                webusb_input_report[25] = (ly & 0xFF);
                webusb_input_report[26] = (rx & 0xFF00) >> 8;
                webusb_input_report[27] = (rx & 0xFF);
                webusb_input_report[28] = (ry & 0xFF00) >> 8;
                webusb_input_report[29] = (ry & 0xFF);
            break;
        }

        tud_vendor_n_write(0, webusb_input_report, 64);
        tud_vendor_n_flush(0);

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

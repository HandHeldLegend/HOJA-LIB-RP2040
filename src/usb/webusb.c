#include "usb/webusb.h"
#include "utilities/interval.h"
#include "input_shared_types.h"

#include "utilities/settings.h"
#include "hal/sys_hal.h"

#include "input/analog.h"
#include "input/button.h"
#include "input/imu.h"

#include "bsp/board.h"
#include "tusb.h"

#define WEBUSB_ITF 0

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
    memset(_webusb_out_buffer, 0, 64);
    memcpy(_webusb_out_buffer, data, size);

    if(webusb_ready_blocking(4000))
    {
        tud_vendor_n_write(0, _webusb_out_buffer, 64);
        tud_vendor_n_flush(0);
    }
}

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))
void webusb_input_task(uint32_t timestamp)
{
    static interval_s interval_ready = {0};
    static interval_s interval = {0};
    static button_data_s buttons = {0};
    static analog_data_s analog = {0};
    static imu_data_s imu = {0};

    static bool ready = false;

    if(interval_run(timestamp, 2000, &interval_ready))
    {
        //if(!ready)
        //    ready = _webusb_ready();
    }

    if (ready && interval_run(timestamp, 8000, &interval))
    {
        uint8_t webusb_input_report[64] = {0};

        analog_access_try(&analog, ANALOG_ACCESS_DEADZONE_DATA);
        button_access_try(&buttons, BUTTON_ACCESS_REMAPPED_DATA);

        webusb_input_report[0] = WEBUSB_INPUT_PROCESSED;

        webusb_input_report[1] = CLAMP_0_255(analog.lx >> 4);
        webusb_input_report[2] = CLAMP_0_255(analog.ly >> 4);
        webusb_input_report[3] = CLAMP_0_255(analog.rx >> 4);
        webusb_input_report[4] = CLAMP_0_255(analog.ry >> 4);

        webusb_input_report[5] = buttons.buttons_all & 0xFF;
        webusb_input_report[6] = (buttons.buttons_all >> 8) & 0xFF;
        webusb_input_report[7] = buttons.buttons_system;
        webusb_input_report[8] = CLAMP_0_255(buttons.zl_analog >> 4);
        webusb_input_report[9] = CLAMP_0_255(buttons.zr_analog >> 4);


        webusb_input_report[10] = CLAMP_0_255((uint8_t) ( ( (imu.ax*4)+32767)>>8));
        webusb_input_report[11] = CLAMP_0_255((uint8_t) ( ( (imu.ay*4)+32767)>>8));
        webusb_input_report[12] = CLAMP_0_255((uint8_t) ( ( (imu.az*4)+32767)>>8));
        webusb_input_report[13] = CLAMP_0_255((uint8_t) ( ( (imu.gx*4)+32767)>>8));
        webusb_input_report[14] = CLAMP_0_255((uint8_t) ( ( (imu.gy*4)+32767)>>8));
        webusb_input_report[15] = CLAMP_0_255((uint8_t) ( ( (imu.gz*4)+32767)>>8));

        //tud_vendor_n_write(0, webusb_input_report, 64);
        //tud_vendor_n_flush(0);
    }
}

void webusb_command_confirm_cb(cfg_block_t config_block, uint8_t cmd)
{
    uint8_t cmd_confirm_buffer[64] = {0x00};
    cmd_confirm_buffer[0] = WEBUSB_ID_COMMAND_CONFIRM;
    cmd_confirm_buffer[1] = config_block;
    cmd_confirm_buffer[2] = cmd;
    webusb_send_bulk(cmd_confirm_buffer, 3);
}

void webusb_command_handler(uint8_t *data, uint32_t size)
{
    switch(data[0])
    {
        case WEBUSB_ID_READ_CONFIG_BLOCK:
            settings_return_config_block(data[1], webusb_send_bulk);
        break;

        /*  
            Data must be written in chunks of 32 bytes, set at index 3
            index 1 defines which block to write the data to
            index 2 is the index number of the array data. Index 2 value of 0xFF indicates
            that our data is complete and we should now write the whole block to active memory.
        */
        case WEBUSB_ID_WRITE_CONFIG_BLOCK:
            settings_write_config_block(data[1], data);
        break;

        case WEBUSB_ID_READ_ALL_CONFIG_BLOCKS:
        break;;

        case WEBUSB_ID_READ_STATIC_BLOCK:
        break;

        case WEBUSB_ID_READ_ALL_STATIC_BLOCKS:
        break;

        case WEBUSB_ID_CONFIG_COMMAND:
            settings_config_command(data[1], data[2]);
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

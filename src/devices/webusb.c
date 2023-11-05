#include "webusb.h"
#include "interval.h"

uint8_t _webusb_out_buffer[64] = {0x00};
bool _webusb_output_enabled = false;

hoja_capabilities_t _webusb_capabilities = {
    .analog_stick_left = HOJA_CAPABILITY_ANALOG_STICK_L,
    .analog_stick_right = HOJA_CAPABILITY_ANALOG_STICK_R,
    .analog_trigger_left = HOJA_CAPABILITY_ANALOG_TRIGGER_L,
    .analog_trigger_right = HOJA_CAPABILITY_ANALOG_TRIGGER_R,
    .bluetooth = HOJA_CAPABILITY_BLUETOOTH,
    .rgb = HOJA_CAPABILITY_RGB,
    .gyroscope = HOJA_CAPABILITY_GYRO,
    .nintendo_serial = HOJA_CAPABILITY_NINTENDO_SERIAL,
    .nintendo_joybus = HOJA_CAPABILITY_NINTENDO_JOYBUS,
    .rumble = HOJA_CAPABILITY_RUMBLE,
    .battery_pmic = HOJA_CAPABILITY_BATTERY,
    .padding = 0,
};

void webusb_save_confirm()
{
    printf("Sending Save receipt...\n");
    memset(_webusb_out_buffer, 0, 64);
    _webusb_out_buffer[0] = 0xF1;
    if (webusb_ready_blocking(4000))
    {
        tud_vendor_n_write(0, _webusb_out_buffer, 64);
        tud_vendor_n_flush(0);
    }
}

void webusb_command_processor(uint8_t *data)
{
    switch (data[0])
    {
    default:
        break;

    case WEBUSB_CMD_BB_SET:
    {
        #if(HOJA_CAPABILITY_BLUETOOTH)
            cb_hoja_set_uart_enabled(true);
            cb_hoja_set_bluetooth_enabled(true);
        #endif
    }
    break;

    case WEBUSB_CMD_CAPABILITIES_GET:
    {
        printf("WebUSB: Got Capabilities GET command.\n");
        _webusb_out_buffer[0] = WEBUSB_CMD_CAPABILITIES_GET;
        memcpy(&_webusb_out_buffer[1], &_webusb_capabilities, sizeof(uint8_t)*2);

        webusb_enable_output(false);
        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
        sleep_ms(30);
        webusb_enable_output(true);
    }
    break;

    case WEBUSB_CMD_IMU_CALIBRATION_START:
    {
        printf("WebUSB: Got IMU calibration START command.\n");
        imu_calibrate_start();
    }
    break;

    case WEBUSB_CMD_FW_SET:
    {
        reset_usb_boot(0, 0);
    }
    break;

    case WEBUSB_CMD_FW_GET:
    {
        _webusb_out_buffer[0] = WEBUSB_CMD_FW_GET;
        _webusb_out_buffer[1] = (HOJA_FW_VERSION & 0xFF00) >> 8;
        _webusb_out_buffer[2] = HOJA_FW_VERSION & 0xFF;
        _webusb_out_buffer[3] = (HOJA_DEVICE_ID & 0xFF00) >> 8;
        _webusb_out_buffer[4] = (HOJA_DEVICE_ID & 0xFF);
        _webusb_out_buffer[5] = (HOJA_BACKEND_VERSION & 0xFF00) >> 8;
        _webusb_out_buffer[6] = (HOJA_BACKEND_VERSION & 0xFF);
        webusb_enable_output(false);

        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
        sleep_ms(30);
        webusb_enable_output(true);
    }
    break;

    case WEBUSB_CMD_CALIBRATION_START:
    {
        printf("WebUSB: Got calibration START command.\n");
        analog_calibrate_start();
        _webusb_out_buffer[0] = WEBUSB_CMD_CALIBRATION_START;
        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    case WEBUSB_CMD_CALIBRATION_STOP:
    {
        printf("WebUSB: Got calibration STOP command.\n");
        analog_calibrate_stop();
        _webusb_out_buffer[0] = WEBUSB_CMD_CALIBRATION_STOP;
        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    case WEBUSB_CMD_ANALOG_INVERT_SET:
    {
        printf("WebUSB: Got analog invert SET command.\n");
        switch(data[1])
        {
            default:
            break;

            case 0:
            global_loaded_settings.lx_center.invert = (data[2]>0);
            break;

            case 1:
            global_loaded_settings.ly_center.invert = (data[2]>0);
            break;

            case 2:
            global_loaded_settings.rx_center.invert = (data[2]>0);
            break;

            case 3:
            global_loaded_settings.ry_center.invert = (data[2]>0);
            break;
        }
        stick_scaling_get_settings();
    }
    break;

    case WEBUSB_CMD_ANALOG_INVERT_GET:
    {
        printf("WebUSB: Got analog invert GET command.\n");
        _webusb_out_buffer[0] = WEBUSB_CMD_ANALOG_INVERT_GET;
        _webusb_out_buffer[1] = global_loaded_settings.lx_center.invert;
        _webusb_out_buffer[2] = global_loaded_settings.ly_center.invert;
        _webusb_out_buffer[3] = global_loaded_settings.rx_center.invert;
        _webusb_out_buffer[4] = global_loaded_settings.ry_center.invert;

        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    case WEBUSB_CMD_OCTAGON_SET:
    {
        printf("WebUSB: Got angle capture command.\n");
        analog_calibrate_angle();
        stick_scaling_set_settings();
        stick_scaling_init();
    }
    break;

    // Set RGB Group
    case WEBUSB_CMD_RGB_SET:
    {
        printf("WebUSB: Got RGB SET command.\n");
        rgb_s col =
            {
                .r = data[2],
                .g = data[3],
                .b = data[4],
            };
        global_loaded_settings.rgb_colors[data[1]] = col.color;
        rgb_set_group(data[1], col.color);
        rgb_set_dirty();
    }
    break;

    case WEBUSB_CMD_RGB_GET:
    {
        printf("WebUSB: Got RGB GET command.\n");
        memset(_webusb_out_buffer, 0, 64);
        _webusb_out_buffer[0] = WEBUSB_CMD_RGB_GET;

        for (uint8_t i = 0; i < 12; i++)
        {
            rgb_s c = {.color = global_loaded_settings.rgb_colors[i]};
            uint8_t t = (i * 3) + 1;
            _webusb_out_buffer[t] = c.r;
            _webusb_out_buffer[t + 1] = c.g;
            _webusb_out_buffer[t + 2] = c.b;
        }
        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    case WEBUSB_CMD_REMAP_SET:
    {
        printf("WebUSB: Got Remap SET command.\n");
        remap_listen_enable(data[1], data[2]);
    }
    break;

    case WEBUSB_CMD_REMAP_GET:
    {
        printf("WebUSB: Got Remap GET command.\n");
        remap_send_data_webusb(data[1]);
    }
    break;

    case WEBUSB_CMD_GCSP_SET:
    {
        printf("WebUSB: Got GCSP SET command.\n");
        
        if(data[1] == 0xFF)
        {
            global_loaded_settings.gc_sp_light_trigger = data[2];
        }
        else
        {
            remap_set_gc_sp(data[1]);
        }
    }
    break;

    case WEBUSB_CMD_REMAP_DEFAULT:
    {
        printf("WebUSB: Got Remap SET default command.\n");
        remap_reset_default(data[1]);
        remap_send_data_webusb(data[1]);
    }
    break;

    case WEBUSB_CMD_VIBRATE_GET:
    {
        printf("WebUSB: Got Vibrate GET command.");
        memset(_webusb_out_buffer, 0, 64);
        _webusb_out_buffer[0] = WEBUSB_CMD_VIBRATE_GET;
        _webusb_out_buffer[1] = global_loaded_settings.rumble_intensity;

        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    case WEBUSB_CMD_VIBRATE_SET:
    {
        printf("WebUSB: Got Vibrate SET command.");
        settings_set_rumble(data[1]);
    }
    break;

    case WEBUSB_CMD_VIBRATEFLOOR_GET:
    {
        printf("WebUSB: Got Vibrate FLOOR GET command.");
        memset(_webusb_out_buffer, 0, 64);
        _webusb_out_buffer[0] = WEBUSB_CMD_VIBRATEFLOOR_GET;
        _webusb_out_buffer[1] = global_loaded_settings.rumble_floor;

        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    case WEBUSB_CMD_VIBRATEFLOOR_SET:
    {
        printf("WebUSB: Got Vibrate FLOOR SET command.");
        settings_set_rumble_floor(data[1]);
    }
    break;

    case WEBUSB_CMD_SAVEALL:
    {
        printf("WebUSB: Got SAVE command.\n");
        settings_save_webindicate();
        settings_save();
    }
    break;
    }
}

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))

void webusb_input_report_task(uint32_t timestamp, a_data_s *analog)
{
    if (interval_run(timestamp, 16000))
    {
        uint8_t webusb_input_report[64] = {0};
        webusb_input_report[0] = WEBUSB_CMD_INPUT_REPORT;
        webusb_input_report[1] = CLAMP_0_255(analog->lx >> 4);
        webusb_input_report[2] = CLAMP_0_255(analog->ly >> 4);
        webusb_input_report[3] = CLAMP_0_255(analog->rx >> 4);
        webusb_input_report[4] = CLAMP_0_255(analog->ry >> 4);

        if (webusb_ready_blocking(5000))
        {
            tud_vendor_n_write(0, webusb_input_report, 64);
            tud_vendor_n_flush(0);
            analog_send_reset();
        }
    }
}

// Can be used to block until webUSB
// is clear and ready to send output data
// Set timeout to value greater than zero.
bool webusb_ready_blocking(int timeout)
{
    if (timeout > 0)
    {
        int internal = timeout;
        while (!tud_vendor_n_write_available(0) && (internal > 0))
        {
            sleep_us(100);
            tud_task();
            internal--;
        }

        if (!internal)
        {
            webusb_enable_output(false);
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

bool webusb_output_enabled()
{
    return _webusb_output_enabled;
}

void webusb_enable_output(bool enable)
{
    _webusb_output_enabled = enable;
}
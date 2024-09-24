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
    .rumble_erm = HOJA_CAPABILITY_RUMBLE_ERM,
    .battery_pmic = HOJA_CAPABILITY_BATTERY,
    .rumble_lra = HOJA_CAPABILITY_RUMBLE_LRA,
    .padding = 0,
};

void webusb_save_confirm()
{
    printf("Sending Save receipt...\n");
    memset(_webusb_out_buffer, 0, 64);
    _webusb_out_buffer[0] = WEBUSB_CMD_SAVEALL;
    if (webusb_ready_blocking(4000))
    {
        tud_vendor_n_write(0, _webusb_out_buffer, 64);
        tud_vendor_n_flush(0);
    }

    webusb_enable_output(true);
}

void webusb_send_debug_dump(uint8_t len, uint8_t *data)
{
    if(!len) return;
    memset(_webusb_out_buffer, 0, 64);
    _webusb_out_buffer[0] = WEBUSB_CMD_DEBUG_REPORT;
    _webusb_out_buffer[1] = len;

    memcpy(&(_webusb_out_buffer[2]), data, len);

    if (webusb_ready_blocking(4000))
    {
        tud_vendor_n_write(0, _webusb_out_buffer, 64);
        tud_vendor_n_flush(0);
    }
}

/**
 * @brief Processes the webUSB commands received.
 *
 * This function takes a pointer to the data received and processes the webUSB commands based on the value of data[0].
 * It performs different actions based on the command received, such as enabling Bluetooth, getting device capabilities,
 * starting IMU calibration, setting firmware, getting firmware information, starting analog calibration, stopping analog calibration,
 * setting analog invert, getting analog invert, getting analog subangle, setting analog subangle, getting analog octoangle,
 * setting analog octoangle, setting octagon angle capture, setting RGB color, getting RGB color, setting remap, getting remap,
 * setting GCSP (GameCube Speed Profile), setting remap to default, getting vibrate intensity, setting vibrate intensity floor,
 * saving settings, getting hardware test result.
 *
 * @param data Pointer to the data received.
 */
void webusb_command_processor(uint8_t *data)
{
    memset(_webusb_out_buffer, 0, 64);

    switch (data[0])
    {
    default:
        break;

    case WEBUSB_CMD_BATTERY_STATUS_GET:
    {
        _webusb_out_buffer[0] = WEBUSB_CMD_BATTERY_STATUS_GET;
        uint16_t battery_level = battery_get_level()/10;
        _webusb_out_buffer[1] = (uint8_t) battery_level;
        _webusb_out_buffer[2] = battery_get_plugged_status();
        _webusb_out_buffer[3] = battery_get_charging_status();

        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    case WEBUSB_CMD_BB_SET:
    {
#if (HOJA_CAPABILITY_BLUETOOTH)
        hoja_set_baseband_update(true);
        btinput_init(INPUT_MODE_BASEBANDUPDATE);
#endif
    }
    break;

#if (HOJA_CAPABILITY_ANALOG_TRIGGER_L | HOJA_CAPABILITY_ANALOG_TRIGGER_R)
    case WEBUSB_CMD_TRIGGER_CALIBRATION_START:
    {
        printf("WebUSB: Got trigger calibrate START command.\n");
        
        triggers_start_calibration();
    }
    break;

    case WEBUSB_CMD_TRIGGER_CALIBRATION_STOP:
    {
        printf("WebUSB: Got trigger calibrate STOP command.\n");
        triggers_stop_calibration();
    }
    break;
#endif

    case WEBUSB_CMD_TRIGGER_CALIBRATION_GET:
    {
        printf("WebUSB: Got Trigger Calibration GET command.\n");

        _webusb_out_buffer[0] = WEBUSB_CMD_TRIGGER_CALIBRATION_GET;
        _webusb_out_buffer[1] = global_loaded_settings.trigger_l.disabled;
        _webusb_out_buffer[2] = global_loaded_settings.trigger_r.disabled;

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

    case WEBUSB_CMD_TRIGGER_CALIBRATION_SET:
    {
        printf("WebUSB: Got Trigger Calibration SET command.\n");

        // Sanitize data
        uint8_t axis = data[1] & 0x1;
        uint8_t disable = data[2] & 0x1;
        triggers_set_disabled(axis, disable);
    }
    break;

    case WEBUSB_CMD_CAPABILITIES_GET:
    {
        printf("WebUSB: Got Capabilities GET command.\n");

        // Set Player LEDs all
        rgb_set_player(4);

        _webusb_out_buffer[0] = WEBUSB_CMD_CAPABILITIES_GET;
        memcpy(&_webusb_out_buffer[1], &_webusb_capabilities, sizeof(uint8_t) * 2);

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

        uint16_t bt_fw_version = btinput_get_version();

        _webusb_out_buffer[7] = (bt_fw_version & 0xFF00) >> 8;
        _webusb_out_buffer[8] = (bt_fw_version & 0xFF);

        _webusb_out_buffer[9] = (HOJA_SETTINGS_VERSION & 0xFF00) >> 8;
        _webusb_out_buffer[10] = (HOJA_SETTINGS_VERSION & 0xFF);
        _webusb_out_buffer[11] = settings_get_bank();

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
        switch (data[1])
        {
        default:
            break;

        case 0:
            global_loaded_settings.lx_center.invert = (data[2] > 0);
            break;

        case 1:
            global_loaded_settings.ly_center.invert = (data[2] > 0);
            break;

        case 2:
            global_loaded_settings.rx_center.invert = (data[2] > 0);
            break;

        case 3:
            global_loaded_settings.ry_center.invert = (data[2] > 0);
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

    case WEBUSB_CMD_SUBANGLE_GET:
    {
        printf("WebUSB: Got analog subangle GET command.\n");
        _webusb_out_buffer[0] = WEBUSB_CMD_SUBANGLE_GET;

        uint8_t axis = 0;
        uint8_t octant = 0;

        analog_get_subangle_data(&axis, &octant);

        _webusb_out_buffer[1] = axis;
        _webusb_out_buffer[2] = octant;

        switch (axis)
        {
        case 0:
            memcpy(&(_webusb_out_buffer[3]), &(global_loaded_settings.l_sub_angles[octant]), sizeof(float));
            break;

        case 1:
            memcpy(&(_webusb_out_buffer[3]), &(global_loaded_settings.r_sub_angles[octant]), sizeof(float));
            break;
        }

        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    case WEBUSB_CMD_SUBANGLE_SET:
    {
        printf("WebUSB: Got analog subangle SET command.\n");
        if (!data[1])
        {
            memcpy(&(global_loaded_settings.l_sub_angles[data[2]]), &(data[3]), sizeof(float));
        }
        else
        {
            memcpy(&(global_loaded_settings.r_sub_angles[data[2]]), &(data[3]), sizeof(float));
        }

        stick_scaling_init();
    }
    break;

    case WEBUSB_CMD_OCTOANGLE_GET:
    {
        printf("WebUSB: Got analog octoangle GET command.\n");
        _webusb_out_buffer[0] = WEBUSB_CMD_OCTOANGLE_GET;

        uint8_t axis = 0;
        uint8_t octant = 0;

        analog_get_octoangle_data(&axis, &octant);

        _webusb_out_buffer[1] = axis;
        _webusb_out_buffer[2] = octant;

        switch (axis)
        {
        case 0:
            memcpy(&(_webusb_out_buffer[3]), &(global_loaded_settings.l_angles[octant]), sizeof(float));
            break;

        case 1:
            memcpy(&(_webusb_out_buffer[3]), &(global_loaded_settings.r_angles[octant]), sizeof(float));
            break;
        }

        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    case WEBUSB_CMD_OCTOANGLE_SET:
    {
        printf("WebUSB: Got analog octoangle SET command.\n");
        if (!data[1])
        {
            memcpy(&(global_loaded_settings.l_angles[data[2]]), &(data[3]), sizeof(float));
        }
        else
        {
            memcpy(&(global_loaded_settings.r_angles[data[2]]), &(data[3]), sizeof(float));
        }

        stick_scaling_init();
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

        uint8_t idx = data[1] % 32;

        rgb_s col =
            {
                .r = data[2],
                .g = data[3],
                .b = data[4],
            };
        global_loaded_settings.rgb_colors[idx] = col.color;
        rgb_set_group(idx, col.color, false);
    }
    break;

    case WEBUSB_CMD_RGB_GET:
    {
        printf("WebUSB: Got RGB GET command.\n");
        memset(_webusb_out_buffer, 0, 64);
        _webusb_out_buffer[0] = WEBUSB_CMD_RGB_GET;

        for (uint8_t i = 0; i < 16; i++)
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

    case WEBUSB_CMD_USERCYCLE_SET:
    {
        printf("WebUSB: Got RAINBOW SET command.\n");
        uint8_t idx = data[1] % 7;
        rgb_s col =
            {
                .r = data[2],
                .g = data[3],
                .b = data[4],
            };

        if(idx < 6)
        {
            global_loaded_settings.rainbow_colors[idx] = col.color;
        }
        else if(idx == 6)
        {
            rgb_update_speed(data[2]);
        }
        
    }
    break;

    case WEBUSB_CMD_USERCYCLE_GET:
    {
        printf("WebUSB: Got RAINBOW GET command.\n");
        memset(_webusb_out_buffer, 0, 64);
        _webusb_out_buffer[0] = WEBUSB_CMD_USERCYCLE_GET;

        for (uint8_t i = 0; i < 6; i++)
        {
            rgb_s c = {.color = global_loaded_settings.rainbow_colors[i]};
            uint8_t t = (i * 3) + 1;
            _webusb_out_buffer[t] = c.r;
            _webusb_out_buffer[t + 1] = c.g;
            _webusb_out_buffer[t + 2] = c.b;
        }

        _webusb_out_buffer[19] = global_loaded_settings.rgb_step_speed;
        
        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    case WEBUSB_CMD_RGBMODE_SET:
    {
        printf("WebUSB: Got RGBMODE SET command.\n");
        rgb_mode_t mode = data[1] % RGB_MODE_MAX;
        global_loaded_settings.rgb_mode = mode;
        rgb_init(mode, BRIGHTNESS_RELOAD);
    }
    break;

    case WEBUSB_CMD_RGBMODE_GET:
    {
        printf("WebUSB: Got RGBMODE GET command.\n");
        memset(_webusb_out_buffer, 0, 64);
        _webusb_out_buffer[0] = WEBUSB_CMD_RGBMODE_GET;
        _webusb_out_buffer[1] = global_loaded_settings.rgb_mode;

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

        if (data[1] == 0xFF)
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
        _webusb_out_buffer[0] = WEBUSB_CMD_VIBRATE_GET;
        _webusb_out_buffer[1] = global_loaded_settings.rumble_intensity;
        _webusb_out_buffer[2] = global_loaded_settings.rumble_mode;

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
        settings_set_rumble(data[1], data[2]);
        // Use data[2] for rumble mode
    }
    break;

    case WEBUSB_CMD_SAVEALL:
    {
        printf("WebUSB: Got SAVE command.\n");
        settings_save_webindicate();
        settings_save_from_core0();
    }
    break;

    case WEBUSB_CMD_HWTEST_GET:
    {

        // Reset our bluetooth flag
        hoja_hw_test_u _test = {0};
        _test.val = cb_hoja_hardware_test();
        btinput_capability_reset_flag();

        _webusb_out_buffer[0] = WEBUSB_CMD_HWTEST_GET;
        memcpy(&(_webusb_out_buffer[1]), &(_test.val), 2);

        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    case WEBUSB_CMD_DEADZONE_GET:
    {
        _webusb_out_buffer[0] = WEBUSB_CMD_DEADZONE_GET;
        memcpy(&(_webusb_out_buffer[1]), &global_loaded_settings.deadzone_left_center, 2);
        memcpy(&(_webusb_out_buffer[3]), &global_loaded_settings.deadzone_left_outer, 2);
        memcpy(&(_webusb_out_buffer[5]), &global_loaded_settings.deadzone_right_center, 2);
        memcpy(&(_webusb_out_buffer[7]), &global_loaded_settings.deadzone_right_outer, 2);

        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    case WEBUSB_CMD_DEADZONE_SET:
    {
        uint16_t combined = (data[2]<<8) | data[3];
        settings_set_deadzone(data[1], combined);
    }
    break;

    case WEBUSB_CMD_RUMBLETEST_GET:
    {
        cb_hoja_rumble_test();
    }
    break;

    case WEBUSB_CMD_BOOTMODE_SET:
    {
        settings_set_mode(data[1]);
    }
    break;

    case WEBUSB_CMD_BOOTMODE_GET:
    {
        _webusb_out_buffer[0] = WEBUSB_CMD_BOOTMODE_GET;
        _webusb_out_buffer[1] = global_loaded_settings.input_mode;

        if (webusb_ready_blocking(4000))
        {
            tud_vendor_n_write(0, _webusb_out_buffer, 64);
            tud_vendor_n_flush(0);
        }
    }
    break;

    }
}

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))
void webusb_input_report_task(uint32_t timestamp, a_data_s *analog, button_data_s *buttons)
{
    static interval_s interval = {0};

    if (interval_run(timestamp, 4000, &interval))
    {
        uint8_t webusb_input_report[64] = {0};
        webusb_input_report[0] = WEBUSB_CMD_INPUT_REPORT;

        if(analog != NULL)
        {
            webusb_input_report[1] = CLAMP_0_255(analog->lx >> 4);
            webusb_input_report[2] = CLAMP_0_255(analog->ly >> 4);
            webusb_input_report[3] = CLAMP_0_255(analog->rx >> 4);
            webusb_input_report[4] = CLAMP_0_255(analog->ry >> 4);
        }

        if (buttons != NULL)
        {
            webusb_input_report[5] = buttons->buttons_all & 0xFF;
            webusb_input_report[6] = (buttons->buttons_all >> 8) & 0xFF;
            webusb_input_report[7] = buttons->buttons_system;
            webusb_input_report[8] = CLAMP_0_255(buttons->zl_analog >> 4);
            webusb_input_report[9] = CLAMP_0_255(buttons->zr_analog >> 4);
        }

        imu_data_s *web_imu = imu_fifo_last();
        if(web_imu != NULL)
        {
            
            webusb_input_report[10] = CLAMP_0_255((uint8_t) ( ( (web_imu->ax*4)+32767)>>8));
            webusb_input_report[11] = CLAMP_0_255((uint8_t) ( ( (web_imu->ay*4)+32767)>>8));
            webusb_input_report[12] = CLAMP_0_255((uint8_t) ( ( (web_imu->az*4)+32767)>>8));
            webusb_input_report[13] = CLAMP_0_255((uint8_t) ( ( (web_imu->gx*4)+32767)>>8));
            webusb_input_report[14] = CLAMP_0_255((uint8_t) ( ( (web_imu->gy*4)+32767)>>8));
            webusb_input_report[15] = CLAMP_0_255((uint8_t) ( ( (web_imu->gz*4)+32767)>>8));
        }

        if (webusb_ready_blocking(4000))
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
#include "btinput.h"
#include "interval.h"

#define HOJA_I2C_MSG_SIZE_OUT   32
#define HOJA_I2C_MSG_SIZE_IN    8

void btinput_init(input_mode_t input_mode)
{
    rgb_set_all(COLOR_BLUE.color);
    rgb_set_instant();

    cb_hoja_set_bluetooth_enabled(true);
    sleep_ms(600);
    rgb_preset_reload();
    rgb_set_dirty();

    static uint8_t data_out[HOJA_I2C_MSG_SIZE_OUT];
    data_out[HOJA_I2C_MSG_SIZE_OUT-3] = 0xDD;
    data_out[HOJA_I2C_MSG_SIZE_OUT-2] = 0xEE;
    data_out[HOJA_I2C_MSG_SIZE_OUT-1] = 0xFF;

    data_out[0] = I2CINPUT_ID_INIT;
    data_out[1] = (uint8_t) input_mode; 

    data_out[2] = global_loaded_settings.switch_mac_address[0];
    data_out[3] = global_loaded_settings.switch_mac_address[1];
    data_out[4] = global_loaded_settings.switch_mac_address[2];
    data_out[5] = global_loaded_settings.switch_mac_address[3];
    data_out[6] = global_loaded_settings.switch_mac_address[4];
    data_out[7] = global_loaded_settings.switch_mac_address[5];

    data_out[8] = global_loaded_settings.switch_host_address[0];
    data_out[9] = global_loaded_settings.switch_host_address[1];
    data_out[10] = global_loaded_settings.switch_host_address[2];
    data_out[11] = global_loaded_settings.switch_host_address[3];
    data_out[12] = global_loaded_settings.switch_host_address[4];
    data_out[13] = global_loaded_settings.switch_host_address[5];

    i2c_write_blocking(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false);
}

void _btinput_message_parse(uint8_t *msg)
{
    switch(msg[0])
    {
        default:
        break;

        case I2CINPUT_ID_STATUS:
        {
            static uint8_t _i_rumble = 0;
            i2cinput_status_s status = {.rumble_intensity = msg[1], .connected_status = msg[2]};

            if(_i_rumble != status.rumble_intensity)
            {
                _i_rumble = status.rumble_intensity;
                if(!_i_rumble)
                {
                    cb_hoja_rumble_enable(0);
                }
                else
                {
                    cb_hoja_rumble_enable((float) _i_rumble/100.0f);
                }
                
            }
        }
        break;

        case I2CINPUT_ID_SAVEMAC:
        {
            // Paired to Nintendo Switch
            printf("New BT Switch Pairing Completed.");
            // Save MAC address of host
            rgb_set_all(COLOR_RED.color);
            rgb_set_instant();
            global_loaded_settings.switch_host_address[0] = msg[1];
            global_loaded_settings.switch_host_address[1] = msg[2];
            global_loaded_settings.switch_host_address[2] = msg[3];
            global_loaded_settings.switch_host_address[3] = msg[4];
            global_loaded_settings.switch_host_address[4] = msg[5];
            global_loaded_settings.switch_host_address[5] = msg[6];
            settings_save();
        }
        break;
    }
    memset(msg, 0, HOJA_I2C_MSG_SIZE_IN);
}

void btinput_comms_task(uint32_t timestamp, button_data_s *buttons, a_data_s *analog)
{
    static uint8_t data_out[HOJA_I2C_MSG_SIZE_OUT];
    static uint8_t data_in[HOJA_I2C_MSG_SIZE_IN];
    data_out[HOJA_I2C_MSG_SIZE_OUT-3] = 0xDD;
    data_out[HOJA_I2C_MSG_SIZE_OUT-2] = 0xEE;
    data_out[HOJA_I2C_MSG_SIZE_OUT-1] = 0xFF;
    static i2cinput_input_s data = {0};

    if(interval_run(timestamp, 1500))
    {
        
        data_out[0]         = I2CINPUT_ID_INPUT;
        data_out[HOJA_I2C_MSG_SIZE_OUT-3] = 0xDD;
        data_out[HOJA_I2C_MSG_SIZE_OUT-2] = 0xEE;
        data_out[HOJA_I2C_MSG_SIZE_OUT-1] = 0xFF;

        data.buttons_all    = buttons->buttons_all;
        data.buttons_system = buttons->buttons_system;

        data.lx = analog->lx;
        data.ly = analog->ly;
        data.rx = analog->rx;
        data.ry = analog->ry;

        data.lt = analog->lt;
        data.rt = analog->rt;
        
        memcpy(&data_out[1], &data, sizeof(data));
        //analog_send_reset();

        //if(i2c_get_write_available(HOJA_I2C_BUS))
        i2c_write_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false, 8000);
        i2c_read_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_in, HOJA_I2C_MSG_SIZE_IN, false, 8000);
        _btinput_message_parse(data_in);
        analog_send_reset();
    }
}
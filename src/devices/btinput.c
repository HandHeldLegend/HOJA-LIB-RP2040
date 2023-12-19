#include "btinput.h"
#include "interval.h"

#define HOJA_I2C_MSG_SIZE_OUT   32
#define HOJA_I2C_MSG_SIZE_IN    8

void _pack_i2c_msg(i2cinput_input_s *input, uint8_t *output)
{
    output[0] = (input->buttons_all & 0xFF);
    output[1] = (input->buttons_all >> 8);
    output[2] = (input->buttons_system);

    
    
    // LX, LY, RX, RY, LT, RT
    output[3] = input->lx & 0xFF;
    output[4] = (input->lx >> 8);

    output[5] = input->ly & 0xFF;
    output[6] = (input->ly >> 8);

    output[7] = input->rx & 0xFF;
    output[8] = (input->rx >> 8);

    output[9] = input->ry & 0xFF;
    output[10] = (input->ry >> 8);

    output[11] = input->lt & 0xFF;
    output[12] = (input->lt >> 8);

    output[13] = input->rt & 0xFF;
    output[14] = (input->rt >> 8);

    // AX, AY, AZ, GX, GY, GZ
    output[15] = input->ax & 0xFF;
    output[16] = (input->ax >> 8);

    output[17] = input->ay & 0xFF;
    output[18] = (input->ay >> 8);

    output[19] = input->az & 0xFF;
    output[20] = (input->az >> 8);

    
    output[21] = input->gx & 0xFF;
    output[22] = (input->gx >> 8);

    output[23] = input->gy & 0xFF;
    output[24] = (input->gy >> 8);

    output[25] = input->gz & 0xFF;
    output[26] = (input->gz >> 8);
}

uint8_t data_out[HOJA_I2C_MSG_SIZE_OUT] = {0};

bool btinput_init(input_mode_t input_mode)
{
    #if (HOJA_CAPABILITY_BLUETOOTH==1)

        rgb_flash((input_mode == INPUT_MODE_SWPRO) ? COLOR_WHITE.color : COLOR_GREEN.color);
        cb_hoja_set_bluetooth_enabled(true);

        // Optional delay to ensure you have time to hook into the UART
        #ifdef HOJA_BT_LOGGING_DEBUG
            #if (HOJA_BT_LOGGING_DEBUG == 1 )
            sleep_ms(5000);
            #endif
        #endif

        sleep_ms(600);

        data_out[0] = 0xDD;
        data_out[1] = 0xEE;
        data_out[2] = 0xAA;

        data_out[3] = I2CINPUT_ID_INIT;
        data_out[4] = (uint8_t) input_mode; 

        data_out[5] = global_loaded_settings.switch_mac_address[0];
        data_out[6] = global_loaded_settings.switch_mac_address[1];
        data_out[7] = global_loaded_settings.switch_mac_address[2];
        data_out[8] = global_loaded_settings.switch_mac_address[3];
        data_out[9] = global_loaded_settings.switch_mac_address[4];
        data_out[10] = global_loaded_settings.switch_mac_address[5];

        data_out[11] = global_loaded_settings.switch_host_address[0];
        data_out[12] = global_loaded_settings.switch_host_address[1];
        data_out[13] = global_loaded_settings.switch_host_address[2];
        data_out[14] = global_loaded_settings.switch_host_address[3];
        data_out[15] = global_loaded_settings.switch_host_address[4];
        data_out[16] = global_loaded_settings.switch_host_address[5];

        int stat = i2c_write_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false, 150000);

        if(stat<0)
        {
            return false;
        }

        imu_set_enabled(true);
        return true;
    #else
    return false;
    #endif
}

void _btinput_message_parse(uint8_t *msg)
{
    #if (HOJA_CAPABILITY_BLUETOOTH==1)
    switch(msg[0])
    {
        default:
            memset(msg, 0, HOJA_I2C_MSG_SIZE_IN);
        break;

        case I2CINPUT_ID_SHUTDOWN:
        {
            hoja_shutdown();
        }
        break;

        case I2CINPUT_ID_STATUS:
        {
            static uint8_t _i_rumble = 0;
            static uint8_t _i_connected = 0;
            i2cinput_status_s status = {.rumble_intensity = msg[1], .connected_status = msg[2]};

            if(_i_connected)
            {
                if( (_i_rumble != status.rumble_intensity))
                {
                    _i_rumble = status.rumble_intensity;
                    _i_rumble = (_i_rumble > 100) ? 100 : _i_rumble;

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
            else
            {
                cb_hoja_rumble_enable(0);
            }

            if (_i_connected != status.connected_status)
            {
                _i_connected = status.connected_status;
                if(!_i_connected)
                {
                    rgb_flash(COLOR_BLUE.color);
                }
                else
                {
                    rgb_init(global_loaded_settings.rgb_mode, -1);
                }
            }
        }
        break;

        case I2CINPUT_ID_SAVEMAC:
        {
            // Paired to Nintendo Switch
            printf("New BT Switch Pairing Completed.");
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
    #endif
}

uint8_t data_out[HOJA_I2C_MSG_SIZE_OUT];
uint8_t data_in[HOJA_I2C_MSG_SIZE_IN];

void btinput_comms_task(uint32_t timestamp, button_data_s *buttons, a_data_s *analog)
{
    #if (HOJA_CAPABILITY_BLUETOOTH==1)
    
    static i2cinput_input_s data = {0};

    if(interval_run(timestamp, 1500))
    {
        data_out[0] = 0xDD;
        data_out[1] = 0xEE;
        data_out[2] = 0xAA;
        data_out[3] = I2CINPUT_ID_INPUT;

        data.buttons_all    = buttons->buttons_all;
        data.buttons_system = buttons->buttons_system;

        data.lx = (uint16_t) analog->lx;
        data.ly = (uint16_t) analog->ly;
        data.rx = (uint16_t) analog->rx;
        data.ry = (uint16_t) analog->ry;

        data.lt = (uint16_t) buttons->zl_analog;
        data.rt = (uint16_t) buttons->zr_analog;

        imu_data_s* imu_tmp = imu_fifo_last();

        if(imu_tmp != NULL)
        {
            data.gx = imu_tmp->gx;
            data.gy = imu_tmp->gy;
            data.gz = imu_tmp->gz;

            data.ax = imu_tmp->ax;
            data.ay = imu_tmp->ay;
            data.az = imu_tmp->az;
        }

        _pack_i2c_msg(&data, &(data_out[4]));

        int write = i2c_write_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false, 16000);
        if(write==HOJA_I2C_MSG_SIZE_OUT) analog_send_reset();

        int read = i2c_read_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_in, HOJA_I2C_MSG_SIZE_IN, false, 16000);
        if(read==HOJA_I2C_MSG_SIZE_IN) _btinput_message_parse(data_in);
        
    }
    #endif
}
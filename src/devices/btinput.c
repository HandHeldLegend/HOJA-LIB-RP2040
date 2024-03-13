#include "btinput.h"
#include "interval.h"

#define HOJA_I2C_MSG_SIZE_OUT   32
#define HOJA_I2C_MSG_SIZE_IN    11+1

uint32_t _mode_color = 0;

uint8_t data_out[HOJA_I2C_MSG_SIZE_OUT] = {0};


#define BTINPUT_GET_VERSION_ATTEMPTS 10

uint16_t btinput_get_version()
{
    uint8_t attempts = BTINPUT_GET_VERSION_ATTEMPTS;
    
    uint16_t v = 0xFFFF;
    #if (HOJA_CAPABILITY_BLUETOOTH == 1)

    v = 0xFFFE;
    cb_hoja_set_bluetooth_enabled(true);
    sleep_ms(650);

    static uint8_t data_in[HOJA_I2C_MSG_SIZE_IN] = {0};

    while(attempts--)
    {
        data_out[0] = 0xDD;
        data_out[1] = 0xEE;
        data_out[2] = 0xAA;

        data_out[3] = I2CINPUT_ID_GETVERSION;

        int stat = i2c_write_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false, 10000);
        sleep_ms(4);
        int read = i2c_read_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_in, HOJA_I2C_MSG_SIZE_IN, false, 10000);

        if(read==HOJA_I2C_MSG_SIZE_IN)
        {
            attempts=0;
            printf("BT Version received and response got.");
            v = (data_in[1] << 8) | data_in[2];
        }
    }

    cb_hoja_set_bluetooth_enabled(false);
    #endif
    return v;
}

bool btinput_init(input_mode_t input_mode)
{
    #if (HOJA_CAPABILITY_BLUETOOTH==1)
        #ifdef HOJA_BT_LOGGING_DEBUG
            #if (HOJA_BT_LOGGING_DEBUG == 1 )
            cb_hoja_set_uart_enabled(true);
            #endif
        #endif
        switch(input_mode)
        {
            case INPUT_MODE_SWPRO:
                _mode_color = COLOR_WHITE.color;
            break;

            case INPUT_MODE_XINPUT:
                _mode_color = COLOR_GREEN.color;
            break;

            default:
                input_mode = INPUT_MODE_SWPRO;
                _mode_color = COLOR_WHITE.color;
            break;
        }
        rgb_flash(_mode_color);
        cb_hoja_set_bluetooth_enabled(true);

        // Optional delay to ensure you have time to hook into the UART
        #ifdef HOJA_BT_LOGGING_DEBUG
            #if (HOJA_BT_LOGGING_DEBUG == 1 )
            sleep_ms(5000);
            #endif
        #endif

        sleep_ms(1000);

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

        case I2CINPUT_ID_REBOOT:
        {
            hoja_reboot_memory_u mem = {
                .reboot_reason      = ADAPTER_REBOOT_REASON_MODECHANGE,
                .gamepad_mode       = INPUT_MODE_SWPRO,
                .gamepad_protocol   = INPUT_METHOD_BLUETOOTH
                };

            reboot_with_memory(mem.value);
        }
        break;

        case I2CINPUT_ID_SHUTDOWN:
        {
            hoja_shutdown();
        }
        break;

        case I2CINPUT_ID_STATUS:
        {
            static i2cinput_status_s status = {0};
            static int8_t _i_connected = -1;

            memcpy(&status, &msg[1], sizeof(i2cinput_status_s));

            if(_i_connected==1)
            {
                if( (status.rumble_amplitude_lo>0) || (status.rumble_amplitude_hi>0) )
                {
                    float _ahi = (float) status.rumble_amplitude_hi / 100;
                    float _alo = (float) status.rumble_amplitude_lo / 100;
                    hoja_rumble_set(status.rumble_frequency_hi, _ahi, status.rumble_frequency_lo, _alo);
                }
                else
                {
                    hoja_rumble_set(0,0,0,0);
                }
            }
            else
            {
                hoja_rumble_set(0, 0,0,0);
            }

            if(_i_connected<0)
            {
                //rgb_flash(_mode_color);
                _i_connected = 0;
            }
            else if (_i_connected != (int8_t) status.connected_status )
            {
                if(status.connected_status == 1)
                {
                    rgb_init(global_loaded_settings.rgb_mode, -1);
                }
                else
                {
                    rgb_flash(_mode_color);
                }
                _i_connected = (int8_t) status.connected_status;
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
    static interval_s interval = {0};

    if(interval_run(timestamp, 1900, &interval))
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
        
        memcpy(&(data_out[4]), &data, sizeof(i2cinput_input_s));

        int write = i2c_write_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE_OUT, false, 16000);
        if(write==HOJA_I2C_MSG_SIZE_OUT) analog_send_reset();

        int read = i2c_read_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_in, HOJA_I2C_MSG_SIZE_IN, false, 16000);
        if(read==HOJA_I2C_MSG_SIZE_IN) _btinput_message_parse(data_in);
        
    }
    #endif
}
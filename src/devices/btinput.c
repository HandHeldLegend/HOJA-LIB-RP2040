#include "btinput.h"
#include "interval.h"

#define HOJA_I2C_MSG_SIZE 32

void btinput_init(input_mode_t input_mode)
{
    static uint8_t data_out[HOJA_I2C_MSG_SIZE];
    data_out[HOJA_I2C_MSG_SIZE-3] = 0xDD;
    data_out[HOJA_I2C_MSG_SIZE-2] = 0xEE;
    data_out[HOJA_I2C_MSG_SIZE-1] = 0xFF;

    data_out[0] = I2CINPUT_ID_INIT;
    data_out[1] = (uint8_t) input_mode; 
    i2c_write_blocking(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE, false);
}

void btinput_comms_task(uint32_t timestamp, button_data_s *buttons, a_data_s *analog)
{
    static uint8_t data_out[HOJA_I2C_MSG_SIZE];
    data_out[HOJA_I2C_MSG_SIZE-3] = 0xDD;
    data_out[HOJA_I2C_MSG_SIZE-2] = 0xEE;
    data_out[HOJA_I2C_MSG_SIZE-1] = 0xFF;
    static i2cinput_input_s data = {0};

    if(interval_run(timestamp, 2000))
    {
        data_out[0]         = I2CINPUT_ID_INPUT;
        data_out[HOJA_I2C_MSG_SIZE-3] = 0xDD;
        data_out[HOJA_I2C_MSG_SIZE-2] = 0xEE;
        data_out[HOJA_I2C_MSG_SIZE-1] = 0xFF;

        data.buttons_all    = buttons->buttons_all;
        data.buttons_system = buttons->buttons_system;

        data.lx = analog->lx;
        data.ly = analog->ly;
        data.rx = analog->rx;
        data.ry = analog->ry;

        data.lt = analog->lt;
        data.rt = analog->rt;
        
        memcpy(&data_out[1], &data, sizeof(data));
        analog_send_reset();

        //if(i2c_get_write_available(HOJA_I2C_BUS))
        i2c_write_timeout_us(HOJA_I2C_BUS, HOJA_I2CINPUT_ADDRESS, data_out, HOJA_I2C_MSG_SIZE, false, 8000);
    }
}
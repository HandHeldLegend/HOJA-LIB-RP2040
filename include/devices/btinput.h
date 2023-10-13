#ifndef BTINPUT_H
#define BTINPUT_H

#include "hoja_includes.h"

typedef enum
{
    I2CINPUT_ID_INIT    = 0x00,
    I2CINPUT_ID_INPUT   = 0x01,
    I2CINPUT_ID_STOP    = 0x02,
} i2cinput_id_t;

#define I2CINPUT_INIT_SIZE 7
#define I2CINPUT_INPUT_SIZE 27

typedef struct
{
    uint8_t mode;
    uint8_t mac[6];
} i2cinput_init_s;

typedef struct
{
    union
    {
        struct
        {
            // D-Pad
            uint8_t dpad_up     : 1;
            uint8_t dpad_down   : 1;
            uint8_t dpad_left   : 1;
            uint8_t dpad_right  : 1;
            // Buttons
            uint8_t button_a    : 1;
            uint8_t button_b    : 1;
            uint8_t button_x    : 1;
            uint8_t button_y    : 1;

            // Triggers
            uint8_t trigger_l   : 1;
            uint8_t trigger_zl  : 1;
            uint8_t trigger_r   : 1;
            uint8_t trigger_zr  : 1;

            // Special Functions
            uint8_t button_plus     : 1;
            uint8_t button_minus    : 1;

            // Stick clicks
            uint8_t button_stick_left   : 1;
            uint8_t button_stick_right  : 1;
        };
        uint16_t buttons_all;
    };

    union
    {
        struct
        {
            // Menu buttons (Not remappable by API)
            uint8_t button_capture  : 1;
            uint8_t button_home     : 1;
            uint8_t button_safemode : 1;
            uint8_t padding         : 5;
        };
        uint8_t buttons_system;
    };

    uint16_t lx;
    uint16_t ly;
    uint16_t rx;
    uint16_t ry;
    uint16_t lt;
    uint16_t rt;

    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    
} i2cinput_input_s;

void btinput_init(input_mode_t input_mode);
void btinput_comms_task(uint32_t timestamp, button_data_s *buttons, a_data_s *analog);

#endif
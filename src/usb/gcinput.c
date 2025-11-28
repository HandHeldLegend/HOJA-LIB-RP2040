#include "usb/gcinput.h"
#include "input/mapper.h"
#include <stdlib.h>
#include "usb/ginput_usbd.h"

/**--------------------------**/
/**--------------------------**/

bool _gc_first = false;
bool _gc_enable = false;

// Input structure for Nintendo GameCube Adapter USB Data
#pragma pack(push, 1) // Ensure byte alignment
typedef struct
{
    union
    {
        struct
        {
            uint8_t button_a    : 1;
            uint8_t button_b    : 1;
            uint8_t button_x    : 1;
            uint8_t button_y    : 1;
            uint8_t dpad_left   : 1; //Left
            uint8_t dpad_right  : 1; //Right
            uint8_t dpad_down   : 1; //Down
            uint8_t dpad_up     : 1; //Up
        };
        uint8_t buttons_1;
    };

    union
    {
        struct
        {
            uint8_t button_start: 1;
            uint8_t button_z    : 1;
            uint8_t button_r    : 1;
            uint8_t button_l    : 1;
            uint8_t blank1      : 4;
        }; 
        uint8_t buttons_2;
    };

  uint8_t stick_x;
  uint8_t stick_y;
  uint8_t cstick_x;
  uint8_t cstick_y;
  uint8_t trigger_l;
  uint8_t trigger_r;

} gc_input_s;
#pragma pack(pop)

void gcinput_enable(bool enable)
{
    _gc_enable = enable;
}

#define GCUSB_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

int16_t scale_with_deadzone(int16_t value, int16_t max, int16_t deadzone) {
    // Get the magnitude and sign of the input value
    int16_t magnitude = abs(value);
    int16_t sign = (value >= 0) ? 1 : -1;
    
    // If magnitude is within deadzone, return 0
    if (magnitude <= deadzone) {
        return 0;
    }
    
    // If magnitude is already at or above max, return max with appropriate sign
    if (magnitude >= max) {
        return sign * max;
    }
    
    // Calculate the usable range after deadzone
    int32_t usable_range = max - deadzone;
    int32_t adjusted_magnitude = magnitude - deadzone;
    
    // Scale the adjusted magnitude to the full 0-max range
    // Using int32_t to prevent overflow during multiplication
    int32_t scaled_magnitude = (adjusted_magnitude * max) / usable_range;
    
    return sign * (int16_t)scaled_magnitude;
}


void gcinput_hid_report(uint64_t timestamp, hid_report_tunnel_cb cb)
{
    static gc_input_s   data = {0};
    static uint8_t      buffer[37] = {0};

    mapper_input_s *input = mapper_get_input();

    buffer[0] = 0x21;

    const float   target_max = 110.0f / 2048.0f;

    //input->joysticks_combined[0] = scale_with_deadzone(input->joysticks_combined[0], 2047, 64);
    //input->joysticks_combined[1] = scale_with_deadzone(input->joysticks_combined[1], 2047, 64);
    //input->joysticks_combined[2] = scale_with_deadzone(input->joysticks_combined[2], 2047, 64);
    //input->joysticks_combined[3] = scale_with_deadzone(input->joysticks_combined[3], 2047, 64);
//
    //float lx = (float)input->joysticks_combined[0] * target_max;
    //float ly = (float)input->joysticks_combined[1] * target_max;
    //float rx = (float)input->joysticks_combined[2] * target_max;
    //float ry = (float)input->joysticks_combined[3] * target_max;

    //uint8_t lx8 = (uint8_t)GCUSB_CLAMP(lx + 128, 0, 255);
    //uint8_t ly8 = (uint8_t)GCUSB_CLAMP(ly + 128, 0, 255);
    //uint8_t rx8 = (uint8_t)GCUSB_CLAMP(rx + 128, 0, 255);
    //uint8_t ry8 = (uint8_t)GCUSB_CLAMP(ry + 128, 0, 255);

    data.button_a = MAPPER_BUTTON_DOWN(input->inputs, GAMECUBE_CODE_A);
    data.button_b = MAPPER_BUTTON_DOWN(input->inputs, GAMECUBE_CODE_B);
    data.button_x = MAPPER_BUTTON_DOWN(input->inputs, GAMECUBE_CODE_X);
    data.button_y = MAPPER_BUTTON_DOWN(input->inputs, GAMECUBE_CODE_Y);

    data.button_start   = MAPPER_BUTTON_DOWN(input->inputs, GAMECUBE_CODE_START);
    data.button_l       = MAPPER_BUTTON_DOWN(input->inputs, GAMECUBE_CODE_L);
    data.button_r       = MAPPER_BUTTON_DOWN(input->inputs, GAMECUBE_CODE_R);

    //uint8_t lt8 = data.button_l ? 255 : GCUSB_CLAMP(input->triggers[0] >> 4, 0, 255);
    //uint8_t rt8 = data.button_r ? 255 : GCUSB_CLAMP(input->triggers[1] >> 4, 0, 255);

    data.dpad_down   = MAPPER_BUTTON_DOWN(input->inputs, GAMECUBE_CODE_DOWN);
    data.dpad_left   = MAPPER_BUTTON_DOWN(input->inputs, GAMECUBE_CODE_LEFT);
    data.dpad_right  = MAPPER_BUTTON_DOWN(input->inputs, GAMECUBE_CODE_RIGHT);
    data.dpad_up     = MAPPER_BUTTON_DOWN(input->inputs, GAMECUBE_CODE_UP);
    data.button_z    = MAPPER_BUTTON_DOWN(input->inputs, GAMECUBE_CODE_Z);

    //data.stick_x = lx8;
    //data.stick_y = ly8;
    //data.cstick_x = rx8;
    //data.cstick_y = ry8;
    //data.trigger_l  = lt8;
    //data.trigger_r  = rt8;

    if(!_gc_first)
    {
        /*GC adapter notes for new data

        with only black USB plugged in
        - no controller, byte 1 is 0
        - controller plugged in to port 1, byte 1 is 0x10
        - controller plugged in port 2, byte 10 is 0x10
        with both USB plugged in
        - no controller, byte 1 is 0x04
        - controller plugged in to port 1, byte is 0x14 */
        buffer[1] = 0x14;
        buffer[10] = 0x04;
        buffer[19] = 0x04;
        buffer[28] = 0x04;
        _gc_first = true;
    }
    else
    {
        memcpy(&buffer[2], &data, 8);
    }

    tud_ginput_report(0, buffer, 37);
}   


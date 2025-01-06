#include "usb/gcinput.h"

#include "input/button.h"
#include "input/analog.h"

#include "usb/ginput_usbd.h"

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))

/**--------------------------**/
/**--------------------------**/

bool _gc_first = false;
bool _gc_enable = false;

void gcinput_enable(bool enable)
{
    _gc_enable = enable;
}

void gcinput_hid_report(uint32_t timestamp)
{
    static gc_input_s data = {0};
    static uint8_t buffer[37] = {0};

    static button_data_s buttons = {0};
    static analog_data_s analog  = {0};

    // Update input data
    button_access_try(&buttons, BUTTON_ACCESS_REMAPPED_DATA);
    analog_access_try(&analog,  ANALOG_ACCESS_DEADZONE_DATA);

    buffer[0] = 0x21;

    const int32_t center_value = 128;
    const float   target_max = 110.0f / 2048.0f;
    const int32_t fixed_multiplier = (int32_t) (target_max * (1<<16));

    bool lx_sign = analog.lx < 0;
    bool ly_sign = analog.ly < 0;
    bool rx_sign = analog.rx < 0;
    bool ry_sign = analog.ry < 0;

    uint32_t lx_abs = lx_sign ? -analog.lx : analog.lx;
    uint32_t ly_abs = ly_sign ? -analog.ly : analog.ly;
    uint32_t rx_abs = rx_sign ? -analog.rx : analog.rx;
    uint32_t ry_abs = ry_sign ? -analog.ry : analog.ry;

    // Analog stick data conversion
    int32_t lx = ((lx_abs * fixed_multiplier) >> 16) * (lx_sign ? -1 : 1);
    int32_t ly = ((ly_abs * fixed_multiplier) >> 16) * (ly_sign ? -1 : 1);
    int32_t rx = ((rx_abs * fixed_multiplier) >> 16) * (rx_sign ? -1 : 1);
    int32_t ry = ((ry_abs * fixed_multiplier) >> 16) * (ry_sign ? -1 : 1);

    uint8_t lx8 = CLAMP_0_255(lx + center_value);
    uint8_t ly8 = CLAMP_0_255(ly + center_value);
    uint8_t rx8 = CLAMP_0_255(rx + center_value);
    uint8_t ry8 = CLAMP_0_255(ry + center_value);

    data.button_a = buttons.button_a;
    data.button_b = buttons.button_b;
    data.button_x = buttons.button_x;
    data.button_y = buttons.button_y;
    data.button_start = buttons.button_plus;

    data.button_l = buttons.trigger_zl;
    data.button_r = buttons.trigger_zr;
    data.button_z = buttons.trigger_r;

    data.stick_x    = lx8;
    data.stick_y    = ly8;
    data.cstick_x   = rx8;
    data.cstick_y   = ry8;

    data.dpad_down  = buttons.dpad_down;
    data.dpad_up    = buttons.dpad_up;
    data.dpad_left  = buttons.dpad_left;
    data.dpad_right = buttons.dpad_right;

    int outl = 0;
    int outr = 0;

    /*
    // Handle trigger SP stuff
    switch(global_loaded_settings.gc_sp_mode)
    {
        default:
            data.trigger_l = buttons.trigger_zl ? 255 : (buttons.zl_analog >> 4);
            data.trigger_r = buttons.trigger_zr ? 255 : (buttons.zr_analog >> 4);

            break;

        case GC_SP_MODE_LT:
            outl = buttons.trigger_l ? (global_loaded_settings.gc_sp_light_trigger) : 0;
            data.trigger_l = buttons.trigger_zl ? 255 : outl;

            data.trigger_r = buttons.zr_analog >> 4;
            data.trigger_r = buttons.trigger_zr ? 255 : data.trigger_r;

            break;

        case GC_SP_MODE_RT:
            outr = buttons.trigger_l ? (global_loaded_settings.gc_sp_light_trigger) : 0;
            data.trigger_r = buttons.trigger_zr ? 255 : outr;

            data.trigger_l = buttons.zl_analog >> 4;
            data.trigger_l = buttons.trigger_zl ? 255 : data.trigger_l;

            break;

        case GC_SP_MODE_TRAINING:
        
            data.trigger_l = buttons.zl_analog >> 4;
            data.trigger_l = buttons.trigger_zl ? 255 : data.trigger_l;

            data.trigger_r = buttons.zr_analog >> 4;
            data.trigger_r = buttons.trigger_zr ? 255 : data.trigger_r;

            if(buttons.trigger_l)
            {
                data.button_a = 1;
                data.trigger_l = 255;
                data.trigger_r = 255;
                data.button_l = 1;
                data.button_r = 1;
            }

            break;

        case GC_SP_MODE_DUALZ:
            data.button_z |= buttons.trigger_l;

            data.button_l = buttons.trigger_zl;
            data.button_r = buttons.trigger_zr;

            data.trigger_l = buttons.zl_analog >> 4;
            data.trigger_l = buttons.trigger_zl ? 255 : data.trigger_l;

            data.trigger_r = buttons.zr_analog >> 4;
            data.trigger_r = buttons.trigger_zr ? 255 : data.trigger_r;

          break;
    }
    */

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


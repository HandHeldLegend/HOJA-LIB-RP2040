#include "gcinput.h"

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))

/**--------------------------**/
/**--------------------------**/

bool _gc_first = false;
bool _gc_enable = false;

void gcinput_enable(bool enable)
{
    _gc_enable = enable;
}

void gcinput_hid_report(button_data_s *button_data, a_data_s *analog_data)
{
    static gc_input_s data = {0};
    static uint8_t buffer[37] = {0};

    buffer[0] = 0x21;

    float lx = (analog_data->lx*0.0488f) + 28;
    float ly = (analog_data->ly*0.0488f) + 28;
    float rx = (analog_data->rx*0.0488f) + 28;
    float ry = (analog_data->ry*0.0488f) + 28;

    data.button_a = button_data->button_a;
    data.button_b = button_data->button_b;
    data.button_x = button_data->button_x;
    data.button_y = button_data->button_y;
    data.button_start = button_data->button_plus;

    data.button_l = button_data->trigger_zl;
    data.button_r = button_data->trigger_zr;
    data.button_z = button_data->trigger_r;

    data.stick_x = CLAMP_0_255(lx);
    data.stick_y = CLAMP_0_255(ly);
    data.cstick_x = CLAMP_0_255(rx);
    data.cstick_y = CLAMP_0_255(ry);

    data.dpad_down  = button_data->dpad_down;
    data.dpad_up    = button_data->dpad_up;
    data.dpad_left  = button_data->dpad_left;
    data.dpad_right = button_data->dpad_right;

    int outl = 0;
    int outr = 0;

    // Handle trigger SP stuff
    switch(global_loaded_settings.gc_sp_mode)
    {
        default:

            data.trigger_l = button_data->zl_analog >> 4;
            data.trigger_r = button_data->zr_analog >> 4;

            break;

        case GC_SP_MODE_LT:
            outl = button_data->trigger_l ? (global_loaded_settings.gc_sp_light_trigger) : 0;
            data.trigger_l = button_data->trigger_zl ? 255 : outl;

            data.trigger_r = button_data->zr_analog >> 4;
            data.trigger_r = button_data->trigger_zr ? 255 : data.trigger_r;

            break;

        case GC_SP_MODE_RT:
            outr = button_data->trigger_l ? (global_loaded_settings.gc_sp_light_trigger) : 0;
            data.trigger_r = button_data->trigger_zr ? 255 : outr;

            data.trigger_l = button_data->zl_analog >> 4;
            data.trigger_l = button_data->trigger_zl ? 255 : data.trigger_l;

            break;

        case GC_SP_MODE_TRAINING:
        
            data.trigger_l = button_data->zl_analog >> 4;
            data.trigger_l = button_data->trigger_zl ? 255 : data.trigger_l;

            data.trigger_r = button_data->zr_analog >> 4;
            data.trigger_r = button_data->trigger_zr ? 255 : data.trigger_r;

            if(button_data->trigger_l)
            {
                data.button_a = 1;
                data.trigger_l = 255;
                data.trigger_r = 255;
                data.button_l = 1;
                data.button_r = 1;
            }

            break;

        case GC_SP_MODE_DUALZ:
            data.button_z |= button_data->trigger_l;

            data.button_l = button_data->trigger_zl;
            data.button_r = button_data->trigger_zr;

            data.trigger_l = button_data->zl_analog >> 4;
            data.trigger_l = button_data->trigger_zl ? 255 : data.trigger_l;

            data.trigger_r = button_data->zr_analog >> 4;
            data.trigger_r = button_data->trigger_zr ? 255 : data.trigger_r;

          break;
    }

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
    analog_send_reset();
}   


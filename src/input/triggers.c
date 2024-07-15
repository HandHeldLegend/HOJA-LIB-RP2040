#include "triggers.h"

float _lt_scaler = 1;
float _rt_scaler = 1;
bool _trigger_calibration = false;
#define TRIGGER_DEADZONE 100

void triggers_set_disabled(bool left_right, bool disabled)
{
    if(!left_right)
    {
        global_loaded_settings.trigger_l.disabled = disabled;
    }
    else global_loaded_settings.trigger_r.disabled = disabled;
}

void triggers_start_calibration()
{
    rgb_flash(COLOR_ORANGE.color, -1);
    global_loaded_settings.trigger_l.lower = 0x7FFF;
    global_loaded_settings.trigger_r.lower = 0x7FFF;
    global_loaded_settings.trigger_l.upper = 0;
    global_loaded_settings.trigger_r.upper = 0;
    _trigger_calibration = true;
}

void triggers_stop_calibration()
{
    rgb_init(global_loaded_settings.rgb_mode, -1);
    _trigger_calibration = false;
}

void triggers_scale(int left_trigger_in, int *left_trigger_out, int right_trigger_in, int *right_trigger_out)
{
    if(_trigger_calibration)
    {
        if( (left_trigger_in)  < (int)global_loaded_settings.trigger_l.lower) global_loaded_settings.trigger_l.lower = (uint16_t) left_trigger_in   & 0x7FFF;
        if( (right_trigger_in) < (int)global_loaded_settings.trigger_r.lower) global_loaded_settings.trigger_r.lower = (uint16_t) right_trigger_in  & 0x7FFF;

        if((uint16_t) left_trigger_in  > global_loaded_settings.trigger_l.upper) global_loaded_settings.trigger_l.upper = (uint16_t) left_trigger_in;
        if((uint16_t) right_trigger_in > global_loaded_settings.trigger_r.upper) global_loaded_settings.trigger_r.upper = (uint16_t) right_trigger_in;
        triggers_scale_init();
    }

    float lt = (float) left_trigger_in;
    float rt = (float) right_trigger_in;

    if(global_loaded_settings.trigger_l.disabled)
    *left_trigger_out = 0;
    else
    {
        // Zero out our trigger lower value
        lt -= ((float) global_loaded_settings.trigger_l.lower);
        lt *= _lt_scaler;

        // Cap our value
        lt = (lt<0) ? 0 : lt;
        lt = (lt>(float)0xFFF) ? (float) 0xFFF : lt;

        // Cast out properly
        *left_trigger_out = (int) lt;
    }

    if(global_loaded_settings.trigger_r.disabled)
    *right_trigger_out = 0;
    else
    {
        // Repeat for right trigger
        // Zero out our trigger lower value
        rt -= ((float) global_loaded_settings.trigger_r.lower);
        rt *= _rt_scaler;

        // Cap our value
        rt = (rt<0) ? 0 : rt;
        rt = (rt>(float)0xFFF) ? (float) 0xFFF : rt;

        // Cast out properly
        *right_trigger_out = (int) rt;
    }
}

void triggers_scale_init()
{
    int range = 0;

    // Calculate scalers based on calibration data
    // only if our upper value is greater than zero
    if(global_loaded_settings.trigger_l.upper>0)
    {
        // First calculate our full range
        range = global_loaded_settings.trigger_l.upper - (global_loaded_settings.trigger_l.lower + TRIGGER_DEADZONE);

        // We need to ensure our scale results in a full range of 0xFFF
        _lt_scaler = (float) 0xFFF / (float) range;
    }

    if(global_loaded_settings.trigger_r.upper>0)
    {
        // First calculate our full range
        range = global_loaded_settings.trigger_r.upper - (global_loaded_settings.trigger_r.lower + TRIGGER_DEADZONE);

        // We need to ensure our scale results in a full range of 0xFFF
        _rt_scaler = (float) 0xFFF / (float) range;
    }
}
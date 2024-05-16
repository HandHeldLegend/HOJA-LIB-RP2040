#include "triggers.h"

float _lt_scaler = 1;
float _rt_scaler = 1;
bool _trigger_calibration = false;
#define TRIGGER_DEADZONE 250

void triggers_start_calibration()
{
    rgb_flash(COLOR_ORANGE.color);
    global_loaded_settings.trigger_l_lower = 0xFFF;
    global_loaded_settings.trigger_r_lower = 0xFFF;
    global_loaded_settings.trigger_l_upper = 0;
    global_loaded_settings.trigger_r_upper = 0;
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

        if( (left_trigger_in+TRIGGER_DEADZONE)  < (int)global_loaded_settings.trigger_l_lower) global_loaded_settings.trigger_l_lower = (uint16_t) left_trigger_in+TRIGGER_DEADZONE;
        if( (right_trigger_in+TRIGGER_DEADZONE) < (int)global_loaded_settings.trigger_r_lower) global_loaded_settings.trigger_r_lower = (uint16_t) right_trigger_in+TRIGGER_DEADZONE;

        if((uint16_t) left_trigger_in  > global_loaded_settings.trigger_l_upper) global_loaded_settings.trigger_l_upper = (uint16_t) left_trigger_in;
        if((uint16_t) right_trigger_in > global_loaded_settings.trigger_r_upper) global_loaded_settings.trigger_r_upper = (uint16_t) right_trigger_in;
        triggers_scale_init();
    }

    float lt = (float) left_trigger_in;
    float rt = (float) right_trigger_in;

    // Zero out our trigger lower value
    lt -= ((float) global_loaded_settings.trigger_l_lower);
    lt *= _lt_scaler;

    // Cap our value
    lt = (lt<0) ? 0 : lt;
    lt = (lt>(float)0xFFF) ? (float) 0xFFF : lt;

    // Cast out properly
    *left_trigger_out = (int) lt;

    // Repeat for right trigger
    // Zero out our trigger lower value
    rt -= ((float) global_loaded_settings.trigger_r_lower);
    rt *= _rt_scaler;

    // Cap our value
    rt = (rt<0) ? 0 : rt;
    rt = (rt>(float)0xFFF) ? (float) 0xFFF : rt;

    // Cast out properly
    *right_trigger_out = (int) rt;
}

void triggers_scale_init()
{
    int range = 0;

    // Calculate scalers based on calibration data
    // only if our upper value is greater than zero
    if(global_loaded_settings.trigger_l_upper>0)
    {
        // First calculate our full range
        range = global_loaded_settings.trigger_l_upper - (global_loaded_settings.trigger_l_lower);

        // We need to ensure our scale results in a full range of 0xFFF
        _lt_scaler = (float) 0xFFF / (float) range;
    }

    if(global_loaded_settings.trigger_r_upper>0)
    {
        // First calculate our full range
        range = global_loaded_settings.trigger_r_upper - (global_loaded_settings.trigger_r_lower);

        // We need to ensure our scale results in a full range of 0xFFF
        _rt_scaler = (float) 0xFFF / (float) range;
    }
}
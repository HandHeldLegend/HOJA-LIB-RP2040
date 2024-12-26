#include "input/triggers.h"
#include <stddef.h>

#include "settings_shared_types.h"
#include "utilities/settings.h"

uint32_t _lt_scaler = 0;
uint32_t _rt_scaler = 0;
uint16_t _lt_deadzone = 0;
uint16_t _rt_deadzone = 0;
bool _trigger_calibration = false;

#define MAX_ANALOG_OUT 4095

// Calculate scaler when deadzone or range changes
// Returns 16.16 fixed point scaler
uint32_t _trigger_calculate_scaler(uint16_t deadzone, uint16_t max_value, uint16_t target_range) {
    uint32_t effective_range = max_value - deadzone;
    return ((uint32_t)target_range << 8) / effective_range;  // One-time expensive divide
}

// Fast real-time scaling using the pre-calculated scaler
int16_t _trigger_scale_input(int16_t input, uint16_t deadzone, uint32_t scaler) {
    if (input <= deadzone) return 0;

    uint32_t adjusted = (uint32_t) input - deadzone;
    return (int16_t) ((adjusted * scaler) >> 8);  // Just multiply and shift
}

void triggers_config_cmd(trigger_cmd_t cmd, command_confirm_t cb)
{
    switch(cmd)
    {
        default:
        break;

        case TRIGGER_CMD_REFRESH:
        break;
    }
}

bool triggers_init()
{
    _lt_deadzone = trigger_config->left_min + trigger_config->left_deadzone;
    _lt_scaler = _trigger_calculate_scaler(_lt_deadzone,
        trigger_config->left_max, MAX_ANALOG_OUT);

    _rt_deadzone = trigger_config->right_min + trigger_config->right_deadzone;
    _rt_scaler = _trigger_calculate_scaler(_rt_deadzone,
        trigger_config->right_max, MAX_ANALOG_OUT);

    return true;
}

/*
For Gamecube specific trigger modes
the L trigger (L1, LB) is used for our special function 
*/

// Everything is default
void _gc_mode_0()
{

}

// Left trigger function is split
void _gc_mode_1(button_data_s *safe)
{
    // Use ZL digital as our override
    if(safe->trigger_zl)
    {
        safe->zl_analog = MAX_ANALOG_OUT;
        return;
    }
    else 
    {
        if(safe->trigger_l)
            safe->zl_analog = trigger_config->left_static_output_value;
    }
}

// Right trigger function is split
void _gc_mode_2(button_data_s *safe)
{

}

// Dual Z
void _gc_mode_3(button_data_s *safe)
{

}

// Let's say our analog trigger is bound to another
// button such as ABXY (digital only)
// We can have our hairtrigger function
void _trigger_preprocess_rebound_l(button_data_s *safe)
{
    // We only care if analog function is NOT disabled
    // The remap function will handle the digital press
    if(!trigger_config->left_disabled)
    {   
        // Scale left trigger according to calibration params
        int16_t new_l = _trigger_scale_input(safe->zl_analog, _lt_deadzone, _lt_scaler);

        // Cap the output
        if(new_l > MAX_ANALOG_OUT) new_l = MAX_ANALOG_OUT;
        
        // Apply our hairpin for the digital press
        if(trigger_config->left_hairpin_value && (new_l >= trigger_config->left_hairpin_value))
            safe->trigger_zl |= 1;

        // Output analog trigger here is always
        // Zero'd out
        safe->zl_analog = 0;
    }
}

// Process a trigger where the mapping doesn't
// match. (A btn mapped to ZL as an example)
void _trigger_postprocess_rebound_l(button_data_s *safe)
{
    /* 
        With this safe data, the remap has already been completed.
        If our ZL button is pressed (A activating it), we should 
        apply our full analog output value, no matter if analog is disabled.
    */
    if(safe->trigger_zl)
        safe->zl_analog = MAX_ANALOG_OUT;
    else 
        safe->zl_analog = 0;
}

// Process a trigger where the mapping
// matches (ZL mapped to ZL)
void _trigger_postprocess_matching_l(button_data_s *safe)
{
    // Analog function for the trigger is disabled
    if(trigger_config->left_disabled)
    {   
        // Apply full analog value if digital is pressed
        if(safe->trigger_zl)
            safe->zl_analog = MAX_ANALOG_OUT;
        else 
            safe->zl_analog = 0;
    }
    else 
    {   
        // Scale left trigger according to calibration params
        int16_t new_l = _trigger_scale_input(safe->zl_analog, _lt_deadzone, _lt_scaler);

        // Cap the output
        if(new_l > MAX_ANALOG_OUT) new_l = MAX_ANALOG_OUT;
        
        // Apply our hairpin for the digital press
        if(trigger_config->left_hairpin_value && (new_l >= trigger_config->left_hairpin_value))
            safe->trigger_zl |= 1;

        // Apply full analog value if digital is pressed
        if(safe->trigger_zl)
            safe->zl_analog = MAX_ANALOG_OUT;
        else 
            // Forward our scaled analog signal
            safe->zl_analog = new_l;
    }
}

void _trigger_preprocess_rebound_r(button_data_s *safe)
{
    if(!trigger_config->right_disabled)
    {   
        int16_t new_r = _trigger_scale_input(safe->zr_analog, _rt_deadzone, _rt_scaler);
        if(new_r > MAX_ANALOG_OUT) new_r = MAX_ANALOG_OUT;
        
        if(trigger_config->right_hairpin_value && (new_r >= trigger_config->right_hairpin_value))
            safe->trigger_zr |= 1;

        safe->zr_analog = 0;
    }
}

void _trigger_postprocess_rebound_r(button_data_s *safe)
{
    if(safe->trigger_zr)
        safe->zr_analog = MAX_ANALOG_OUT;
    else 
        safe->zr_analog = 0;
}

void _trigger_postprocess_matching_r(button_data_s *safe)
{
    if(trigger_config->right_disabled)
    {   
        if(safe->trigger_zr)
            safe->zr_analog = MAX_ANALOG_OUT;
        else 
            safe->zr_analog = 0;
    }
    else 
    {   
        int16_t new_r = _trigger_scale_input(safe->zr_analog, _rt_deadzone, _rt_scaler);
        if(new_r > MAX_ANALOG_OUT) new_r = MAX_ANALOG_OUT;
        
        if(trigger_config->right_hairpin_value && (new_r >= trigger_config->right_hairpin_value))
            safe->trigger_zr |= 1;

        if(safe->trigger_zr)
            safe->zr_analog = MAX_ANALOG_OUT;
        else 
            safe->zr_analog = new_r;
    }
}

// Handle hair triggers basically
void triggers_process_pre(int l_type, int r_type, button_data_s *safe)
{
    if(l_type)
    {
        _trigger_preprocess_rebound_l(safe);
    }

    if(r_type)
    {
        _trigger_preprocess_rebound_r(safe);
    }
}

// Handle digital to analog mappings
void triggers_process_post(int l_type, int r_type, button_data_s *safe)
{
    if(l_type)
    {
        _trigger_postprocess_rebound_l(safe);
    }
    else _trigger_postprocess_matching_l(safe);

    if(r_type)
    {
        _trigger_postprocess_rebound_r(safe);
    }
    else _trigger_postprocess_matching_r(safe);
}

void triggers_process_gamecube(button_data_s *in, button_data_s *out)
{

}

#include "input/mapper.h"
#include "input_shared_types.h"
#include <stdlib.h>


typedef void (*mode_mapper_cb)(input_data_s *input);


mode_mapper_cb _mode_mapper_cb;
mapper_profile_s *_mapper_profile;

mapper_update_button_cb _mapper_button_updaters[MAPPER_CODE_MAX];
mapper_input_s _mapper_input = {0};
mapper_input_s _mapper_output = {0};

void _mode_mapper_switch(input_data_s *input)
{

}

void _mode_mapper_xinput(input_data_s *input)
{

}

void _mode_mapper_snes(input_data_s *input)
{

}

void _mode_mapper_n64(input_data_s *input)
{

}

void _mode_mapper_gamecube(input_data_s *input)
{

}

void mapper_init()
{
    // Clear all digital mapper pointers
    for(int i = 0; i < MAPPER_CODE_MAX; i++)
    {
        _mapper_button_updaters[i] = NULL;
    }
}

uint16_t _joystick_thresholds[8] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
uint16_t _trigger_thresholds[2] = {1000, 1000};

uint16_t _trigger_static[2] = {1000, 1000};

void mapper_task()
{
    int16_t lx_threshold = 1000;
    int16_t ly_threshold = 1000;
    int16_t rx_threshold = 1000;
    int16_t ry_threshold = 1000;
    uint16_t lt_threshold = 1000;
    uint16_t rt_threshold = 1000;

    uint16_t lx_static = 1000;
    uint16_t lt_static = 1000;
    uint16_t rt_static = 1000;

    mapper_input_s tmp;
    bool down;
    uint8_t map_digital_bitmask;
    uint8_t analog_idx_in;
    uint8_t analog_idx_out; 
    
    int8_t map_code_output;
    
    //
    // Digital Input
    //
    for(int i = 0; i < MAPPER_CODE_IDX_TRIGGER_START; i++)
    {
        // The OUTPUT value assigned to the input (i) value
        map_code_output = _mapper_profile->map[i];

        // Only process if this input is mapped to something
        if(map_code_output < 0) continue;

        map_digital_bitmask = (1 << i);
        down = (_mapper_input.digital_inputs & map_digital_bitmask) != 0;

        // Only process if the button is pressed
        if(!down) continue;

        switch(map_code_output)
        {
            default:
            // Do nothing
            break;

            // Digital Output
            case MAPPER_CODE_SOUTH ... MAPPER_CODE_RG_LOWER:
            tmp.digital_inputs |= (1 << map_code_output);
            break;

            // Trigger Output
            case MAPPER_CODE_LT_ANALOG ... MAPPER_CODE_RT_ANALOG:
            analog_idx_out = map_code_output - MAPPER_CODE_IDX_TRIGGER_START;
            if(_trigger_static[analog_idx_out] > tmp.triggers[analog_idx_out])
            {
                tmp.triggers[analog_idx_out] = _trigger_static[analog_idx_out];
            }
            break;

            // Joystick Output
            case MAPPER_CODE_LX_RIGHT ... MAPPER_CODE_RY_DOWN:
            analog_idx_out = map_code_output - MAPPER_CODE_IDX_JOYSTICK_START;
            tmp.joysticks_raw[analog_idx_out] |= 0xFFF;
            break;
        }
    }

    //
    // Analog Trigger Input
    // 
    for(int i = MAPPER_CODE_IDX_TRIGGER_START; i < MAPPER_CODE_IDX_JOYSTICK_START; i++)
    {
        // The OUTPUT value assigned to the input (i) value
        map_code_output = _mapper_profile->map[i];

        // Only process if this input is mapped to something
        if(map_code_output < 0) continue;

        // Obtain the index of our trigger input
        analog_idx_in = i - MAPPER_CODE_IDX_TRIGGER_START;

        switch(map_code_output)
        {
            default:
            // Do nothing
            break;

            // Digital Output
            case MAPPER_CODE_SOUTH ... MAPPER_CODE_RG_LOWER:
            if(_mapper_input.triggers[analog_idx_in] >= _trigger_thresholds[analog_idx_in])
            {
                tmp.digital_inputs |= (1 << map_code_output);
            }
            break;

            // Trigger Output
            case MAPPER_CODE_LT_ANALOG ... MAPPER_CODE_RT_ANALOG:
            analog_idx_out = map_code_output - MAPPER_CODE_IDX_TRIGGER_START;
            if(_mapper_input.triggers[analog_idx_in] > tmp.triggers[analog_idx_out])
            {
                tmp.triggers[analog_idx_out] = _mapper_input.triggers[analog_idx_in];
            }
            break;

            // Joystick Output
            case MAPPER_CODE_LX_RIGHT ... MAPPER_CODE_RY_DOWN:
            analog_idx_out = map_code_output - MAPPER_CODE_IDX_JOYSTICK_START;
            if(_mapper_input.triggers[analog_idx_in] > tmp.joysticks_raw[analog_idx_out])
            {
                tmp.joysticks_raw[analog_idx_out] = _mapper_input.triggers[analog_idx_in];
            }
            break;
        }
    }

    //
    // Analog Joystick Input
    //
    for (int i = MAPPER_CODE_IDX_JOYSTICK_START; i < MAPPER_CODE_MAX; i++)
    {
        // The OUTPUT value assigned to the input (i) value
        map_code_output = _mapper_profile->map[i];

        // Only process if this input is mapped to something
        if(map_code_output < 0) continue;

        // Obtain the index of our joystick input
        analog_idx_in = i - MAPPER_CODE_IDX_JOYSTICK_START;

        switch(map_code_output)
        {
            default:
            // Do nothing
            break;

            // Digital Output
            case MAPPER_CODE_SOUTH ... MAPPER_CODE_RG_LOWER:
            if(_mapper_input.joysticks_raw[analog_idx_in] >= _joystick_thresholds[analog_idx_in])
            {
                tmp.digital_inputs |= (1 << map_code_output);
            }
            break;

            // Trigger Output
            case MAPPER_CODE_LT_ANALOG ... MAPPER_CODE_RT_ANALOG:
            analog_idx_out = map_code_output - MAPPER_CODE_IDX_TRIGGER_START;
            if(_mapper_input.joysticks_raw[analog_idx_in] > tmp.triggers[analog_idx_out])
            {
                tmp.triggers[analog_idx_out] = _mapper_input.triggers[analog_idx_in];
            }
            break;

            // Joystick Output
            case MAPPER_CODE_LX_RIGHT ... MAPPER_CODE_RY_DOWN:
            analog_idx_out = map_code_output - MAPPER_CODE_IDX_JOYSTICK_START;
            if(_mapper_input.joysticks_raw[analog_idx_in] > tmp.joysticks_raw[analog_idx_out])
            {
                tmp.joysticks_raw[analog_idx_out] = _mapper_input.joysticks_raw[analog_idx_in];
            }
            break;
        }
    }
}

// Process all remaps
void mapper_process()
{
    // Clear all digital mapper pointers
    for(int i = 0; i < MAPPER_CODE_MAX; i++)
    {
        bool down = ((_mapper_input.digital_inputs >> i) & 0x1) != 0;
        if(_mapper_button_updaters[i]) _mapper_button_updaters[i](down);
    }
}

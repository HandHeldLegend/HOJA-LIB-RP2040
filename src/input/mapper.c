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

typedef union 
{
    struct 
    {
        uint8_t digital_in : 1;
        uint8_t digital_out : 1;
        uint8_t joystick_in : 1;
        uint8_t joystick_out : 1;
        uint8_t trigger_in : 1;
        uint8_t trigger_out : 1;
        uint8_t reserved : 2;
    };
    uint8_t value;
} mapper_remap_type_u;

uint16_t _joystick_thresholds[8] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
uint16_t _trigger_thresholds[2] = {1000, 1000};

void mapper_task()
{
    int16_t lx_threshold = 1000;
    int16_t ly_threshold = 1000;
    int16_t rx_threshold = 1000;
    int16_t ry_threshold = 1000;
    uint16_t lt_threshold = 1000;
    uint16_t rt_threshold = 1000;

    mapper_input_s tmp;

    for(int i = 0; i < MAPPER_CODE_MAX; i++)
    {
        bool down;
        uint8_t analog_idx_in;
        uint8_t analog_idx_out;

        // The bitmask of our digital input
        uint8_t map_digital_bitmask = (1 << i);
        // The OUTPUT value assigned to the input (i) value
        int8_t map_code_output = _mapper_profile->map[i];
        mapper_remap_type_u type = {0};

        switch(i)
        {
            default:
            break;

            case MAPPER_CODE_SOUTH ... MAPPER_CODE_RG_LOWER:
            type.digital_in = 1;
            break;

            case MAPPER_CODE_LT_ANALOG ... MAPPER_CODE_RT_ANALOG:
            type.trigger_in = 1;
            break;

            case MAPPER_CODE_LX_RIGHT ... MAPPER_CODE_RY_DOWN:
            type.joystick_in = 1;
            break;
        }

        switch(map_code_output)
        {
            default:
            break;

            case MAPPER_CODE_SOUTH ... MAPPER_CODE_RG_LOWER:
            type.digital_out = 1;
            break;

            case MAPPER_CODE_LT_ANALOG ... MAPPER_CODE_RT_ANALOG:
            type.trigger_out = 1;
            break;

            case MAPPER_CODE_LX_RIGHT ... MAPPER_CODE_RY_DOWN:
            type.joystick_out = 1;
            break;
        }

        switch(type.value)
        {
            default:
            break;

            // Digital -> Digital
            case 0x03:
            down = (_mapper_input.digital_inputs & map_digital_bitmask) != 0;

            if(!down) break;

            tmp.digital_inputs |= (1 << map_code_output);
            break;

            // Digital -> Joystick
            case 0x09:
            down = (_mapper_input.digital_inputs & map_digital_bitmask) != 0;

            if(!down) break;

            analog_idx_out = (map_code_output-MAPPER_CODE_IDX_JOYSTICK_START);

            if(_joystick_thresholds[analog_idx_out] > tmp.joysticks_raw[analog_idx_out])
            {
                tmp.joysticks_raw[analog_idx_out] = _joystick_thresholds[analog_idx_out];
            }
            break;

            // Digital -> Trigger
            case 0x21:
            down = (_mapper_input.digital_inputs & map_digital_bitmask) != 0;

            if(!down) break;

            analog_idx_out = (map_code_output-MAPPER_CODE_IDX_TRIGGER_START);

            if(_trigger_thresholds[analog_idx_out] > tmp.triggers[analog_idx_out])
            {
                tmp.triggers[analog_idx_out] = _trigger_thresholds[analog_idx_out];
            }
            break;

            // Joystick -> Digital
            case 0x06:
            analog_idx_in = (i-MAPPER_CODE_IDX_JOYSTICK_START);
            down = (_mapper_input.joysticks_raw[analog_idx_in] >= _joystick_thresholds[analog_idx_in]);

            if(!down) break;

            tmp.digital_inputs |= (1 << map_code_output);
            break;

            // Joystick -> Joystick
            case 0x0C:
            analog_idx_in = (i-MAPPER_CODE_IDX_JOYSTICK_START);
            analog_idx_out = (map_code_output-MAPPER_CODE_IDX_JOYSTICK_START);

            if(_mapper_input.joysticks_raw[analog_idx_in] > tmp.joysticks_raw[analog_idx_out])
            {
                tmp.joysticks_raw[analog_idx_out] = _mapper_input.joysticks_raw[analog_idx_in];
            }
            break;

            // Joystick -> Trigger
            case 0x24:
            analog_idx_in = (i-MAPPER_CODE_IDX_JOYSTICK_START);
            analog_idx_out = (map_code_output-MAPPER_CODE_IDX_TRIGGER_START);

            if(_mapper_input.joysticks_raw[analog_idx_in] > tmp.triggers[analog_idx_out])
            {
                tmp.triggers[analog_idx_out] = _mapper_input.joysticks_raw[analog_idx_in];
            }
            break;

            // Trigger -> Digital
            case 0x12:
            break;

            // Trigger -> Joystick
            case 0x18:
            break;

            // Trigger -> Trigger
            case 0x30:
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

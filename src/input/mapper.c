#include "input/mapper.h" 
#include "input_shared_types.h"

#include "input/analog.h"
#include "input/trigger.h"
#include "input/button.h"

#include "utilities/settings.h"

#include "utilities/pcm.h"

#include "hoja.h"

#include <stdlib.h>
#include <string.h>

const inputInfoSlot_s _infoslots[MAPPER_INPUT_COUNT] = HOJA_INPUT_SLOTS;

typedef struct 
{
    int8_t input_type;
    int8_t output_type;
    int8_t output_code;
    int8_t output_mode;
    uint16_t threshold_delta;
    uint16_t static_output;
    uint16_t rapid_value;
    bool rapid_press_state;
} mapper_loaded_config_s;

input_cfg_s *_loaded_input_config = NULL;
mapper_loaded_config_s _loaded_mapper_configs[MAPPER_INPUT_COUNT] = {0};
mapper_input_type_t *_loaded_output_types = NULL;
uint8_t _loaded_output_types_max = 0;


typedef mapper_input_s(*mapper_input_cb)(void);

mapper_input_cb _mapper_input_cb;

mapper_input_s _mapper_input = {0};
nu_mapper_input_s _all_inputs = {0};
nu_mapper_input_s _all_outputs = {0};

mapper_output_cfg_s _output_cfgs[MAPPER_INPUT_COUNT] = {};
uint16_t _rapid_vals[MAPPER_INPUT_COUNT] = {0};
bool _rapid_press_state[MAPPER_INPUT_COUNT] = {0};

void _handle_analog_compare(uint16_t *input_modifiable, uint16_t new_value)
{
    if(*input_modifiable < new_value)
    {
        *input_modifiable = new_value;
    }
}

static inline bool _handle_analog_to_digital(uint16_t input, uint8_t mode, uint16_t param, uint16_t *rapid_val, bool *press)
{
    bool ret = *press;

    switch(mode)
    {
        default:
        case MAPPER_OUTPUT_MODE_DEFAULT:
        case MAPPER_OUTPUT_MODE_RAPID:
        if(input >= *rapid_val)
        {
            ret = true;

            *rapid_val = input;

            if(*rapid_val < param) *rapid_val = param;
        }
        else if (input < (*rapid_val - param))
        {
            ret = false;

            *rapid_val = input + param;
        }
        *press = ret;
        break;

        case MAPPER_OUTPUT_MODE_THRESHOLD:
        ret = (input >= param);
        break;
    }

    return ret;
}

static inline uint16_t _handle_analog_to_analog(uint16_t input, uint8_t mode, uint16_t static_output, uint16_t param, uint16_t *rapid_val, bool *press)
{
    bool ret = *press;

    switch(mode)
    {
        default:
        case MAPPER_OUTPUT_MODE_DEFAULT:
        return input;
        break;

        case MAPPER_OUTPUT_MODE_RAPID:
        if(input >= *rapid_val)
        {
            ret = true;

            *rapid_val = input;

            if(*rapid_val < param) *rapid_val = param;
        }
        else if (input < (*rapid_val - param))
        {
            ret = false;

            *rapid_val = input + param;
        }
        *press = ret;
        break;

        case MAPPER_OUTPUT_MODE_THRESHOLD:
        ret = (input >= param);
        break;
    }

    return ret ? static_output : 0;
}

mapper_output_s _mapper_task_switch_nu()
{
    // Temporary store for new output data
    nu_mapper_input_s tmp = {0};

    for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
    {
        // Grab the configuration for this input
        mapper_loaded_config_s  *cfg = &(_loaded_mapper_configs[i]);

        // The current input value
        uint16_t *input = &_all_inputs.inputs[i];
        uint16_t *output = &tmp.inputs[i];

        // Skip if unmapped or unused
        if(cfg->output_code < 0) continue;

        // We perform different operation types dependent
        // on the type of INPUT (Digital, Hover, Joystick)
        // Joystick and Hover are both analog, but a Joystick is half of the resolution as a Hover (Half of 12 bits)
        switch(cfg->input_type)
        {
            case MAPPER_INPUT_TYPE_DIGITAL:
            {
                // Only process if the button is pressed
                if (*input == 0) continue;

                switch(cfg->output_type)
                {
                    default:
                    // Do nothing
                    break;

                    // The simplest output, just pass through using the output mapcode
                    case MAPPER_OUTPUT_DIGITAL:
                    *output |= 1;
                    break;

                    // Output to our virtual analog dpad
                    // which will be translated later
                    case MAPPER_OUTPUT_DPAD_UP ... MAPPER_OUTPUT_DPAD_RIGHT:
                    _handle_analog_compare(output, 2047);
                    break;

                    // Output to our triggers using the configured static output value
                    // for this particular input
                    case MAPPER_OUTPUT_TRIGGER_L ... MAPPER_OUTPUT_TRIGGER_R:
                    _handle_analog_compare(output, cfg->static_output);
                    break;

                    // Output to our joystick using the configured static output value (divided by 2)
                    case MAPPER_OUTPUT_LX_RIGHT ... MAPPER_OUTPUT_RY_DOWN:
                    _handle_analog_compare(output, cfg->static_output>>1);
                    break;
                }
            }
            break;

            case MAPPER_INPUT_TYPE_HOVER:
            {
                switch(cfg->output_type)
                {
                    default:
                    // Do nothing
                    break;

                    case MAPPER_OUTPUT_DIGITAL:
                    *output |= (uint16_t) _handle_analog_to_digital(input, cfg->output_mode, cfg->threshold_delta, 
                        &cfg->rapid_value, &cfg->rapid_press_state);
                    break;

                    // Output to our virtual analog dpad
                    // which will be translated later
                    case MAPPER_OUTPUT_DPAD_UP ... MAPPER_OUTPUT_DPAD_RIGHT:
                    
                    *output = _handle_analog_to_analog(input, 
                        cfg->output_mode, 0xFFF, cfg->threshold_delta,
                        &cfg->rapid_value, &cfg->rapid_press_state) >> 1; // Divide by 2 (hover -> dpad)
                    break;

                    // Output to our triggers using the configured static output value
                    // for this particular input
                    case MAPPER_OUTPUT_TRIGGER_L ... MAPPER_OUTPUT_TRIGGER_R:
                    
                    *output = _handle_analog_to_analog(input, 
                        cfg->output_mode, cfg->static_output, cfg->threshold_delta,
                        &cfg->rapid_value, &cfg->rapid_press_state);
                    break;

                    // Output to our joystick using the configured static output value (divided by 2)
                    case MAPPER_OUTPUT_LX_RIGHT ... MAPPER_OUTPUT_RY_DOWN:

                    *output = _handle_analog_to_analog(input, 
                        cfg->output_mode, cfg->static_output, cfg->threshold_delta,
                        &cfg->rapid_value, &cfg->rapid_press_state) >> 1; // Divide by 2 (hover -> joystick)
                    break;
                }
            }
            break;

            case MAPPER_INPUT_TYPE_JOYSTICK:
            {
                switch(cfg->output_type)
                {
                    default:
                    // Do nothing
                    break;

                    case MAPPER_OUTPUT_DIGITAL:
                    *output |= (uint16_t) _handle_analog_to_digital(*input<<1, cfg->output_mode, cfg->threshold_delta, 
                        &cfg->rapid_value, &cfg->rapid_press_state);
                    break;

                    case MAPPER_OUTPUT_DPAD_UP ... MAPPER_OUTPUT_DPAD_RIGHT:
                    *output = _handle_analog_to_analog(*input << 1, 
                        cfg->output_mode, 0xFFF, cfg->threshold_delta,
                        &cfg->rapid_value, &cfg->rapid_press_state) >> 1; 
                    break;

                    case MAPPER_OUTPUT_TRIGGER_L ... MAPPER_OUTPUT_TRIGGER_R:
                    *output = _handle_analog_to_analog(*input << 1, 
                        cfg->output_mode, cfg->static_output, cfg->threshold_delta,
                        &cfg->rapid_value, &cfg->rapid_press_state); 
                    break;

                    case MAPPER_OUTPUT_LX_RIGHT ... MAPPER_OUTPUT_RY_DOWN:
                    *output = _handle_analog_to_analog(*input << 1, 
                        cfg->output_mode, cfg->static_output, cfg->threshold_delta,
                        &cfg->rapid_value, &cfg->rapid_press_state) >> 1; 
                    break;
                }
            }
            break;

            default:
            case MAPPER_INPUT_TYPE_UNUSED:
            break;
        }
    }

    return tmp;
}



static inline void _set_joystick_axis(uint16_t *pos, uint16_t *neg, int16_t value) {
    if (value > 0) {
        *pos = value;
        *neg = 0;
    } else if (value < 0) {
        *pos = 0;
        *neg = (uint16_t)(-value);
    } else {
        *pos = 0;
        *neg = 0;
    }
}

void _set_mapper_defaults()
{
    //memcpy(remap_config->remap_profile_switch, (uint8_t *)&_map_default_switch, sizeof(_map_default_switch));
    //memcpy(remap_config->remap_profile_snes, (uint8_t *)&_map_default_snes, sizeof(_map_default_snes));
    //memcpy(remap_config->remap_profile_n64, (uint8_t *)&_map_default_n64, sizeof(_map_default_n64));
    //memcpy(remap_config->remap_profile_gamecube, (uint8_t *)&_map_default_gamecube, sizeof(_map_default_gamecube));
    //memcpy(remap_config->remap_profile_xinput, (uint8_t *)&_map_default_xinput, sizeof(_map_default_xinput));
}

//void mapper_config_command(remap_cmd_t cmd, webreport_cmd_confirm_t cb)
//{
//    switch(cmd)
//    {
//        default:
//        cb(CFG_BLOCK_REMAP, cmd, false, NULL, 0);
//        break;
//
//        case REMAP_CMD_DEFAULT:
//            _set_mapper_defaults();
//            cb(CFG_BLOCK_REMAP, cmd, true, NULL, 0);
//        break;
//    }  
//}

static uint8_t _lhapticmode = 0;
static uint8_t _rhapticmode = 0;

typedef enum 
{
    MAPPER_HAPTIC_MODE_DISABLE,
    MAPPER_HAPTIC_MODE_DIGITAL,
    MAPPER_HAPTIC_MODE_ANALOG,
} mapper_haptic_mode_t;

void mapper_init()
{
    switch(hoja_get_status().gamepad_mode)
    {
        default:
        case GAMEPAD_MODE_SWPRO:
        _loaded_input_config = input_config->input_profile_switch;
        _loaded_output_types = _switch_output_types;
        _loaded_output_types_max = SWITCH_CODE_MAX;
        break;

        case GAMEPAD_MODE_GAMECUBE:
        case GAMEPAD_MODE_GCUSB:
        _loaded_input_config = input_config->input_profile_gamecube;
        _loaded_output_types = _gamecube_output_types;
        _loaded_output_types_max = GAMECUBE_CODE_MAX;
        break;

        case GAMEPAD_MODE_SNES:
        _loaded_input_config = input_config->input_profile_snes;
        _loaded_output_types = _snes_output_types;
        _loaded_output_types_max = SNES_CODE_MAX;
        break;

        case GAMEPAD_MODE_N64:
        _loaded_input_config = input_config->input_profile_n64;
        _loaded_output_types = _n64_output_types;
        _loaded_output_types_max = N64_CODE_MAX;
        break;

        case GAMEPAD_MODE_XINPUT:
        _loaded_input_config = input_config->input_profile_xinput;
        _loaded_output_types = _xinput_output_types;
        _loaded_output_types_max = XINPUT_CODE_MAX;
        break;

        //case GAMEPAD_MODE_SINPUT:
        //_loaded_config = input_config->input_profile_reserved_1;
        //break;
    }

    // Load our current profile data into 
    // active memory for processing
    for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
    {
        _loaded_mapper_configs[i].input_type = _infoslots[i].input_type;
        _loaded_mapper_configs[i].output_code = _loaded_input_config[i].output_code;
        _loaded_mapper_configs[i].output_mode = _loaded_input_config[i].output_mode;
        _loaded_mapper_configs[i].output_type = _loaded_output_types[_loaded_input_config[i].output_code];
        _loaded_mapper_configs[i].rapid_press_state = false;
        _loaded_mapper_configs[i].rapid_value = _loaded_input_config[i].threshold_delta;
        _loaded_mapper_configs[i].threshold_delta = _loaded_input_config[i].threshold_delta;
        _loaded_mapper_configs[i].static_output = _loaded_input_config[i].static_output;
    }
    
}

mapper_input_s* mapper_get_input()
{
    // Get latest inputs
    static analog_data_s joysticks;
    static button_data_s buttons;
    static trigger_data_s triggers;
    static mapper_input_s output;

    // Get joysticks
    analog_access_safe(&joysticks, ANALOG_ACCESS_DEADZONE_DATA);

    // Get triggers
    trigger_access_safe(&triggers, TRIGGER_ACCESS_SCALED_DATA);

    // Get buttons
    button_access_safe(&buttons, BUTTON_ACCESS_RAW_DATA);

    // Formulate mapper input
    _mapper_input.digital_inputs = buttons.buttons;

    // Set mapper joystick input
    _set_joystick_axis(&_mapper_input.joysticks_raw[0], &_mapper_input.joysticks_raw[1], joysticks.lx);
    _set_joystick_axis(&_mapper_input.joysticks_raw[2], &_mapper_input.joysticks_raw[3], joysticks.ly);
    _set_joystick_axis(&_mapper_input.joysticks_raw[4], &_mapper_input.joysticks_raw[5], joysticks.rx);
    _set_joystick_axis(&_mapper_input.joysticks_raw[6], &_mapper_input.joysticks_raw[7], joysticks.ry);

    // Set mapper trigger input
    //_mapper_input.triggers[0] = triggers.left_analog;
    //_mapper_input.triggers[1] = triggers.right_analog;

    bool lbump = false;
    bool rbump = false;

    switch(_lhapticmode)
    {
        case 0: // No trigger haptics
        break;

        case 1: // Haptics only on digital press
        lbump = (buttons.trigger_zl != 0);
        break;

        case 2: // Haptics only on trigger threshold met
        //lbump = (triggers.left_analog >= *_trigger_thresholds[0]);
        break;
    }

    switch(_rhapticmode)
    {
        case 0: // No trigger haptics
        break;

        case 1: // Haptics only on digital press
        rbump = (buttons.trigger_zr != 0);
        break;

        case 2: // Haptics only on trigger threshold met
        //rbump = (triggers.right_analog >= *_trigger_thresholds[1]);
        break;
    }

    pcm_play_bump(lbump, rbump);

    //if(_mapper_input_cb != NULL && _mapper_profile != NULL)
    //{
    //    output = _mapper_input_cb();
    //}
    //else 
    //{
    //    output = _mapper_input;
    //}

    // Combine joystick input
    output.joysticks_combined[0] = output.joysticks_raw[0] - output.joysticks_raw[1];
    output.joysticks_combined[1] = output.joysticks_raw[2] - output.joysticks_raw[3];
    output.joysticks_combined[2] = output.joysticks_raw[4] - output.joysticks_raw[5];
    output.joysticks_combined[3] = output.joysticks_raw[6] - output.joysticks_raw[7];

    // Always forward system buttons
    output.buttons_system = buttons.buttons_system;

    // Return output pointer
    return &output;
}

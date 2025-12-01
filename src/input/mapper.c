#include "board_config.h"
#include "input/mapper.h" 
#include "input_shared_types.h"


#include "input/analog.h"

#include "utilities/settings.h"
#include "utilities/pcm.h"
#include "utilities/crosscore_snapshot.h"

#include "hoja.h"

#include <stdlib.h>
#include <string.h>

mapper_output_type_t _switch_output_types[SWITCH_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // X
    MAPPER_OUTPUT_DIGITAL, // Y
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_DIGITAL, // L
    MAPPER_OUTPUT_DIGITAL, // R
    MAPPER_OUTPUT_DIGITAL, // LZ
    MAPPER_OUTPUT_DIGITAL, // RZ
    MAPPER_OUTPUT_DIGITAL, // PLUS
    MAPPER_OUTPUT_DIGITAL, // MINUS
    MAPPER_OUTPUT_DIGITAL, // HOME
    MAPPER_OUTPUT_DIGITAL, // CAPTURE
    MAPPER_OUTPUT_DIGITAL, // LS Click
    MAPPER_OUTPUT_DIGITAL, // RS Click
    MAPPER_OUTPUT_LX_RIGHT,
    MAPPER_OUTPUT_LX_LEFT,
    MAPPER_OUTPUT_LY_UP,
    MAPPER_OUTPUT_LY_DOWN, 
    MAPPER_OUTPUT_RX_RIGHT,
    MAPPER_OUTPUT_RX_LEFT,
    MAPPER_OUTPUT_RY_UP,
    MAPPER_OUTPUT_RY_DOWN
};

mapper_output_type_t _snes_output_types[SNES_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // X
    MAPPER_OUTPUT_DIGITAL, // Y
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_DIGITAL, // L
    MAPPER_OUTPUT_DIGITAL, // R
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_DIGITAL, // Select
};

mapper_output_type_t _n64_output_types[SWITCH_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // CUP
    MAPPER_OUTPUT_DIGITAL, // CDOWN
    MAPPER_OUTPUT_DIGITAL, // CLEFT
    MAPPER_OUTPUT_DIGITAL, // CRIGHT
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_DIGITAL, // L
    MAPPER_OUTPUT_DIGITAL, // R
    MAPPER_OUTPUT_DIGITAL, // Z
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_LX_RIGHT,
    MAPPER_OUTPUT_LX_LEFT,
    MAPPER_OUTPUT_LY_UP,
    MAPPER_OUTPUT_LY_DOWN, 
};

mapper_output_type_t _gamecube_output_types[GAMECUBE_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // X
    MAPPER_OUTPUT_DIGITAL, // Y
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_DIGITAL, // Z
    MAPPER_OUTPUT_DIGITAL, // L
    MAPPER_OUTPUT_DIGITAL, // R
    MAPPER_OUTPUT_TRIGGER_L, // L Analog
    MAPPER_OUTPUT_TRIGGER_R, // R Analog
    MAPPER_OUTPUT_LX_RIGHT,
    MAPPER_OUTPUT_LX_LEFT,
    MAPPER_OUTPUT_LY_UP,
    MAPPER_OUTPUT_LY_DOWN, 
    MAPPER_OUTPUT_RX_RIGHT,
    MAPPER_OUTPUT_RX_LEFT,
    MAPPER_OUTPUT_RY_UP,
    MAPPER_OUTPUT_RY_DOWN
};

mapper_output_type_t _xinput_output_types[XINPUT_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // X
    MAPPER_OUTPUT_DIGITAL, // Y
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_DIGITAL, // LB
    MAPPER_OUTPUT_DIGITAL, // RB
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_DIGITAL, // Back
    MAPPER_OUTPUT_DIGITAL, // Guide
    MAPPER_OUTPUT_DIGITAL, // LS
    MAPPER_OUTPUT_DIGITAL, // RS
    MAPPER_OUTPUT_TRIGGER_L,
    MAPPER_OUTPUT_TRIGGER_R,
    MAPPER_OUTPUT_LX_RIGHT,
    MAPPER_OUTPUT_LX_LEFT,
    MAPPER_OUTPUT_LY_UP,
    MAPPER_OUTPUT_LY_DOWN,
    MAPPER_OUTPUT_RX_RIGHT,
    MAPPER_OUTPUT_RX_LEFT,
    MAPPER_OUTPUT_RY_UP,
    MAPPER_OUTPUT_RY_DOWN,
};

mapper_output_type_t _sinput_output_types[SINPUT_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // SOUTH
    MAPPER_OUTPUT_DIGITAL, // EAST
    MAPPER_OUTPUT_DIGITAL, // WEST
    MAPPER_OUTPUT_DIGITAL, // NORTH
    MAPPER_OUTPUT_DPAD_UP,
    MAPPER_OUTPUT_DPAD_DOWN,
    MAPPER_OUTPUT_DPAD_LEFT,
    MAPPER_OUTPUT_DPAD_RIGHT,
    MAPPER_OUTPUT_DIGITAL, // Stick left
    MAPPER_OUTPUT_DIGITAL, // Stick right
    MAPPER_OUTPUT_DIGITAL, // Left bumper
    MAPPER_OUTPUT_DIGITAL, // Right bumper
    MAPPER_OUTPUT_DIGITAL, // Left trigger digital
    MAPPER_OUTPUT_DIGITAL, // Right trigger digital
    MAPPER_OUTPUT_DIGITAL, // Left paddle 1
    MAPPER_OUTPUT_DIGITAL, // Right paddle 1
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_DIGITAL, // Select
    MAPPER_OUTPUT_DIGITAL, // Guide
    MAPPER_OUTPUT_DIGITAL, // Share
    MAPPER_OUTPUT_DIGITAL, // Left paddle 2
    MAPPER_OUTPUT_DIGITAL, // Right paddle 2
    MAPPER_OUTPUT_DIGITAL, // Touchpad 1
    MAPPER_OUTPUT_DIGITAL, // Touchpad 2
    MAPPER_OUTPUT_DIGITAL, // Misc 3
    MAPPER_OUTPUT_DIGITAL, // Misc 4
    MAPPER_OUTPUT_DIGITAL, // Misc 5
    MAPPER_OUTPUT_DIGITAL, // Misc 6 
    MAPPER_OUTPUT_TRIGGER_L,
    MAPPER_OUTPUT_TRIGGER_R,
    MAPPER_OUTPUT_LX_RIGHT,
    MAPPER_OUTPUT_LX_LEFT,
    MAPPER_OUTPUT_LY_UP,
    MAPPER_OUTPUT_LY_DOWN,
    MAPPER_OUTPUT_RX_RIGHT,
    MAPPER_OUTPUT_RX_LEFT,
    MAPPER_OUTPUT_RY_UP,
    MAPPER_OUTPUT_RY_DOWN,
};

const inputInfoSlot_s _infoslots[MAPPER_INPUT_COUNT] = HOJA_INPUT_SLOTS;

typedef enum 
{
    MAPPER_OUTPUT_MODE_DEFAULT,
    MAPPER_OUTPUT_MODE_RAPID,
    MAPPER_OUTPUT_MODE_THRESHOLD,
} mapper_output_mode_t;

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

inputConfigSlot_s *_loaded_input_config = NULL;
mapper_loaded_config_s _loaded_mapper_configs[MAPPER_INPUT_COUNT] = {0};
mapper_output_type_t *_loaded_output_types = NULL;
uint8_t _loaded_output_types_max = 0;

SNAPSHOT_TYPE(mapper, mapper_input_s);
snapshot_mapper_t _mapper_snap_in;
snapshot_mapper_t _mapper_snap_out;

mapper_input_s _all_inputs = {0};

void _handle_analog_compare(uint16_t *input_modifiable, uint16_t new_value)
{
    if(*input_modifiable < new_value)
    {
        *input_modifiable = new_value;
    }
}

uint16_t mapper_joystick_concat(uint16_t center, uint16_t neg, uint16_t pos)
{
    neg = (neg > center) ? center : neg;
    pos = (pos > center) ? center : pos;

    return (center - neg) + pos;
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

mapper_input_s _mapper_operation()
{
    // Temporary store for new output data
    mapper_input_s tmp = {0};

    for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
    {
        // Grab the configuration for this input
        mapper_loaded_config_s *cfg = &(_loaded_mapper_configs[i]);

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
                    _handle_analog_compare(output, cfg->static_output >> 1);
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
                    *output |= (uint16_t) _handle_analog_to_digital(*input, cfg->output_mode, cfg->threshold_delta, 
                        &cfg->rapid_value, &cfg->rapid_press_state);
                    break;

                    // Output to our virtual analog dpad
                    // which will be translated later
                    case MAPPER_OUTPUT_DPAD_UP ... MAPPER_OUTPUT_DPAD_RIGHT:
                    
                    *output = _handle_analog_to_analog(*input, 
                        cfg->output_mode, 0xFFF, cfg->threshold_delta,
                        &cfg->rapid_value, &cfg->rapid_press_state) >> 1; // Divide by 2 (hover -> dpad)
                    break;

                    // Output to our triggers using the configured static output value
                    // for this particular input
                    case MAPPER_OUTPUT_TRIGGER_L ... MAPPER_OUTPUT_TRIGGER_R:
                    
                    *output = _handle_analog_to_analog(*input, 
                        cfg->output_mode, cfg->static_output, cfg->threshold_delta,
                        &cfg->rapid_value, &cfg->rapid_press_state);
                    break;

                    // Output to our joystick using the configured static output value (divided by 2)
                    case MAPPER_OUTPUT_LX_RIGHT ... MAPPER_OUTPUT_RY_DOWN:

                    *output = _handle_analog_to_analog(*input, 
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
                    *output |= (uint16_t) _handle_analog_to_digital(*input << 1, cfg->output_mode, cfg->threshold_delta, 
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

void mapper_config_command(mapper_cmd_t cmd, webreport_cmd_confirm_t cb)
{
    switch(cmd)
    {
        default:
        cb(CFG_BLOCK_INPUT, cmd, false, NULL, 0);
        break;

        case MAPPER_CMD_DEFAULT:
            _set_mapper_defaults();
            cb(CFG_BLOCK_INPUT, cmd, true, NULL, 0);
        break;
    }  
}

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

// Thread safe access mapper outputs (Post-remapping)
void mapper_access_output_safe(mapper_input_s *out)
{
    snapshot_mapper_read(&_mapper_snap_out, out);
}

// Thread safe access mapper inputs (Post-scaling, Pre-remapping)
void mapper_access_input_safe(mapper_input_s *out)
{
    snapshot_mapper_read(&_mapper_snap_in, out);
}

mapper_input_s mapper_get_input()
{
    static mapper_input_s output;

    // Return output pointer
    return output;
}

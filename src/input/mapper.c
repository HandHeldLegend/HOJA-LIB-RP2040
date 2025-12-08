#include "board_config.h"
#include "input/mapper.h" 
#include "input/hover.h"
#include "input/analog.h"
#include "input_shared_types.h"
#include "utilities/static_config.h"

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
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
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
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK, 
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK
};

mapper_output_type_t _snes_output_types[SNES_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // X
    MAPPER_OUTPUT_DIGITAL, // Y
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
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
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DIGITAL, // L
    MAPPER_OUTPUT_DIGITAL, // R
    MAPPER_OUTPUT_DIGITAL, // Z
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK, 
};

mapper_output_type_t _gamecube_output_types[GAMECUBE_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // X
    MAPPER_OUTPUT_DIGITAL, // Y
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_DIGITAL, // Z
    MAPPER_OUTPUT_DIGITAL, // L
    MAPPER_OUTPUT_DIGITAL, // R
    MAPPER_OUTPUT_HOVER, // L Analog
    MAPPER_OUTPUT_HOVER, // R Analog
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK, 
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK
};

mapper_output_type_t _xinput_output_types[XINPUT_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // A
    MAPPER_OUTPUT_DIGITAL, // B
    MAPPER_OUTPUT_DIGITAL, // X
    MAPPER_OUTPUT_DIGITAL, // Y
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DPAD,
    MAPPER_OUTPUT_DIGITAL, // LB
    MAPPER_OUTPUT_DIGITAL, // RB
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_DIGITAL, // Back
    MAPPER_OUTPUT_DIGITAL, // Guide
    MAPPER_OUTPUT_DIGITAL, // LS
    MAPPER_OUTPUT_DIGITAL, // RS
    MAPPER_OUTPUT_HOVER,
    MAPPER_OUTPUT_HOVER,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
};

mapper_output_type_t _sinput_output_types[SINPUT_CODE_MAX] = {
    MAPPER_OUTPUT_DIGITAL, // South
    MAPPER_OUTPUT_DIGITAL, // East
    MAPPER_OUTPUT_DIGITAL, // West
    MAPPER_OUTPUT_DIGITAL, // North
    MAPPER_OUTPUT_DPAD, // Up
    MAPPER_OUTPUT_DPAD, // Down
    MAPPER_OUTPUT_DPAD, // Left
    MAPPER_OUTPUT_DPAD, // Right
    MAPPER_OUTPUT_DIGITAL, // Stick Left
    MAPPER_OUTPUT_DIGITAL, // Stick Right
    MAPPER_OUTPUT_DIGITAL, // LB
    MAPPER_OUTPUT_DIGITAL, // RB
    MAPPER_OUTPUT_DIGITAL, // L
    MAPPER_OUTPUT_DIGITAL, // R
    MAPPER_OUTPUT_DIGITAL, // LP1
    MAPPER_OUTPUT_DIGITAL, // RP1
    MAPPER_OUTPUT_DIGITAL, // Start
    MAPPER_OUTPUT_DIGITAL, // Select
    MAPPER_OUTPUT_DIGITAL, // Guide
    MAPPER_OUTPUT_DIGITAL, // Share
    MAPPER_OUTPUT_DIGITAL, // LP2
    MAPPER_OUTPUT_DIGITAL, // RP2
    MAPPER_OUTPUT_DIGITAL, // TP1
    MAPPER_OUTPUT_DIGITAL, // TP2
    MAPPER_OUTPUT_DIGITAL, // MISC3
    MAPPER_OUTPUT_DIGITAL, // MISC4
    MAPPER_OUTPUT_DIGITAL, // MISC5,
    MAPPER_OUTPUT_DIGITAL, // MISC6
    MAPPER_OUTPUT_HOVER, // LT
    MAPPER_OUTPUT_HOVER, // RT
    MAPPER_OUTPUT_JOYSTICK, // LX
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK, // RX
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK,
    MAPPER_OUTPUT_JOYSTICK
};

const inputInfoSlot_s _infoslots[MAPPER_INPUT_COUNT] = HOJA_INPUT_SLOTS;

typedef enum 
{
    MAPPER_OUTPUT_MODE_RAPID,
    MAPPER_OUTPUT_MODE_THRESHOLD,
    MAPPER_OUTPUT_MODE_PASSTHROUGH,
} mapper_output_mode_t;

inputConfigSlot_s *_loaded_input_config = NULL;
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
    neg = (neg & 0x1FFF)>>1;
    pos = (pos & 0x1FFF)>>1;

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
        case MAPPER_OUTPUT_MODE_PASSTHROUGH:
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

typedef struct 
{
    inputConfigSlot_s *input_slots;
    mapper_output_type_t *output_types;
    bool remap_en;
    uint8_t output_types_max;
    uint16_t rapid_value[MAPPER_INPUT_COUNT];
    bool rapid_press_state[MAPPER_INPUT_COUNT];
} mapper_operation_s;

mapper_operation_s _webusb_op = {.input_slots = NULL, .output_types = NULL, .remap_en=false, .rapid_value={0}, .rapid_press_state ={0}};
mapper_operation_s _standard_op = {.input_slots = NULL, .output_types = NULL, .remap_en=true, .rapid_value={0}, .rapid_press_state ={0}};

mapper_input_s _mapper_operation(mapper_operation_s *op)
{
    // Temporary store for new output data
    mapper_input_s tmp = {0};

    for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
    {
        // i here is the input code
        uint8_t input_type = input_static.input_info[i].input_type;
        uint8_t mapped_output_code = op->input_slots[i].output_code;
        uint8_t output_type = op->output_types[mapped_output_code];
        uint8_t output_code = op->remap_en ? mapped_output_code : i;
        uint8_t output_mode = op->input_slots[i].output_mode;
        uint16_t static_output = op->input_slots[i].static_output;
        uint16_t threshold_delta = op->input_slots[i].threshold_delta;

        uint16_t *input = &_all_inputs.inputs[i];
        uint16_t *output = &tmp.inputs[output_code];

        // We perform different operation types dependent
        // on the type of INPUT (Digital, Hover, Joystick)
        // Joystick and Hover are both analog, but a Joystick is half of the resolution as a Hover (Half of 12 bits)
        switch(input_type)
        {
            default:
            case MAPPER_INPUT_TYPE_UNUSED:
                continue;

            case MAPPER_INPUT_TYPE_DIGITAL:
            {
                // Only process if the button is pressed
                if (*input == 0) continue;

                switch(output_type)
                {
                    default:
                    if(!op->remap_en)
                    {
                        *output |= MAPPER_DIGITAL_PRESS_MASK;
                        *output |= 0xFFF;
                    }
                    break;

                    // The simplest output, just pass through using the output mapcode
                    case MAPPER_OUTPUT_DIGITAL:
                    *output |= 0xFFF;
                    // Set press mask
                    *output |= MAPPER_DIGITAL_PRESS_MASK;
                    break;

                    // Output to our virtual analog dpad
                    // which will be translated later
                    case MAPPER_OUTPUT_DPAD:
                    _handle_analog_compare(output, 0xFFF);
                    *output |= MAPPER_DIGITAL_PRESS_MASK;
                    break;

                    // Output to our triggers using the configured static output value
                    // for this particular input
                    case MAPPER_OUTPUT_HOVER:
                    _handle_analog_compare(output, static_output);
                    *output |= MAPPER_DIGITAL_PRESS_MASK;
                    break;

                    // Output to our joystick using the configured static output value
                    case MAPPER_OUTPUT_JOYSTICK:
                    _handle_analog_compare(output, static_output);
                    *output |= MAPPER_DIGITAL_PRESS_MASK;
                    break;
                }
            }
            break;

            case MAPPER_INPUT_TYPE_HOVER:
            {
                switch(output_type)
                {
                    default:
                    if(!op->remap_en)
                    {
                        *output |= *input;
                        *output |= *input>0 ? MAPPER_DIGITAL_PRESS_MASK : 0;
                    }
                    break;

                    case MAPPER_OUTPUT_DIGITAL:
                    *output |= _handle_analog_to_digital(*input, output_mode, threshold_delta, 
                        &op->rapid_value[i], &op->rapid_press_state[i]) ? MAPPER_DIGITAL_PRESS_MASK : 0;

                    // Forward the original analog state too
                    *output |= *input;
                    break;

                    // Output to our virtual analog dpad
                    // which will be translated later
                    case MAPPER_OUTPUT_DPAD:
                    *output = _handle_analog_to_analog(*input, 
                        output_mode, 0xFFF, threshold_delta,
                        &op->rapid_value[i], &op->rapid_press_state[i]); 
                    
                    // Apply press mask if we have an input
                    *output |= *output>0 ? MAPPER_DIGITAL_PRESS_MASK : 0;
                    break;

                    // Output to our triggers using the configured static output value
                    // for this particular input
                    case MAPPER_OUTPUT_HOVER:
                    *output = _handle_analog_to_analog(*input, 
                        output_mode, static_output, threshold_delta,
                        &op->rapid_value[i], &op->rapid_press_state[i]);

                    *output |= *output>0 ? MAPPER_DIGITAL_PRESS_MASK : 0;
                    break;

                    // Output to our joystick using the configured static output value (divided by 2)
                    case MAPPER_OUTPUT_JOYSTICK:
                    *output = _handle_analog_to_analog(*input, 
                        output_mode, static_output, threshold_delta,
                        &op->rapid_value[i], &op->rapid_press_state[i]); 

                    *output |= *output>0 ? MAPPER_DIGITAL_PRESS_MASK : 0;
                    break;
                }
            }
            break;

            case MAPPER_INPUT_TYPE_JOYSTICK:
            {
                switch(output_type)
                {
                    default:
                    if(!op->remap_en)
                    {
                        *output |= *input<<1;
                        *output |= *input>0 ? MAPPER_DIGITAL_PRESS_MASK : 0;
                    }
                    break;

                    case MAPPER_OUTPUT_DIGITAL:
                    *output |= _handle_analog_to_digital(*input << 1, output_mode, threshold_delta, 
                        &op->rapid_value[i], &op->rapid_press_state[i]) ? MAPPER_DIGITAL_PRESS_MASK : 0;

                    // Forward the original analog state too
                    *output |= *input << 1;
                    break;

                    case MAPPER_OUTPUT_DPAD:
                    *output = _handle_analog_to_analog(*input << 1, 
                        output_mode, 0xFFF, threshold_delta,
                        &op->rapid_value[i], &op->rapid_press_state[i]) >> 1; 

                    *output |= *input>0 ? MAPPER_DIGITAL_PRESS_MASK : 0;
                    break;

                    case MAPPER_OUTPUT_HOVER:
                    *output = _handle_analog_to_analog(*input << 1, 
                        output_mode, static_output, threshold_delta,
                        &op->rapid_value[i], &op->rapid_press_state[i]); 

                    *output |= *output>0 ? MAPPER_DIGITAL_PRESS_MASK : 0;
                    break;

                    case MAPPER_OUTPUT_JOYSTICK:
                    *output = _handle_analog_to_analog(*input << 1, 
                        output_mode, static_output, threshold_delta,
                        &op->rapid_value[i], &op->rapid_press_state[i]); 

                    *output |= *output>0 ? MAPPER_DIGITAL_PRESS_MASK : 0;
                    break;
                }
            }
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

void _set_mapper_defaults(uint8_t command_code)
{
    switch(command_code)
    {
        case MAPPER_CMD_DEFAULT_ALL:
        break;

        case MAPPER_CMD_DEFAULT_SWITCH:
        break;

        case MAPPER_CMD_DEFAULT_XINPUT:
        break;

        case MAPPER_CMD_DEFAULT_SNES:
        break;

        case MAPPER_CMD_DEFAULT_N64:
        break;

        case MAPPER_CMD_DEFAULT_GAMECUBE:
        break;

        case MAPPER_CMD_DEFAULT_SINPUT:
        break;
    }
}

void _set_webusb_output_profile(uint8_t mode)
{
    _webusb_op.remap_en = false;

    switch(mode)
    {
        default:
        case GAMEPAD_MODE_SWPRO:
        _webusb_op.input_slots = input_config->input_profile_switch;
        _webusb_op.output_types = _switch_output_types;
        _webusb_op.output_types_max = SWITCH_CODE_MAX;
        break;

        case GAMEPAD_MODE_GAMECUBE:
        case GAMEPAD_MODE_GCUSB:
        _webusb_op.input_slots = input_config->input_profile_gamecube;
        _webusb_op.output_types = _gamecube_output_types;
        _webusb_op.output_types_max = GAMECUBE_CODE_MAX;
        break;

        case GAMEPAD_MODE_SNES:
        _webusb_op.input_slots = input_config->input_profile_snes;
        _webusb_op.output_types = _snes_output_types;
        _webusb_op.output_types_max = SNES_CODE_MAX;
        break;

        case GAMEPAD_MODE_N64:
        _webusb_op.input_slots = input_config->input_profile_n64;
        _webusb_op.output_types = _n64_output_types;
        _webusb_op.output_types_max = N64_CODE_MAX;
        break;

        case GAMEPAD_MODE_XINPUT:
        _webusb_op.input_slots = input_config->input_profile_xinput;
        _webusb_op.output_types = _xinput_output_types;
        _webusb_op.output_types_max = XINPUT_CODE_MAX;
        break;

        case GAMEPAD_MODE_SINPUT:
        _webusb_op.input_slots = input_config->input_profile_sinput;
        _webusb_op.output_types = _sinput_output_types;
        _webusb_op.output_types_max = SINPUT_CODE_MAX;
        break;
    }

    for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
    {
        _webusb_op.rapid_value[i] = _webusb_op.input_slots[i].threshold_delta;
    }
}

void mapper_config_command(mapper_cmd_t cmd, webreport_cmd_confirm_t cb)
{
    switch(cmd)
    {
        default:
        cb(CFG_BLOCK_INPUT, cmd, false, NULL, 0);
        break;

        case MAPPER_CMD_DEFAULT_ALL:
        //_set_mapper_defaults();
        cb(CFG_BLOCK_INPUT, cmd, true, NULL, 0);
        break;

        case MAPPER_CMD_WEBUSB_SWITCH:
        _set_webusb_output_profile(GAMEPAD_MODE_SWPRO);
        cb(CFG_BLOCK_INPUT, cmd, true, NULL, 0);
        break;

        case MAPPER_CMD_WEBUSB_XINPUT:
        _set_webusb_output_profile(GAMEPAD_MODE_XINPUT);
        cb(CFG_BLOCK_INPUT, cmd, true, NULL, 0);
        break;

        case MAPPER_CMD_WEBUSB_SNES:
        _set_webusb_output_profile(GAMEPAD_MODE_SNES);
        cb(CFG_BLOCK_INPUT, cmd, true, NULL, 0);
        break;

        case MAPPER_CMD_WEBUSB_N64:
        _set_webusb_output_profile(GAMEPAD_MODE_N64);
        cb(CFG_BLOCK_INPUT, cmd, true, NULL, 0);
        break;

        case MAPPER_CMD_WEBUSB_GAMECUBE:
        _set_webusb_output_profile(GAMEPAD_MODE_GAMECUBE);
        cb(CFG_BLOCK_INPUT, cmd, true, NULL, 0);
        break;

        case MAPPER_CMD_WEBUSB_SINPUT:
        _set_webusb_output_profile(GAMEPAD_MODE_SINPUT);
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

static inline void _mapper_set_defaults(inputConfigSlot_s *cfg_slots, const int8_t *output_codes, uint8_t *output_types)
{
    for(int i = 0; i < MAPPER_INPUT_COUNT; i++)
    {
        int8_t output_code = output_codes[i];
        cfg_slots[i].output_code = output_code;
        

        switch(input_static.input_info[i].input_type)
        {
            case MAPPER_INPUT_TYPE_HOVER:
            case MAPPER_INPUT_TYPE_JOYSTICK:

            switch(output_types[output_code])
            {
                case MAPPER_OUTPUT_DPAD:
                case MAPPER_OUTPUT_HOVER:
                case MAPPER_OUTPUT_JOYSTICK:
                cfg_slots[i].output_mode = MAPPER_OUTPUT_MODE_PASSTHROUGH;
                break;

                default:
                cfg_slots[i].output_mode = 0; // Default
                break;
            }
            break;

            default:
            cfg_slots[i].output_mode = 0; // Default
            break;
        }

        cfg_slots[i].static_output = 0xFFF+1; // Max 
        cfg_slots[i].threshold_delta = (0xFFF+1)/6;
    }
}

void mapper_init()
{
    const int8_t default_codes_switch[MAPPER_INPUT_COUNT] = HOJA_INPUT_DEFAULTS_SWITCH;
    const int8_t default_codes_snes[MAPPER_INPUT_COUNT] = HOJA_INPUT_DEFAULTS_SNES;
    const int8_t default_codes_n64[MAPPER_INPUT_COUNT] = HOJA_INPUT_DEFAULTS_N64;
    const int8_t default_codes_gamecube[MAPPER_INPUT_COUNT] = HOJA_INPUT_DEFAULTS_GAMECUBE;
    const int8_t default_codes_xinput[MAPPER_INPUT_COUNT] = HOJA_INPUT_DEFAULTS_XINPUT;
    const int8_t default_codes_sinput[MAPPER_INPUT_COUNT] = HOJA_INPUT_DEFAULTS_SINPUT;

    // Debug always set to defaults on reboot
    if(input_config->input_config_version != CFG_BLOCK_INPUT_VERSION)
    {
        input_config->input_config_version = CFG_BLOCK_INPUT_VERSION;
        _mapper_set_defaults(input_config->input_profile_switch, default_codes_switch, _switch_output_types);
        _mapper_set_defaults(input_config->input_profile_snes, default_codes_snes, _snes_output_types);
        _mapper_set_defaults(input_config->input_profile_n64, default_codes_n64, _n64_output_types);
        _mapper_set_defaults(input_config->input_profile_gamecube, default_codes_gamecube, _gamecube_output_types);
        _mapper_set_defaults(input_config->input_profile_xinput, default_codes_xinput, _xinput_output_types);
        _mapper_set_defaults(input_config->input_profile_sinput, default_codes_sinput, _sinput_output_types);
    }

    static bool boot_init = false;
    if(!boot_init)
        _set_webusb_output_profile(GAMEPAD_MODE_SWPRO);
    boot_init = true;

    switch(hoja_get_status().gamepad_mode)
    {
        default:
        case GAMEPAD_MODE_SWPRO:
        _standard_op.input_slots = input_config->input_profile_switch;
        _standard_op.output_types = _switch_output_types;
        _standard_op.output_types_max = SWITCH_CODE_MAX;
        break;

        case GAMEPAD_MODE_GAMECUBE:
        case GAMEPAD_MODE_GCUSB:
        _standard_op.input_slots = input_config->input_profile_gamecube;
        _standard_op.output_types = _gamecube_output_types;
        _standard_op.output_types_max = GAMECUBE_CODE_MAX;
        break;

        case GAMEPAD_MODE_SNES:
        _standard_op.input_slots = input_config->input_profile_snes;
        _standard_op.output_types = _snes_output_types;
        _standard_op.output_types_max = SNES_CODE_MAX;
        break;

        case GAMEPAD_MODE_N64:
        _standard_op.input_slots = input_config->input_profile_n64;
        _standard_op.output_types = _n64_output_types;
        _standard_op.output_types_max = N64_CODE_MAX;
        break;

        case GAMEPAD_MODE_XINPUT:
        _standard_op.input_slots = input_config->input_profile_xinput;
        _standard_op.output_types = _xinput_output_types;
        _standard_op.output_types_max = XINPUT_CODE_MAX;
        break;

        case GAMEPAD_MODE_SINPUT:
        _standard_op.input_slots = input_config->input_profile_sinput;
        _standard_op.output_types = _sinput_output_types;
        _standard_op.output_types_max = SINPUT_CODE_MAX;
        break;
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

mapper_input_s mapper_get_webusb_input()
{
    analog_data_s joysticks;
    mapper_input_s hovers;

    analog_access_safe(&joysticks, ANALOG_ACCESS_DEADZONE_DATA);
    hover_access_safe(&hovers);

    _all_inputs = hovers;
    _all_inputs.inputs[INPUT_CODE_LX_LEFT] = joysticks.lx < 0 ? -joysticks.lx : 0;
    _all_inputs.inputs[INPUT_CODE_LX_RIGHT] = joysticks.lx > 0 ? joysticks.lx : 0;
    _all_inputs.inputs[INPUT_CODE_LY_UP] = joysticks.ly > 0 ? joysticks.ly : 0;
    _all_inputs.inputs[INPUT_CODE_LY_DOWN] = joysticks.ly < 0 ? -joysticks.ly : 0;

    _all_inputs.inputs[INPUT_CODE_RX_LEFT] = joysticks.rx < 0 ? -joysticks.rx : 0;
    _all_inputs.inputs[INPUT_CODE_RX_RIGHT] = joysticks.rx > 0 ? joysticks.rx : 0;
    _all_inputs.inputs[INPUT_CODE_RY_UP] = joysticks.ry > 0 ? joysticks.ry : 0;
    _all_inputs.inputs[INPUT_CODE_RY_DOWN] = joysticks.ry < 0 ? -joysticks.ry : 0;

    return _mapper_operation(&_webusb_op);
}

mapper_input_s mapper_get_input()
{
    analog_data_s joysticks;
    mapper_input_s hovers;

    analog_access_safe(&joysticks, ANALOG_ACCESS_DEADZONE_DATA);
    hover_access_safe(&hovers);

    _all_inputs = hovers;
    _all_inputs.inputs[INPUT_CODE_LX_LEFT] = joysticks.lx < 0 ? -joysticks.lx : 0;
    _all_inputs.inputs[INPUT_CODE_LX_RIGHT] = joysticks.lx > 0 ? joysticks.lx : 0;
    _all_inputs.inputs[INPUT_CODE_LY_UP] = joysticks.ly > 0 ? joysticks.ly : 0;
    _all_inputs.inputs[INPUT_CODE_LY_DOWN] = joysticks.ly < 0 ? -joysticks.ly : 0;

    _all_inputs.inputs[INPUT_CODE_RX_LEFT] = joysticks.rx < 0 ? -joysticks.rx : 0;
    _all_inputs.inputs[INPUT_CODE_RX_RIGHT] = joysticks.rx > 0 ? joysticks.rx : 0;
    _all_inputs.inputs[INPUT_CODE_RY_UP] = joysticks.ry > 0 ? joysticks.ry : 0;
    _all_inputs.inputs[INPUT_CODE_RY_DOWN] = joysticks.ry < 0 ? -joysticks.ry : 0;

    // Return output pointer
    return _mapper_operation(&_standard_op);
}

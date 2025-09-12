#include "input/mapper.h" 
#include "input_shared_types.h"

#include "input/analog.h"
#include "input/trigger.h"
#include "input/button.h"

#include "utilities/settings.h"

#include "hoja.h"

#include <stdlib.h>
#include <string.h>

typedef mapper_input_s(*mapper_input_cb)(void);

mapper_input_cb _mapper_input_cb;
mapper_profile_s *_mapper_profile;

// Switch (modern layout)
mapper_profile_s _map_default_switch = {
#if defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_BAYX)
    .south   = SWITCH_CODE_B,
    .east    = SWITCH_CODE_A,
    .west    = SWITCH_CODE_Y,
    .north   = SWITCH_CODE_X,
#elif defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_AXBY)
    .south   = SWITCH_CODE_A,
    .east    = SWITCH_CODE_X,
    .west    = SWITCH_CODE_B,
    .north   = SWITCH_CODE_Y,
#else
    .south   = SWITCH_CODE_A,
    .east    = SWITCH_CODE_B,
    .west    = SWITCH_CODE_X,
    .north   = SWITCH_CODE_Y,
#endif
    .up      = SWITCH_CODE_UP,
    .down    = SWITCH_CODE_DOWN,
    .left    = SWITCH_CODE_LEFT,
    .right   = SWITCH_CODE_RIGHT,
    .lb      = SWITCH_CODE_L,
    .rb      = SWITCH_CODE_R,
    .lt      = SWITCH_CODE_LZ,
    .rt      = SWITCH_CODE_RZ,
    .start   = SWITCH_CODE_PLUS,
    .select  = SWITCH_CODE_MINUS,
    .home    = SWITCH_CODE_HOME,
    .capture = SWITCH_CODE_CAPTURE,
    .ls      = SWITCH_CODE_LS,
    .rs      = SWITCH_CODE_RS,
    .lg_upper = SWITCH_CODE_UNUSED,
    .rg_upper = SWITCH_CODE_UNUSED,
    .lg_lower = SWITCH_CODE_UNUSED,
    .rg_lower = SWITCH_CODE_UNUSED,
    .lt_analog = SWITCH_CODE_LZ,
    .rt_analog = SWITCH_CODE_RZ,
    .lx_right = SWITCH_CODE_LX_RIGHT,
    .lx_left  = SWITCH_CODE_LX_LEFT,
    .ly_up    = SWITCH_CODE_LY_UP,
    .ly_down  = SWITCH_CODE_LY_DOWN,
    .rx_right = SWITCH_CODE_RX_RIGHT,
    .rx_left  = SWITCH_CODE_RX_LEFT,
    .ry_up    = SWITCH_CODE_RY_UP,
    .ry_down  = SWITCH_CODE_RY_DOWN,
};

// SNES (no sticks, no triggers beyond L/R) 
mapper_profile_s _map_default_snes = {
#if defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_BAYX)
    .south   = SNES_CODE_B,
    .east    = SNES_CODE_A,
    .west    = SNES_CODE_Y,
    .north   = SNES_CODE_X,
#elif defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_AXBY)
    .south   = SNES_CODE_A,
    .east    = SNES_CODE_X,
    .west    = SNES_CODE_B,
    .north   = SNES_CODE_Y,
#else
    .south   = SNES_CODE_A,
    .east    = SNES_CODE_B,
    .west    = SNES_CODE_X,
    .north   = SNES_CODE_Y,
#endif
    .up      = SNES_CODE_UP,
    .down    = SNES_CODE_DOWN,
    .left    = SNES_CODE_LEFT,
    .right   = SNES_CODE_RIGHT,
    .lb      = SNES_CODE_L,
    .rb      = SNES_CODE_R,
    .lt      = SNES_CODE_L,  // no separate ZL
    .rt      = SNES_CODE_R,  // no separate ZR
    .start   = SNES_CODE_START,
    .select  = SNES_CODE_SELECT,
    .home    = SNES_CODE_UNUSED,
    .capture = SNES_CODE_UNUSED,
    .ls      = SNES_CODE_UNUSED,
    .rs      = SNES_CODE_UNUSED,
    .lg_upper = SNES_CODE_UNUSED,
    .rg_upper = SNES_CODE_UNUSED,
    .lg_lower = SNES_CODE_UNUSED,
    .rg_lower = SNES_CODE_UNUSED,
    .lt_analog = SNES_CODE_L,
    .rt_analog = SNES_CODE_R,
    .lx_right = SNES_CODE_RIGHT,
    .lx_left  = SNES_CODE_LEFT,
    .ly_up    = SNES_CODE_UP,
    .ly_down  = SNES_CODE_DOWN,
    .rx_right = SNES_CODE_UNUSED,
    .rx_left  = SNES_CODE_UNUSED,
    .ry_up    = SNES_CODE_UNUSED,
    .ry_down  = SNES_CODE_UNUSED,
};

// N64 (1 stick + C buttons)
mapper_profile_s _map_default_n64 = {
#if defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_BAYX)
    .south   = N64_CODE_B,
    .east    = N64_CODE_A,
    .west    = N64_CODE_CDOWN,
    .north   = N64_CODE_CUP,
#elif defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_AXBY)
    .south   = N64_CODE_A,
    .east    = N64_CODE_CDOWN,
    .west    = N64_CODE_B,
    .north   = N64_CODE_CUP,
#else 
    .south   = N64_CODE_A,
    .east    = N64_CODE_B,
    .west    = N64_CODE_CDOWN,
    .north   = N64_CODE_CUP,
#endif
    .up      = N64_CODE_UP,
    .down    = N64_CODE_DOWN,
    .left    = N64_CODE_LEFT,
    .right   = N64_CODE_RIGHT,
    .lb      = N64_CODE_CLEFT,
    .rb      = N64_CODE_CRIGHT,
    .lt      = N64_CODE_Z,
    .rt      = N64_CODE_R,
    .start   = N64_CODE_START,
    .select  = N64_CODE_L,
    .home    = N64_CODE_UNUSED,
    .capture = N64_CODE_UNUSED,
    .ls      = N64_CODE_UNUSED,
    .rs      = N64_CODE_UNUSED,
    .lg_upper = N64_CODE_UNUSED,
    .rg_upper = N64_CODE_UNUSED,
    .lg_lower = N64_CODE_UNUSED,
    .rg_lower = N64_CODE_UNUSED,
    .lt_analog = N64_CODE_Z,
    .rt_analog = N64_CODE_R,
    .lx_right = N64_CODE_LX_RIGHT,
    .lx_left  = N64_CODE_LX_LEFT,
    .ly_up    = N64_CODE_LY_UP,
    .ly_down  = N64_CODE_LY_DOWN,
    .rx_right = N64_CODE_CRIGHT,
    .rx_left  = N64_CODE_CLEFT,
    .ry_up    = N64_CODE_CUP,
    .ry_down  = N64_CODE_CDOWN,
};

// GameCube (2 sticks + analog triggers)
mapper_profile_s _map_default_gamecube = {
#if defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_BAYX)
    .south   = GAMECUBE_CODE_B,
    .east    = GAMECUBE_CODE_A,
    .west    = GAMECUBE_CODE_Y,
    .north   = GAMECUBE_CODE_X,
#elif defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_AXBY)
    .south   = GAMECUBE_CODE_A,
    .east    = GAMECUBE_CODE_X,
    .west    = GAMECUBE_CODE_B,
    .north   = GAMECUBE_CODE_Y,
#else 
    .south   = GAMECUBE_CODE_A,
    .east    = GAMECUBE_CODE_B,
    .west    = GAMECUBE_CODE_X,
    .north   = GAMECUBE_CODE_Y,
#endif
    .up      = GAMECUBE_CODE_UP,
    .down    = GAMECUBE_CODE_DOWN,
    .left    = GAMECUBE_CODE_LEFT,
    .right   = GAMECUBE_CODE_RIGHT,
    .lb      = GAMECUBE_CODE_UNUSED,
    .rb      = GAMECUBE_CODE_Z,
    .lt      = GAMECUBE_CODE_L,
    .rt      = GAMECUBE_CODE_R,
    .start   = GAMECUBE_CODE_START,
    .select  = GAMECUBE_CODE_UNUSED,
    .home    = GAMECUBE_CODE_UNUSED,
    .capture = GAMECUBE_CODE_UNUSED,
    .ls      = GAMECUBE_CODE_UNUSED,
    .rs      = GAMECUBE_CODE_UNUSED,
    .lg_upper = GAMECUBE_CODE_UNUSED,
    .rg_upper = GAMECUBE_CODE_UNUSED,
    .lg_lower = GAMECUBE_CODE_UNUSED,
    .rg_lower = GAMECUBE_CODE_UNUSED,
    .lt_analog = GAMECUBE_CODE_L_ANALOG,
    .rt_analog = GAMECUBE_CODE_R_ANALOG,
    .lx_right = GAMECUBE_CODE_LX_RIGHT,
    .lx_left  = GAMECUBE_CODE_LX_LEFT,
    .ly_up    = GAMECUBE_CODE_LY_UP,
    .ly_down  = GAMECUBE_CODE_LY_DOWN,
    .rx_right = GAMECUBE_CODE_RX_RIGHT,
    .rx_left  = GAMECUBE_CODE_RX_LEFT,
    .ry_up    = GAMECUBE_CODE_RY_UP,
    .ry_down  = GAMECUBE_CODE_RY_DOWN,
};

// XInput (Xbox 360/One style)
mapper_profile_s _map_default_xinput = {
#if defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_BAYX)
    .south   = XINPUT_CODE_B,
    .east    = XINPUT_CODE_A,
    .west    = XINPUT_CODE_Y,
    .north   = XINPUT_CODE_X,
#elif defined(HOJA_SEWN_TYPE) && (HOJA_SEWN_TYPE == SEWN_LAYOUT_AXBY)
    .south   = XINPUT_CODE_A,
    .east    = XINPUT_CODE_X,
    .west    = XINPUT_CODE_B,
    .north   = XINPUT_CODE_Y,
#else 
    .south   = XINPUT_CODE_A,
    .east    = XINPUT_CODE_B,
    .west    = XINPUT_CODE_X,
    .north   = XINPUT_CODE_Y,
#endif
    .up      = XINPUT_CODE_UP,
    .down    = XINPUT_CODE_DOWN,
    .left    = XINPUT_CODE_LEFT,
    .right   = XINPUT_CODE_RIGHT,
    .lb      = XINPUT_CODE_LB,
    .rb      = XINPUT_CODE_RB,
    .lt      = XINPUT_CODE_LT_FULL,
    .rt      = XINPUT_CODE_RT_FULL,
    .start   = XINPUT_CODE_START,
    .select  = XINPUT_CODE_BACK,
    .home    = XINPUT_CODE_GUIDE,
    .capture = XINPUT_CODE_UNUSED,
    .ls      = XINPUT_CODE_LS,
    .rs      = XINPUT_CODE_RS,
    .lg_upper = XINPUT_CODE_UNUSED,
    .rg_upper = XINPUT_CODE_UNUSED,
    .lg_lower = XINPUT_CODE_UNUSED,
    .rg_lower = XINPUT_CODE_UNUSED,
    .lt_analog = XINPUT_CODE_LT_ANALOG,
    .rt_analog = XINPUT_CODE_RT_ANALOG,
    .lx_right = XINPUT_CODE_LX_RIGHT,
    .lx_left  = XINPUT_CODE_LX_LEFT,
    .ly_up    = XINPUT_CODE_LY_UP,
    .ly_down  = XINPUT_CODE_LY_DOWN,
    .rx_right = XINPUT_CODE_RX_RIGHT,
    .rx_left  = XINPUT_CODE_RX_LEFT,
    .ry_up    = XINPUT_CODE_RY_UP,
    .ry_down  = XINPUT_CODE_RY_DOWN,
};

mapper_input_s _mapper_input = {0};

uint16_t *_joystick_thresholds[8] = {NULL};
uint16_t *_trigger_thresholds[2] = {NULL};
uint16_t *_trigger_static[2] = {NULL};

// Completed
mapper_input_s _mapper_task_switch()
{
    mapper_input_s tmp = {0};
    bool down = false;
    uint32_t map_digital_bitmask = 0;
    uint8_t analog_idx_in = 0;
    uint8_t analog_idx_out = 0; 
    
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
            case 0 ... SWITCH_CODE_RS:
            tmp.digital_inputs |= (1 << map_code_output);
            break;

            // Joystick Output
            case SWITCH_CODE_LX_RIGHT ... SWITCH_CODE_RY_DOWN:
            analog_idx_out = map_code_output - SWITCH_CODE_IDX_JOYSTICK_START;
            tmp.joysticks_raw[analog_idx_out] = 0x7FF;
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
            case SWITCH_CODE_B ... SWITCH_CODE_RS:
            if(_mapper_input.triggers[analog_idx_in] >= *_trigger_thresholds[analog_idx_in])
            {
                tmp.digital_inputs |= (1 << map_code_output);
            }
            break;

            // Joystick Output
            case SWITCH_CODE_LX_RIGHT ... SWITCH_CODE_RY_DOWN:
            analog_idx_out = map_code_output - SWITCH_CODE_IDX_JOYSTICK_START;
            uint16_t trigger_scaled = _mapper_input.triggers[analog_idx_in]>>1;
            if(trigger_scaled > tmp.joysticks_raw[analog_idx_out])
            {
                tmp.joysticks_raw[analog_idx_out] = trigger_scaled; // Half of trigger range
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
            case SWITCH_CODE_B ... SWITCH_CODE_RS:
            if(_mapper_input.joysticks_raw[analog_idx_in] >= *_joystick_thresholds[analog_idx_in])
            {
                tmp.digital_inputs |= (1 << map_code_output);
            }
            break;

            // Joystick Output
            case SWITCH_CODE_LX_RIGHT ... SWITCH_CODE_RY_DOWN:
            analog_idx_out = map_code_output - SWITCH_CODE_IDX_JOYSTICK_START;
            if(_mapper_input.joysticks_raw[analog_idx_in] > tmp.joysticks_raw[analog_idx_out])
            {
                tmp.joysticks_raw[analog_idx_out] = _mapper_input.joysticks_raw[analog_idx_in];
            }
            break;
        }
    }

    return tmp;
}

// Completed
mapper_input_s _mapper_task_snes()
{
    mapper_input_s tmp = {0};
    bool down = false;
    uint32_t map_digital_bitmask = 0;
    uint8_t analog_idx_in = 0;
    uint8_t analog_idx_out = 0; 
    
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
            case 0 ... SNES_CODE_SELECT:
            tmp.digital_inputs |= (1 << map_code_output);
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
            case SNES_CODE_B ... SNES_CODE_SELECT:
            if(_mapper_input.triggers[analog_idx_in] >= *_trigger_thresholds[analog_idx_in])
            {
                tmp.digital_inputs |= (1 << map_code_output);
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
            case SNES_CODE_B ... SNES_CODE_SELECT:
            if(_mapper_input.joysticks_raw[analog_idx_in] >= *_joystick_thresholds[analog_idx_in])
            {
                tmp.digital_inputs |= (1 << map_code_output);
            }
            break;
        }
    }

    return tmp;
}

// Completed
mapper_input_s _mapper_task_n64()
{
    mapper_input_s tmp = {0};
    bool down = false;
    uint32_t map_digital_bitmask = 0;
    uint8_t analog_idx_in = 0;
    uint8_t analog_idx_out = 0; 
    
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
            case 0 ... N64_CODE_START:
            tmp.digital_inputs |= (1 << map_code_output);
            break;

            // Joystick Output
            case N64_CODE_LX_RIGHT ... N64_CODE_LY_DOWN:
            analog_idx_out = map_code_output - N64_CODE_IDX_JOYSTICK_START;
            tmp.joysticks_raw[analog_idx_out] = 0x7FF;
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
            case N64_CODE_B ... N64_CODE_START:
            if(_mapper_input.triggers[analog_idx_in] >= *_trigger_thresholds[analog_idx_in])
            {
                tmp.digital_inputs |= (1 << map_code_output);
            }
            break;

            // Joystick Output
            case N64_CODE_LX_RIGHT ... N64_CODE_LY_DOWN:
            analog_idx_out = map_code_output - N64_CODE_IDX_JOYSTICK_START;
            uint16_t trigger_scaled = _mapper_input.triggers[analog_idx_in]>>1;
            if(trigger_scaled > tmp.joysticks_raw[analog_idx_out])
            {
                tmp.joysticks_raw[analog_idx_out] = trigger_scaled;
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
            case N64_CODE_B ... N64_CODE_START:
            if(_mapper_input.joysticks_raw[analog_idx_in] >= *_joystick_thresholds[analog_idx_in])
            {
                tmp.digital_inputs |= (1 << map_code_output);
            }
            break;

            // Joystick Output
            case N64_CODE_LX_RIGHT ... N64_CODE_LY_DOWN:
            analog_idx_out = map_code_output - N64_CODE_IDX_JOYSTICK_START;
            if(_mapper_input.joysticks_raw[analog_idx_in] > tmp.joysticks_raw[analog_idx_out])
            {
                tmp.joysticks_raw[analog_idx_out] = _mapper_input.joysticks_raw[analog_idx_in];
            }
            break;
        }
    }

    return tmp;
}

// Completed
mapper_input_s _mapper_task_gamecube()
{
    mapper_input_s tmp = {0};
    bool down = false;
    uint32_t map_digital_bitmask = 0;
    uint8_t analog_idx_in = 0;
    uint8_t analog_idx_out = 0; 
    
    int8_t map_code_output = -1;
    
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
            case 0 ... GAMECUBE_CODE_R:
            tmp.digital_inputs |= (1 << (uint8_t)map_code_output);
            break;

            // Trigger Output
            case GAMECUBE_CODE_L_ANALOG ... GAMECUBE_CODE_R_ANALOG:
            analog_idx_out = map_code_output - GAMECUBE_CODE_IDX_TRIGGER_START;
            if(*_trigger_static[analog_idx_out] > tmp.triggers[analog_idx_out])
            {
                tmp.triggers[analog_idx_out] = *_trigger_static[analog_idx_out];
            }
            break;

            // Joystick Output
            case GAMECUBE_CODE_LX_RIGHT ... GAMECUBE_CODE_RY_DOWN:
            analog_idx_out = map_code_output - GAMECUBE_CODE_IDX_JOYSTICK_START;
            tmp.joysticks_raw[analog_idx_out] = 2048;
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
            case GAMECUBE_CODE_B ... GAMECUBE_CODE_START:
            if(_mapper_input.triggers[analog_idx_in] >= *_trigger_thresholds[analog_idx_in])
            {
                tmp.digital_inputs |= (1 << map_code_output);
            }
            break;

            // Trigger Output
            case GAMECUBE_CODE_L_ANALOG ... GAMECUBE_CODE_R_ANALOG:
            analog_idx_out = map_code_output - GAMECUBE_CODE_IDX_TRIGGER_START;
            if(_mapper_input.triggers[analog_idx_in] > tmp.triggers[analog_idx_out])
            {
                tmp.triggers[analog_idx_out] = _mapper_input.triggers[analog_idx_in];
            }
            break;

            // Joystick Output
            case GAMECUBE_CODE_LX_RIGHT ... GAMECUBE_CODE_RY_DOWN:
            analog_idx_out = map_code_output - GAMECUBE_CODE_IDX_JOYSTICK_START;
            uint16_t trigger_scaled = _mapper_input.triggers[analog_idx_in]>>1;
            if(trigger_scaled > tmp.joysticks_raw[analog_idx_out])
            {
                tmp.joysticks_raw[analog_idx_out] = trigger_scaled;
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
            case GAMECUBE_CODE_B ... GAMECUBE_CODE_START:
            if(_mapper_input.joysticks_raw[analog_idx_in] >= *_joystick_thresholds[analog_idx_in])
            {
                tmp.digital_inputs |= (1 << map_code_output);
            }
            break;

            // Trigger Output
            case GAMECUBE_CODE_L_ANALOG ... GAMECUBE_CODE_R_ANALOG:
            analog_idx_out = map_code_output - GAMECUBE_CODE_IDX_TRIGGER_START;
            uint16_t joystick_scaled = _mapper_input.joysticks_raw[analog_idx_in]<<1;
            if(joystick_scaled > tmp.triggers[analog_idx_out])
            {
                tmp.triggers[analog_idx_out] = joystick_scaled;
            }
            break;

            // Joystick Output
            case GAMECUBE_CODE_LX_RIGHT ... GAMECUBE_CODE_RY_DOWN:
            analog_idx_out = map_code_output - GAMECUBE_CODE_IDX_JOYSTICK_START;
            if(_mapper_input.joysticks_raw[analog_idx_in] > tmp.joysticks_raw[analog_idx_out])
            {
                tmp.joysticks_raw[analog_idx_out] = _mapper_input.joysticks_raw[analog_idx_in];
            }
            break;
        }
    }

    return tmp;
}

// Completed
mapper_input_s _mapper_task_xinput()
{
    mapper_input_s tmp = {0};
    bool down;
    uint32_t map_digital_bitmask;
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
            case XINPUT_CODE_A ... XINPUT_CODE_RT_FULL:
            tmp.digital_inputs |= (1 << map_code_output);
            break;

            // Trigger Output
            case XINPUT_CODE_LT_ANALOG ... XINPUT_CODE_RT_ANALOG:
            analog_idx_out = map_code_output - XINPUT_CODE_IDX_TRIGGER_START;
            if(*_trigger_static[analog_idx_out] > tmp.triggers[analog_idx_out])
            {
                tmp.triggers[analog_idx_out] = *_trigger_static[analog_idx_out];
            }
            break;

            // Joystick Output
            case XINPUT_CODE_LX_RIGHT ... XINPUT_CODE_RY_DOWN:
            analog_idx_out = map_code_output - XINPUT_CODE_IDX_JOYSTICK_START;
            tmp.joysticks_raw[analog_idx_out] = 0x7FF;
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
            case XINPUT_CODE_A ... XINPUT_CODE_RS:
            if(_mapper_input.triggers[analog_idx_in] >= *_trigger_thresholds[analog_idx_in])
            {
                tmp.digital_inputs |= (1 << map_code_output);
            }
            break;

            // Trigger Output
            case XINPUT_CODE_LT_ANALOG ... XINPUT_CODE_RT_ANALOG:
            analog_idx_out = map_code_output - XINPUT_CODE_IDX_TRIGGER_START;
            if(_mapper_input.triggers[analog_idx_in] > tmp.triggers[analog_idx_out])
            {
                tmp.triggers[analog_idx_out] = _mapper_input.triggers[analog_idx_in];
            }
            break;

            // Joystick Output
            case XINPUT_CODE_LX_RIGHT ... XINPUT_CODE_RY_DOWN:
            analog_idx_out = map_code_output - XINPUT_CODE_IDX_JOYSTICK_START;
            uint16_t trigger_scaled = _mapper_input.triggers[analog_idx_in];
            if(trigger_scaled > tmp.joysticks_raw[analog_idx_out])
            {
                tmp.joysticks_raw[analog_idx_out] = trigger_scaled;
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
            case XINPUT_CODE_A ... XINPUT_CODE_RS:
            if(_mapper_input.joysticks_raw[analog_idx_in] >= *_joystick_thresholds[analog_idx_in])
            {
                tmp.digital_inputs |= (1 << map_code_output);
            }
            break;

            // Trigger Output
            case XINPUT_CODE_LT_ANALOG ... XINPUT_CODE_RT_ANALOG:
            analog_idx_out = map_code_output - XINPUT_CODE_IDX_TRIGGER_START;
            uint16_t joystick_scaled = _mapper_input.joysticks_raw[analog_idx_in]<<1;
            if(joystick_scaled > tmp.triggers[analog_idx_out])
            {
                tmp.triggers[analog_idx_out] = joystick_scaled;
            }
            break;

            // Joystick Output
            case XINPUT_CODE_LX_RIGHT ... XINPUT_CODE_RY_DOWN:
            analog_idx_out = map_code_output - XINPUT_CODE_IDX_JOYSTICK_START;
            if(_mapper_input.joysticks_raw[analog_idx_in] > tmp.joysticks_raw[analog_idx_out])
            {
                tmp.joysticks_raw[analog_idx_out] = _mapper_input.joysticks_raw[analog_idx_in];
            }
            break;
        }
    }

    return tmp;
}

int8_t digital_idx_last = MAPPER_CODE_IDX_TRIGGER_START;
int8_t trigger_idx_start = MAPPER_CODE_IDX_TRIGGER_START;
int8_t joystick_idx_start = MAPPER_CODE_IDX_JOYSTICK_START;

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
    memcpy(remap_config->remap_profile_switch, (uint8_t *)&_map_default_switch, sizeof(_map_default_switch));
    memcpy(remap_config->remap_profile_snes, (uint8_t *)&_map_default_snes, sizeof(_map_default_snes));
    memcpy(remap_config->remap_profile_n64, (uint8_t *)&_map_default_n64, sizeof(_map_default_n64));
    memcpy(remap_config->remap_profile_gamecube, (uint8_t *)&_map_default_gamecube, sizeof(_map_default_gamecube));
    memcpy(remap_config->remap_profile_xinput, (uint8_t *)&_map_default_xinput, sizeof(_map_default_xinput));
}

void mapper_config_command(remap_cmd_t cmd, webreport_cmd_confirm_t cb)
{
    switch(cmd)
    {
        default:
        cb(CFG_BLOCK_REMAP, cmd, false, NULL, 0);
        break;

        case REMAP_CMD_DEFAULT:
            _set_mapper_defaults();
            cb(CFG_BLOCK_REMAP, cmd, true, NULL, 0);
        break;
    }  
}

void mapper_init()
{
    // Check for remap defaults
    if(remap_config->remap_config_version != CFG_BLOCK_REMAP_VERSION)
    {
        _set_mapper_defaults();
        remap_config->remap_config_version = CFG_BLOCK_REMAP_VERSION;
    }

    _joystick_thresholds[0] = &analog_config->l_threshold;
    _joystick_thresholds[1] = &analog_config->l_threshold;
    _joystick_thresholds[2] = &analog_config->l_threshold;
    _joystick_thresholds[3] = &analog_config->l_threshold;

    _joystick_thresholds[4] = &analog_config->r_threshold;
    _joystick_thresholds[5] = &analog_config->r_threshold;
    _joystick_thresholds[6] = &analog_config->r_threshold;
    _joystick_thresholds[7] = &analog_config->r_threshold;

    _trigger_thresholds[0] = &trigger_config->left_hairpin_value;
    _trigger_thresholds[1] = &trigger_config->right_hairpin_value;

    _trigger_static[0] = &trigger_config->left_static_output_value;
    _trigger_static[1] = &trigger_config->right_static_output_value;

    switch(hoja_get_status().gamepad_mode)
    {
        case GAMEPAD_MODE_SWPRO:
        _mapper_input_cb = _mapper_task_switch;
        _mapper_profile = (mapper_profile_s *) remap_config->remap_profile_switch;
        break;

        case GAMEPAD_MODE_SNES:
        _mapper_input_cb = _mapper_task_snes;
        _mapper_profile = (mapper_profile_s *) remap_config->remap_profile_snes;
        break;

        case GAMEPAD_MODE_N64:
        _mapper_input_cb = _mapper_task_n64;
        _mapper_profile = (mapper_profile_s *) remap_config->remap_profile_n64;
        break;

        case GAMEPAD_MODE_GAMECUBE:
        case GAMEPAD_MODE_GCUSB:
        _mapper_input_cb = _mapper_task_gamecube;
        _mapper_profile = (mapper_profile_s *) remap_config->remap_profile_gamecube;
        break;

        case GAMEPAD_MODE_XINPUT:
        _mapper_input_cb = _mapper_task_xinput;
        _mapper_profile = (mapper_profile_s *) remap_config->remap_profile_xinput;
        break;

        default:
        _mapper_input_cb = NULL;
        _mapper_profile = NULL;
        break;
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
    _mapper_input.triggers[0] = triggers.left_analog;
    _mapper_input.triggers[1] = triggers.right_analog;

    if(_mapper_input_cb != NULL && _mapper_profile != NULL)
    {
        output = _mapper_input_cb();
    }
    else 
    {
        output = _mapper_input;
    }

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

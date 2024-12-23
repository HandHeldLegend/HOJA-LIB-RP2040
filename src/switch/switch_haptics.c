#include "switch/switch_haptics.h"
#include "utilities/pcm.h"
#include <string.h>
#include "devices_shared_types.h"
#include <math.h>

/* We used to use this ExpBase lookup table made with this data
    #define EXP_BASE2_RANGE_START (-8.0f)
    #define EXP_BASE2_RANGE_END (2.0f)
    #define EXP_BASE2_LOOKUP_RESOLUTION (1 / 32.0f)
    #define EXP_BASE2_LOOKUP_LENGTH 320 //((size_t)(((EXP_BASE2_RANGE_END - EXP_BASE2_RANGE_START) + EXP_BASE2_LOOKUP_RESOLUTION) / EXP_BASE2_LOOKUP_RESOLUTION))

    static float ExpBase2Lookup[EXP_BASE2_LOOKUP_LENGTH];
    static float RumbleAmpLookup[128];
    static float RumbleFreqLookup[128];

    void initialize_exp_base2_lookup(void)
    {
        for (size_t i = 0; i < EXP_BASE2_LOOKUP_LENGTH; ++i)
        {
            float f = EXP_BASE2_RANGE_START + i * EXP_BASE2_LOOKUP_RESOLUTION;
            if (f >= StartingAmplitude)
            {
                ExpBase2Lookup[i] = exp2f(f);
            }
            else
            {
                ExpBase2Lookup[i] = 0.0f;
            }
        }
    }

    We no longer use this because we are using fixed-point values
*/

// Amplitude, we are using Q16.16 fixed-point
int32_t float_to_q1616(float value) {
    return (int32_t)(value * 65536.0f);
}

float q1616_to_float(int32_t value) {
    return value / 65536.0f;
}

// Frequency sine increment values are Q8.8 fixed-point
static uint16_t _haptics_hi_freq_increment[128];
static uint16_t _haptics_lo_freq_increment[128];

/* Original frequency lookup table algorithm
    void initialize_rumble_freq_lookup(void)
    {
        for (size_t i = 0; i < 128; ++i)
        {
            RumbleFreqLookup[i] = 0.03125f * i - 2.0f;
        }
    }
*/

// Q8.8 fixed-point frequency table
void _init_frequency_phase_increment_tables(void) {

    // Initialize low frequency table (160Hz center)
    for(int i = 0; i < 128; i++) {
        // Calculate the linear exponent
        double linear = 0.03125 * i - 2.0;
        
        // Calculate actual frequency
        double freq = 160.0 * exp2f(linear);
        
        // Calculate phase increment in fixed point
        double increment = (freq * PCM_SINE_TABLE_SIZE * PCM_FIXED_POINT_SCALE_FREQ) / PCM_SAMPLE_RATE;
        
        // Store as fixed point
        _haptics_hi_freq_increment[i] = (uint16_t)(increment + 0.5);
    }
    
    // Initialize high frequency table (320Hz center)
    for(int i = 0; i < 128; i++) {
        double linear = 0.03125 * i - 2.0;
        double freq = 320.0 * exp2f(linear);
        double increment = (freq * PCM_SINE_TABLE_SIZE * PCM_FIXED_POINT_SCALE_FREQ) / PCM_SAMPLE_RATE;
        _haptics_lo_freq_increment[i] = (uint16_t)(increment + 0.5);
    }
}

/* Original amplitude lookup table algorithm
    void initialize_rumble_amp_lookup(void)
    {
        for (size_t i = 0; i < 128; ++i)
        {
            if (i == 0)
            {
                RumbleAmpLookup[i] = -8.0f;
            }
            else if (i < 16)
            {
                RumbleAmpLookup[i] = 0.25f * i - 7.75f;
            }
            else if (i < 32)
            {
                RumbleAmpLookup[i] = 0.0625f * i - 4.9375f;
            }
            else
            {
                RumbleAmpLookup[i] = 0.03125f * i - 3.96875f;
            }
        }
    }
*/

// Q16.16 fixed-point amplitude lookup table
static uint32_t _haptics_amplitude_fp[128];

// Initialize fixed point amplitude lookup table
void _init_fp_amp_lookup(void)
{
    for (size_t i = 0; i < 128; ++i)
    {
        float tmp = 0;

        if (i == 0)
        {
            tmp = -8.0f;
        }
        else if (i < 16)
        {
            tmp = 0.25f * i - 7.75f;
        }
        else if (i < 32)
        {
            tmp = 0.0625f * i - 4.9375f;
        }
        else
        {
            tmp = 0.03125f * i - 3.96875f;
        }

        tmp = exp2f(tmp);

        _haptics_amplitude_fp[i] = float_to_q1616(tmp);
    }
}

/*
    Here's our original command table
    const Switch5BitCommand_s CommandTable[] = {
            {.am_action = Switch5BitAction_Default,     .fm_action = Switch5BitAction_Default, .am_offset = 0.0f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Substitute,  .fm_action = Switch5BitAction_Ignore, .am_offset = 0.0f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Substitute,  .fm_action = Switch5BitAction_Ignore, .am_offset = -0.5f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Substitute,  .fm_action = Switch5BitAction_Ignore, .am_offset = -1.0f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Substitute,  .fm_action = Switch5BitAction_Ignore, .am_offset = -1.5f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Substitute,  .fm_action = Switch5BitAction_Ignore, .am_offset = -2.0f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Substitute,  .fm_action = Switch5BitAction_Ignore, .am_offset = -2.5f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Substitute,  .fm_action = Switch5BitAction_Ignore, .am_offset = -3.0f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Substitute,  .fm_action = Switch5BitAction_Ignore, .am_offset = -3.5f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Substitute,  .fm_action = Switch5BitAction_Ignore, .am_offset = -4.0f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Substitute,  .fm_action = Switch5BitAction_Ignore, .am_offset = -4.5f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Substitute,  .fm_action = Switch5BitAction_Ignore, .am_offset = -5.0f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Ignore,      .fm_action = Switch5BitAction_Substitute, .am_offset = 0.0f, .fm_offset = -0.375f},
            {.am_action = Switch5BitAction_Ignore,      .fm_action = Switch5BitAction_Substitute, .am_offset = 0.0f, .fm_offset = -0.1875f},
            {.am_action = Switch5BitAction_Ignore,      .fm_action = Switch5BitAction_Substitute, .am_offset = 0.0f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Ignore,      .fm_action = Switch5BitAction_Substitute, .am_offset = 0.0f, .fm_offset = 0.1875f},
            {.am_action = Switch5BitAction_Ignore,      .fm_action = Switch5BitAction_Substitute, .am_offset = 0.0f, .fm_offset = 0.375f},
            {.am_action = Switch5BitAction_Sum,         .fm_action = Switch5BitAction_Sum, .am_offset = 0.125f, .fm_offset = 0.03125f},
            {.am_action = Switch5BitAction_Sum,         .fm_action = Switch5BitAction_Ignore, .am_offset = 0.125f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Sum,         .fm_action = Switch5BitAction_Sum, .am_offset = 0.125f, .fm_offset = -0.03125f},
            {.am_action = Switch5BitAction_Sum,         .fm_action = Switch5BitAction_Sum, .am_offset = 0.03125f, .fm_offset = 0.03125f},
            {.am_action = Switch5BitAction_Sum,         .fm_action = Switch5BitAction_Ignore, .am_offset = 0.03125f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Sum,         .fm_action = Switch5BitAction_Sum, .am_offset = 0.03125f, .fm_offset = -0.03125f},
            {.am_action = Switch5BitAction_Ignore,      .fm_action = Switch5BitAction_Sum, .am_offset = 0.0f, .fm_offset = 0.03125f},
            {.am_action = Switch5BitAction_Ignore,      .fm_action = Switch5BitAction_Ignore, .am_offset = 0.0f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Ignore,      .fm_action = Switch5BitAction_Sum, .am_offset = 0.0f, .fm_offset = -0.03125f},
            {.am_action = Switch5BitAction_Sum,         .fm_action = Switch5BitAction_Sum, .am_offset = -0.03125f, .fm_offset = 0.03125f},
            {.am_action = Switch5BitAction_Sum,         .fm_action = Switch5BitAction_Ignore, .am_offset = -0.03125f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Sum,         .fm_action = Switch5BitAction_Sum, .am_offset = -0.03125f, .fm_offset = -0.03125f},
            {.am_action = Switch5BitAction_Sum,         .fm_action = Switch5BitAction_Sum, .am_offset = -0.125f, .fm_offset = 0.03125f},
            {.am_action = Switch5BitAction_Sum,         .fm_action = Switch5BitAction_Ignore, .am_offset = -0.125f, .fm_offset = 0.0f},
            {.am_action = Switch5BitAction_Sum,         .fm_action = Switch5BitAction_Sum, .am_offset = -0.125f, .fm_offset = -0.03125f}
        };

    Basically, since this table uses the floating point values, we need to convert them to fixed point values, but only for amplitude
    The frequency offsets can actually be represented as simple index shift values, since we will use those to index
    into our sine step freqency lookup table to get the actual phase increment values for hi/low frequencies respectively 

    We used these functions

    Create the offset index shift for frequencies
    int8_t freq_offset_to_shift(float offset) 
    {
        // 0.03125 is our step size, so divide by it to get index shift
        return (int8_t)(offset / 0.03125f + 0.5f);  // +0.5f for rounding
    }

    Convert amplitude offset to fixed point
    int32_t amp_offset_to_fixed(float offset) {
        // Convert from dB to linear scale
        float scale = pow(2.0f, offset);
        // Convert to Q8.8 fixed point

        return (int16_t)(scale * 256.0f + 0.5f);
    }
*/

// Here's the command table we use with fixed point values
// The fm action does an index offset to access the correct phase increment value
// The am action does a fixed point amplitude offset. We ran the old values through the conversion function
// to get the fixed point values we need
static const Switch5BitCommand_s CommandTable[32] = {
    {.fm_offset =   0, .fm_action = 1, .am_offset = 0x0000, .am_action = 1},  // [ 0]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0x0000, .am_action = 2},  // [ 1]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0xFFFFB504, .am_action = 2},  // [ 2]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0xFFFF8000, .am_action = 2},  // [ 3]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0x5A82, .am_action = 2},  // [ 4]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0x4000, .am_action = 2},  // [ 5]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0x2D41, .am_action = 2},  // [ 6]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0x2000, .am_action = 2},  // [ 7]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0x16A0, .am_action = 2},  // [ 8]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0x1000, .am_action = 2},  // [ 9]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0x0B50, .am_action = 2},  // [10]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0x0800, .am_action = 2},  // [11]
    {.fm_offset = -11, .fm_action = 2, .am_offset = 0x0000, .am_action = 0},  // [12]
    {.fm_offset =  -5, .fm_action = 2, .am_offset = 0x0000, .am_action = 0},  // [13]
    {.fm_offset =   0, .fm_action = 2, .am_offset = 0x0000, .am_action = 0},  // [14]
    {.fm_offset =   6, .fm_action = 2, .am_offset = 0x0000, .am_action = 0},  // [15]
    {.fm_offset =  12, .fm_action = 2, .am_offset = 0x0000, .am_action = 0},  // [16]
    {.fm_offset =   1, .fm_action = 3, .am_offset = 0x172B, .am_action = 3},  // [17]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0x172B, .am_action = 3},  // [18]
    {.fm_offset =   0, .fm_action = 3, .am_offset = 0x172B, .am_action = 3},  // [19]
    {.fm_offset =   1, .fm_action = 3, .am_offset = 0x059B, .am_action = 3},  // [20]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0x059B, .am_action = 3},  // [21]
    {.fm_offset =   0, .fm_action = 3, .am_offset = 0x059B, .am_action = 3},  // [22]
    {.fm_offset =   1, .fm_action = 3, .am_offset = 0x0000, .am_action = 0},  // [23]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0x0000, .am_action = 0},  // [24]
    {.fm_offset =   0, .fm_action = 3, .am_offset = 0x0000, .am_action = 0},  // [25]
    {.fm_offset =   1, .fm_action = 3, .am_offset = 0xFFFFFA83, .am_action = 3},  // [26]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0xFFFFFA83, .am_action = 3},  // [27]
    {.fm_offset =   0, .fm_action = 3, .am_offset = 0xFFFFFA83, .am_action = 3},  // [28]
    {.fm_offset =   1, .fm_action = 3, .am_offset = 0xFFFFEAC0, .am_action = 3},  // [29]
    {.fm_offset =   0, .fm_action = 0, .am_offset = 0xFFFFEAC0, .am_action = 3},  // [30]
    {.fm_offset =   0, .fm_action = 3, .am_offset = 0xFFFFEAC0, .am_action = 3},  // [31]
};

typedef struct 
{
    uint8_t default_amplitude;
    uint8_t min_amplitude;
    uint8_t max_amplitude;
    uint8_t starting_amplitude;
    uint8_t default_frequency;
    uint8_t min_frequency;
    uint8_t max_frequency;
} haptic_defaults_s;

haptic_defaults_s _haptic_defaults = {
    .default_amplitude = 0,
    .min_amplitude = 0,
    .max_amplitude = 127,
    .starting_amplitude = 0,
    .default_frequency = 64,
    .min_frequency = 0,
    .max_frequency = 127
};

// For amplitude
static inline uint8_t _apply_command_amp(Action_t action, 
                                       int32_t offset, 
                                       uint8_t current) {
    int32_t result;
    int shift;

    if(current < 16) 
    {
        // 0.25f slope range (1/0.25 = 4 bit shift)
        shift = 2;
    }
    else if(current < 32) 
    {
        // 0.0625f slope range (1/0.0625 = 16 bit shift)
        shift = 4;
    }
    else 
    {
        // 0.03125f slope range (1/0.03125 = 32 bit shift)
        shift = 5;
    }

    int32_t shifted_offset = offset >> shift;
    int32_t new_index = current + shifted_offset;
    
    switch (action) {
        case Action_Ignore:
            return current;
            
        case Action_Substitute:
            return (uint8_t) CLAMP(new_index, 0, 127);
            
        case Action_Sum:
            result = (int32_t) current + new_index;
            result = CLAMP(result, 0, 127);
            return (uint8_t) result;
            
        default: // Action_Default
            return 0;
    }
}

// For frequency
static inline uint8_t _apply_command_freq(Action_t action, 
                                        int16_t offset, 
                                        int16_t current) {
    int16_t result;
    
    switch (action) {
        case Action_Ignore:
            return (uint8_t) current;
            
        case Action_Substitute:
            return (uint8_t) offset;
            
        case Action_Sum:
            // For multiplication in Q8.8
            result = current + offset;
            result = CLAMP(result, _haptic_defaults.min_frequency, _haptic_defaults.max_frequency);
            return (uint8_t) result;
            
        default: // Action_Default
            return _haptic_defaults.default_frequency;
    }
}


// Decode the type 1 packet
void _haptics_decode_type_1(const SwitchHapticPacket_s *encoded, haptic_raw_state_s *out)
{
    uint8_t samples = encoded->frame_count;
    out->sample_count = samples;

    // decoded->count = 3;
    Switch5BitCommand_s hi_cmd = {0};
    Switch5BitCommand_s low_cmd = {0};

    // Decode sample 0
    if (samples > 0)
    {   
        // Get the command from the table
        hi_cmd = CommandTable[encoded->type1.cmd_hi_0];
        
        out->state.hi_frequency_idx     = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx     = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        // Get the command from the table
        low_cmd = CommandTable[encoded->type1.cmd_lo_0];

        out->state.lo_frequency_idx     = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx     = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        // Apply the values to the data
        memcpy(&out->samples[0], &out->state, sizeof(haptic_raw_s));
    }

    // Decode sample 1
    if (samples > 1)
    {
        // Get the command from the table
        hi_cmd = CommandTable[encoded->type1.cmd_hi_1];

        out->state.hi_frequency_idx     = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx     = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        // Get the command from the table
        low_cmd = CommandTable[encoded->type1.cmd_lo_1];

        out->state.lo_frequency_idx     = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx     = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        // Apply the values to the data
        memcpy(&out->samples[1], &out->state, sizeof(haptic_raw_s));
    }

    // Decode sample 2
    if (samples > 2)
    {
        // Get the command from the table
        hi_cmd = CommandTable[encoded->type1.cmd_hi_2];

        out->state.hi_frequency_idx     = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx   = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        low_cmd = CommandTable[encoded->type1.cmd_lo_2];

        out->state.lo_frequency_idx     = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx   = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        // Apply the values to the data
        memcpy(&out->samples[2], &out->state, sizeof(haptic_raw_s));
    }
}

// Decode the type 2 packet
void _haptics_decode_type_2(const SwitchHapticPacket_s *encoded, haptic_raw_state_s *out)
{
    uint8_t samples = encoded->frame_count;
    out->sample_count = samples;

    out->state.hi_frequency_idx = encoded->type2.freq_hi;
    out->state.lo_frequency_idx = encoded->type2.freq_lo;
    out->state.hi_amplitude_idx = _haptics_amplitude_fp[encoded->type2.amp_hi];
    out->state.lo_amplitude_idx = _haptics_amplitude_fp[encoded->type2.amp_lo];

    // Apply the values to the data
    memcpy(&out->samples[0], &out->state, sizeof(haptic_raw_s));
}

void _haptics_decode_type_3(const SwitchHapticPacket_s *encoded, haptic_raw_state_s *out)
{
    // decoded->count = 2;
    Switch5BitCommand_s hi_cmd = {0};
    Switch5BitCommand_s low_cmd = {0};

    uint8_t samples = encoded->frame_count;
    out->sample_count = samples;

    // Decode sample 0
    if (samples > 0)
    {
        if (encoded->type3.high_select)
        {
            out->state.hi_frequency_idx = encoded->type3.freq_xx_0;
            out->state.hi_amplitude_idx = _haptics_amplitude_fp[encoded->type3.amp_xx_0];

            // Get the command from the table
            low_cmd = CommandTable[encoded->type3.cmd_xx_0];

            out->state.lo_frequency_idx     = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
            out->state.lo_amplitude_idx   = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);
        }
        else
        {
            out->state.lo_frequency_idx = encoded->type3.freq_xx_0;
            out->state.lo_amplitude_idx = _haptics_amplitude_fp[encoded->type3.amp_xx_0];

            // Get the command from the table
            hi_cmd = CommandTable[encoded->type3.cmd_xx_0];

            out->state.hi_frequency_idx     = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
            out->state.hi_amplitude_idx   = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);
        }
        
        // Apply the values to the data
        memcpy(&out->samples[0], &out->state, sizeof(haptic_raw_s));
    }

    // Decode sample 1
    if (samples > 1)
    {
        // Get the command from the table
        hi_cmd = CommandTable[encoded->type3.cmd_hi_1];

        out->state.hi_frequency_idx     = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx   = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        // Get the command from the table
        low_cmd = CommandTable[encoded->type3.cmd_lo_1];

        out->state.lo_frequency_idx     = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx   = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        // Apply the values to the data
        memcpy(&out->samples[1], &out->state, sizeof(haptic_raw_s));
    }
}

void _haptics_decode_type_4(const SwitchHapticPacket_s *encoded, haptic_raw_state_s *out)
{
    Switch5BitCommand_s hi_cmd = {0};
    Switch5BitCommand_s low_cmd = {0};

    uint8_t samples = encoded->frame_count;
    out->sample_count = samples;

    // Decode sample 0
    if (samples > 0)
    {
        if (encoded->type4.high_select)
        {
            if (encoded->type4.freq_select)
            {
                out->state.hi_frequency_idx = encoded->type4.xx_xx_0;
            }
            else
            {
                out->state.hi_amplitude_idx = _haptics_amplitude_fp[encoded->type4.xx_xx_0];
            }
        }
        else
        {
            if (encoded->type4.freq_select)
            {
                out->state.lo_frequency_idx = encoded->type4.xx_xx_0;
            }
            else
            {
                out->state.lo_amplitude_idx = _haptics_amplitude_fp[encoded->type4.xx_xx_0];
            }
        }

        // Apply the values to the data
        memcpy(&out->samples[0], &out->state, sizeof(haptic_raw_s));
    }

    // Decode sample 1
    if (samples > 1)
    {
        // Get the command from the table
        hi_cmd = CommandTable[encoded->type4.cmd_hi_1];

        out->state.hi_frequency_idx     = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx   = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        // Get the command from the table
        low_cmd = CommandTable[encoded->type4.cmd_lo_1];

        out->state.lo_frequency_idx     = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx   = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        // Apply the values to the data
        memcpy(&out->samples[1], &out->state, sizeof(haptic_raw_s));
    }

    // Decode sample 2
    if (samples > 2)
    {
        // Get the command from the table
        hi_cmd = CommandTable[encoded->type4.cmd_hi_2];


        out->state.hi_frequency_idx     = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx   = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        // Get the command from the table
        low_cmd = CommandTable[encoded->type4.cmd_lo_2];

        out->state.lo_frequency_idx     = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx   = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        // Apply the values to the data
        memcpy(&out->samples[2], &out->state, sizeof(haptic_raw_s));
    }
}

haptic_raw_state_s _raw_state = {0};

void switch_haptics_init() 
{
    _init_fp_amp_lookup();
    _init_frequency_phase_increment_tables();

    _raw_state.state.hi_amplitude_idx = _haptic_defaults.starting_amplitude;
    _raw_state.state.lo_amplitude_idx = _haptic_defaults.starting_amplitude;
    _raw_state.state.hi_frequency_idx = _haptic_defaults.default_frequency;
    _raw_state.state.lo_frequency_idx = _haptic_defaults.default_frequency;

    // Initialize the default values
    for(int i = 0; i < 3; i++) 
    {
        _raw_state.samples[i].hi_amplitude_idx = _haptic_defaults.starting_amplitude;
        _raw_state.samples[i].lo_amplitude_idx = _haptic_defaults.starting_amplitude;
        _raw_state.samples[i].hi_frequency_idx = _haptic_defaults.default_frequency;
        _raw_state.samples[i].lo_frequency_idx = _haptic_defaults.default_frequency;
    }
}

// This will detect and call the appropriate decoding schema
void _haptics_decode_samples(const SwitchHapticPacket_s *encoded)
{
    switch (encoded->frame_count)
    {
        case 0:
            _raw_state.state.hi_amplitude_idx = 0;
            _raw_state.sample_count = 0;
            break;
        case 1:
            if ((encoded->data & 0xFFFFF) == 0)
            {
                _haptics_decode_type_1(encoded, &_raw_state);
            }
            else if ((encoded->data & 0x3) == 0)
            {
                _haptics_decode_type_2(encoded, &_raw_state);
            }
            else if ((encoded->data & 0x2) == 2)
            {
                _haptics_decode_type_4(encoded, &_raw_state);
            }
            break;
        case 2:
            if ((encoded->data & 0x3FF) == 0)
            {
                _haptics_decode_type_1(encoded, &_raw_state);
            }
            else
            {
                _haptics_decode_type_3(encoded, &_raw_state);
            }
            break;
        case 3:
            _haptics_decode_type_1(encoded, &_raw_state);
            break;
    };
}

void switch_haptics_rumble_translate(const uint8_t *data)
{
    _haptics_decode_samples((const SwitchHapticPacket_s *)data);

    haptic_processed_s processed = {0};

    if(_raw_state.sample_count > 0) 
    {
        for(int i = 0; i < _raw_state.sample_count; i++) 
        {
            processed.hi_amplitude_fixed        = _haptics_amplitude_fp[_raw_state.samples[i].hi_amplitude_idx];
            processed.lo_amplitude_fixed        = _haptics_amplitude_fp[_raw_state.samples[i].lo_amplitude_idx];
            processed.hi_frequency_increment    = _haptics_hi_freq_increment[_raw_state.samples[i].hi_frequency_idx];
            processed.lo_frequency_increment    = _haptics_lo_freq_increment[_raw_state.samples[i].lo_frequency_idx];

            pcm_amfm_push(&processed);
        }
    }

}



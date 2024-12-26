#include "switch/switch_haptics.h"
#include <string.h>
#include "devices_shared_types.h"
#include <math.h>
#include "utilities/pcm.h"

#if defined(HOJA_CONFIG_HDRUMBLE) && (HOJA_CONFIG_HDRUMBLE==1)
#include "hal/hdrumble_hal.h"
#elif defined(HOJA_CONFIG_SDRUMBLE) && (HOJA_CONFIG_SDRUMBLE==1)
#include "hal/sdrumble_hal.h"
#endif

typedef struct 
{
    int16_t default_amplitude;
    int16_t min_amplitude;
    int16_t max_amplitude;
    int16_t starting_amplitude;
    uint8_t default_frequency;
    uint8_t min_frequency;
    uint8_t max_frequency;
} haptic_defaults_s;

haptic_defaults_s _haptic_defaults = {
    .default_amplitude  = 0,
    .min_amplitude      = 0,
    .max_amplitude      = 255,
    .starting_amplitude = 2, // -7.9375f
    .default_frequency  = 64,
    .min_frequency      = 0,
    .max_frequency      = 127
};


int16_t float_to_fixed(float input) {
   return (int16_t)(input * PCM_AMPLITUDE_SHIFT_FIXED);
}

/* We used to use this ExpBase lookup table made with this data
    #define EXP_BASE2_RANGE_START (-8.0f)
    #define EXP_BASE2_RANGE_END (2.0f)
    #define EXP_BASE2_LOOKUP_RESOLUTION (1 / 32.0f)
    #define EXP_BASE2_LOOKUP_LENGTH 320 //((size_t)(((EXP_BASE2_RANGE_END - EXP_BASE2_RANGE_START) + EXP_BASE2_LOOKUP_RESOLUTION) / EXP_BASE2_LOOKUP_RESOLUTION))

    static float _ExpBase2Lookup[EXP_BASE2_LOOKUP_LENGTH];
    static float RumbleAmpLookup[128];
    static float RumbleFreqLookup[128];

    void initialize_exp_base2_lookup(void)
    {
        for (size_t i = 0; i < EXP_BASE2_LOOKUP_LENGTH; ++i)
        {
            float f = EXP_BASE2_RANGE_START + i * EXP_BASE2_LOOKUP_RESOLUTION;
            if (f >= StartingAmplitude)
            {
                _ExpBase2Lookup[i] = exp2f(f);
            }
            else
            {
                _ExpBase2Lookup[i] = 0.0f;
            }
        }
    }
*/

#define AMPLITUDE_RANGE_START       -8.0f
#define AMPLITUDE_INTERVAL          0.03125f
#define STARTING_AMPLITUDE_FLOAT    -7.9375f
#define EXP_BASE2_LOOKUP_LENGTH     256

#define LO_FREQUENCY_MINIMUM    PCM_LO_FREQUENCY_MIN
#define HI_FREQUENCY_MINIMUM    PCM_HI_FREQUENCY_MIN
#define LO_FREQUENCY_RANGE      PCM_LO_FREQUENCY_RANGE
#define HI_FREQUENCY_RANGE      PCM_HI_FREQUENCY_RANGE

// Q1.15 fixed-point amplitude lookup table (Post-exp2f)
static int16_t _ExpBase2LookupHi[EXP_BASE2_LOOKUP_LENGTH];
static int16_t _ExpBase2LookupLo[EXP_BASE2_LOOKUP_LENGTH];

void _initialize_exp_base2_lookup(uint8_t user_intensity) {

    float intensity = user_intensity / 255.0f;
    float scaledRangeHi = intensity * HI_FREQUENCY_RANGE;
    float scaledRangeLo = intensity * LO_FREQUENCY_RANGE;

    for (int i = 0; i < EXP_BASE2_LOOKUP_LENGTH; ++i) {
        float f = AMPLITUDE_RANGE_START + i * AMPLITUDE_INTERVAL;
        if (f >= STARTING_AMPLITUDE_FLOAT) {
            _ExpBase2LookupHi[i] = float_to_fixed( (exp2f(f) * HI_FREQUENCY_RANGE) + HI_FREQUENCY_MINIMUM);
            _ExpBase2LookupLo[i] = float_to_fixed( (exp2f(f) * LO_FREQUENCY_RANGE) + LO_FREQUENCY_MINIMUM);
        } else {
            _ExpBase2LookupHi[i] = 0;
            _ExpBase2LookupLo[i] = 0;
        }
    }
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

// Q8.8 fixed-point sine-increment table
void _init_frequency_phase_increment_tables(void) {

    // Initialize low frequency table (160Hz center)
    for(int i = 0; i < 128; i++) {
        // Calculate the linear exponent
        double linear = 0.03125 * i - 2.0;
        
        // Calculate actual frequency
        double freq = 160.0 * exp2f(linear);
        
        // Calculate phase increment float
        float increment = (freq * PCM_SINE_TABLE_SIZE) / PCM_SAMPLE_RATE; 

        // Scale to fixed point
        uint16_t fixed_increment = (uint16_t)(increment * PCM_FREQUENCY_SHIFT_FIXED + 0.5);
        
        // Store as fixed point
        _haptics_lo_freq_increment[i] = fixed_increment;
    }
    
    // Initialize high frequency table (320Hz center)
    for(int i = 0; i < 128; i++) {
        // Calculate the linear exponent
        double linear = 0.03125 * i - 2.0;

        // Calculate actual frequency
        double freq = 320.0 * exp2f(linear);

        // Calculate phase increment float
        float increment = (freq * PCM_SINE_TABLE_SIZE) / PCM_SAMPLE_RATE; 

        // Scale to fixed point
        uint16_t fixed_increment = (uint16_t)(increment * PCM_FREQUENCY_SHIFT_FIXED + 0.5);

        // Store as fixed point
        _haptics_hi_freq_increment[i] = fixed_increment;
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

// Amplitude lookup table
// Each entry is an index representing a value in our 
// _ExpBase2Lookup table
static uint8_t _haptics_amplitude_index[128];

// Initialize fixed point amplitude lookup table
void _init_amp_idx_lookup()
{
    for (int i = 0; i < 128; ++i)
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

        // Get our final output index 
        float fidx = (tmp/AMPLITUDE_INTERVAL);

        int16_t idx = 255 + (int16_t) fidx;

        // Normalize to positive
        if(!i) idx = 0;
        if(idx > 255) idx = 255;
        if(idx < 0) idx = 0;

        _haptics_amplitude_index[i] = idx;
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

    See command table conversion.txt to see our C code to convert the old to new
*/

// Here's the command table we use with fixed point values
// The fm action and am action does an index offset to access the correct phase increment value
const Switch5BitCommand_s CommandTable[] = {
    {.am_action = Action_Default, .fm_action = Action_Default, .am_offset = 0, .fm_offset = 0},
    {.am_action = Action_Substitute, .fm_action = Action_Ignore, .am_offset = 0, .fm_offset = 0},
    {.am_action = Action_Substitute, .fm_action = Action_Ignore, .am_offset = 240, .fm_offset = 0},
    {.am_action = Action_Substitute, .fm_action = Action_Ignore, .am_offset = 224, .fm_offset = 0},
    {.am_action = Action_Substitute, .fm_action = Action_Ignore, .am_offset = 208, .fm_offset = 0},
    {.am_action = Action_Substitute, .fm_action = Action_Ignore, .am_offset = 192, .fm_offset = 0},
    {.am_action = Action_Substitute, .fm_action = Action_Ignore, .am_offset = 176, .fm_offset = 0},
    {.am_action = Action_Substitute, .fm_action = Action_Ignore, .am_offset = 160, .fm_offset = 0},
    {.am_action = Action_Substitute, .fm_action = Action_Ignore, .am_offset = 144, .fm_offset = 0},
    {.am_action = Action_Substitute, .fm_action = Action_Ignore, .am_offset = 128, .fm_offset = 0},
    {.am_action = Action_Substitute, .fm_action = Action_Ignore, .am_offset = 112, .fm_offset = 0},
    {.am_action = Action_Substitute, .fm_action = Action_Ignore, .am_offset = 96, .fm_offset = 0},
    {.am_action = Action_Ignore, .fm_action = Action_Substitute, .am_offset = 0, .fm_offset = 5},
    {.am_action = Action_Ignore, .fm_action = Action_Substitute, .am_offset = 0, .fm_offset = 5},
    {.am_action = Action_Ignore, .fm_action = Action_Substitute, .am_offset = 0, .fm_offset = 0},
    {.am_action = Action_Ignore, .fm_action = Action_Substitute, .am_offset = 0, .fm_offset = 7},
    {.am_action = Action_Ignore, .fm_action = Action_Substitute, .am_offset = 0, .fm_offset = 7},
    {.am_action = Action_Sum, .fm_action = Action_Sum, .am_offset = 4, .fm_offset = 1},
    {.am_action = Action_Sum, .fm_action = Action_Ignore, .am_offset = 4, .fm_offset = 0},
    {.am_action = Action_Sum, .fm_action = Action_Sum, .am_offset = 4, .fm_offset = -1},
    {.am_action = Action_Sum, .fm_action = Action_Sum, .am_offset = 1, .fm_offset = 1},
    {.am_action = Action_Sum, .fm_action = Action_Ignore, .am_offset = 1, .fm_offset = 0},
    {.am_action = Action_Sum, .fm_action = Action_Sum, .am_offset = 1, .fm_offset = -1},
    {.am_action = Action_Ignore, .fm_action = Action_Sum, .am_offset = 0, .fm_offset = 1},
    {.am_action = Action_Ignore, .fm_action = Action_Ignore, .am_offset = 0, .fm_offset = 0},
    {.am_action = Action_Ignore, .fm_action = Action_Sum, .am_offset = 0, .fm_offset = -1},
    {.am_action = Action_Sum, .fm_action = Action_Sum, .am_offset = -1, .fm_offset = 1},
    {.am_action = Action_Sum, .fm_action = Action_Ignore, .am_offset = -1, .fm_offset = 0},
    {.am_action = Action_Sum, .fm_action = Action_Sum, .am_offset = -1, .fm_offset = -1},
    {.am_action = Action_Sum, .fm_action = Action_Sum, .am_offset = -4, .fm_offset = 1},
    {.am_action = Action_Sum, .fm_action = Action_Ignore, .am_offset = -4, .fm_offset = 0},
    {.am_action = Action_Sum, .fm_action = Action_Sum, .am_offset = -4, .fm_offset = -1},
};

// For amplitude
static inline uint8_t _apply_command_amp(Action_t action, 
                                       int16_t offset, 
                                       uint8_t current) {
    int32_t result;
    
    switch (action) {
        case Action_Ignore:
            return (uint8_t) current;
            
        case Action_Substitute:
            return (uint8_t) offset;
            
        case Action_Sum:
            result = (int32_t) current + offset;
            result = CLAMP(result, _haptic_defaults.min_amplitude, _haptic_defaults.max_amplitude);
            return (uint8_t) result;
            
        default: // Action_Default
            return (uint8_t) _haptic_defaults.default_amplitude;
    }
}

// For frequency
static inline uint8_t _apply_command_freq(Action_t action, 
                                        int16_t offset, 
                                        uint8_t current) {
    int32_t result;
    
    switch (action) {
        case Action_Ignore:
            return (uint8_t) current;
            
        case Action_Substitute:
            return (uint8_t) offset;
            
        case Action_Sum:
            result = (int32_t) current + offset;
            result = CLAMP(result, _haptic_defaults.min_frequency, _haptic_defaults.max_frequency);
            return (uint8_t) result;
            
        default: // Action_Default
            return (uint8_t) _haptic_defaults.default_frequency;
    }
}


// Decode the type 1 packet
void _haptics_decode_type_1(const SwitchHapticPacket_s *encoded, haptic_raw_state_s *out)
{
    uint8_t samples = encoded->frame_count;
    out->sample_count = samples;

    // decoded->count = 3;
    Switch5BitCommand_s hi_cmd  = {0};
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
        out->state.hi_amplitude_idx     = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        low_cmd = CommandTable[encoded->type1.cmd_lo_2];

        out->state.lo_frequency_idx     = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx     = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

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
    out->state.hi_amplitude_idx = _haptics_amplitude_index[encoded->type2.amp_hi];
    out->state.lo_amplitude_idx = _haptics_amplitude_index[encoded->type2.amp_lo];

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
            out->state.hi_amplitude_idx = _haptics_amplitude_index[encoded->type3.amp_xx_0];

            // Get the command from the table
            low_cmd = CommandTable[encoded->type3.cmd_xx_0];

            out->state.lo_frequency_idx   = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
            out->state.lo_amplitude_idx   = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);
        }
        else
        {
            out->state.lo_frequency_idx = encoded->type3.freq_xx_0;
            out->state.lo_amplitude_idx = _haptics_amplitude_index[encoded->type3.amp_xx_0];

            // Get the command from the table
            hi_cmd = CommandTable[encoded->type3.cmd_xx_0];

            out->state.hi_frequency_idx   = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
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

        out->state.hi_frequency_idx   = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx   = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        // Get the command from the table
        low_cmd = CommandTable[encoded->type3.cmd_lo_1];

        out->state.lo_frequency_idx   = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
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
                out->state.hi_amplitude_idx = _haptics_amplitude_index[encoded->type4.xx_xx_0];
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
                out->state.lo_amplitude_idx = _haptics_amplitude_index[encoded->type4.xx_xx_0];
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

        out->state.hi_frequency_idx   = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx   = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        // Get the command from the table
        low_cmd = CommandTable[encoded->type4.cmd_lo_1];

        out->state.lo_frequency_idx   = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx   = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        // Apply the values to the data
        memcpy(&out->samples[1], &out->state, sizeof(haptic_raw_s));
    }

    // Decode sample 2
    if (samples > 2)
    {
        // Get the command from the table
        hi_cmd = CommandTable[encoded->type4.cmd_hi_2];


        out->state.hi_frequency_idx   = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx   = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        // Get the command from the table
        low_cmd = CommandTable[encoded->type4.cmd_lo_2];

        out->state.lo_frequency_idx   = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx   = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        // Apply the values to the data
        memcpy(&out->samples[2], &out->state, sizeof(haptic_raw_s));
    }
}

haptic_raw_state_s _raw_state = {0};

void switch_haptics_init(uint8_t user_intensity) 
{
    _init_amp_idx_lookup();
    _init_frequency_phase_increment_tables();
    _initialize_exp_base2_lookup(user_intensity);

    _raw_state.state.hi_amplitude_idx   = _haptic_defaults.starting_amplitude;
    _raw_state.state.lo_amplitude_idx   = _haptic_defaults.starting_amplitude;
    _raw_state.state.hi_frequency_idx   = _haptic_defaults.default_frequency;
    _raw_state.state.lo_frequency_idx   = _haptic_defaults.default_frequency;

    // Initialize the default values
    for(int i = 0; i < 3; i++) 
    {
        _raw_state.samples[i].hi_amplitude_idx = _haptic_defaults.starting_amplitude;
        _raw_state.samples[i].lo_amplitude_idx = _haptic_defaults.starting_amplitude;
        _raw_state.samples[i].hi_frequency_idx  = _haptic_defaults.default_frequency;
        _raw_state.samples[i].lo_frequency_idx  = _haptic_defaults.default_frequency;
    }
}

// This will detect and call the appropriate decoding schema
void _haptics_decode_samples(const SwitchHapticPacket_s *encoded)
{
    static SwitchHapticPacket_s last_packet = {0};

    if(encoded->data == last_packet.data) return;
    last_packet.data = encoded->data;

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
    // Avoid doing anything if we don't have this defined
    #if !defined(HOJA_HAPTICS_PUSH_AMFM)
    return;
    #endif

    _haptics_decode_samples((const SwitchHapticPacket_s *)data);

    haptic_processed_s processed = {0};

    if(_raw_state.sample_count > 0) 
    {
        for(int i = 0; i < _raw_state.sample_count; i++) 
        {
            processed.hi_amplitude_fixed        = _ExpBase2LookupHi[_raw_state.samples[i].hi_amplitude_idx];
            processed.lo_amplitude_fixed        = _ExpBase2LookupLo[_raw_state.samples[i].lo_amplitude_idx];
            processed.hi_frequency_increment    = _haptics_hi_freq_increment[_raw_state.samples[i].hi_frequency_idx];
            processed.lo_frequency_increment    = _haptics_lo_freq_increment[_raw_state.samples[i].lo_frequency_idx];
            
            #if defined(HOJA_HAPTICS_PUSH_AMFM)
            HOJA_HAPTICS_PUSH_AMFM(&processed);
            #endif
        }
    }
}



#include "switch_haptics.h"

const float MinFrequency = -2.0f;
const float MaxFrequency = 2.0f;
const float DefaultFrequency = 0.0f;
const float MinAmplitude = -8.0f;
const float MaxAmplitude = 0.0f;
const float DefaultAmplitude = -8.0f;
const float StartingAmplitude = -7.9375f;

const float CenterFreqHigh = 320.0f;
const float CenterFreqLow = 160.0f;

static inline float clampf(float val, float min, float max) {
    return (val < min) ? min : ((val > max) ? max : val);
}

const Switch5BitCommand_s CommandTable[] = {
    {.am_action = Switch5BitAction_Ignore, .fm_action = Switch5BitAction_Ignore, .am_offset = 0.0f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Substitute, .fm_action = Switch5BitAction_Ignore, .am_offset = 0.0f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Substitute, .fm_action = Switch5BitAction_Ignore, .am_offset = -0.5f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Substitute, .fm_action = Switch5BitAction_Ignore, .am_offset = -1.0f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Substitute, .fm_action = Switch5BitAction_Ignore, .am_offset = -1.5f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Substitute, .fm_action = Switch5BitAction_Ignore, .am_offset = -2.0f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Substitute, .fm_action = Switch5BitAction_Ignore, .am_offset = -2.5f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Substitute, .fm_action = Switch5BitAction_Ignore, .am_offset = -3.0f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Substitute, .fm_action = Switch5BitAction_Ignore, .am_offset = -3.5f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Substitute, .fm_action = Switch5BitAction_Ignore, .am_offset = -4.0f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Substitute, .fm_action = Switch5BitAction_Ignore, .am_offset = -4.5f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Substitute, .fm_action = Switch5BitAction_Ignore, .am_offset = -5.0f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Ignore, .fm_action = Switch5BitAction_Substitute, .am_offset = 0.0f, .fm_offset = -0.375},
    {.am_action = Switch5BitAction_Ignore, .fm_action = Switch5BitAction_Substitute, .am_offset = 0.0f, .fm_offset = -0.1875},
    {.am_action = Switch5BitAction_Ignore, .fm_action = Switch5BitAction_Substitute, .am_offset = 0.0f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Ignore, .fm_action = Switch5BitAction_Substitute, .am_offset = 0.0f, .fm_offset = 0.1875},
    {.am_action = Switch5BitAction_Ignore, .fm_action = Switch5BitAction_Substitute, .am_offset = 0.0f, .fm_offset = 0.375},
    {.am_action = Switch5BitAction_Sum, .fm_action = Switch5BitAction_Sum, .am_offset = 0.125f, .fm_offset = 0.03125},
    {.am_action = Switch5BitAction_Sum, .fm_action = Switch5BitAction_Ignore, .am_offset = 0.125f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Sum, .fm_action = Switch5BitAction_Sum, .am_offset = 0.125f, .fm_offset = -0.03125},
    {.am_action = Switch5BitAction_Sum, .fm_action = Switch5BitAction_Sum, .am_offset = 0.03125f, .fm_offset = 0.03125},
    {.am_action = Switch5BitAction_Sum, .fm_action = Switch5BitAction_Ignore, .am_offset = 0.03125f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Sum, .fm_action = Switch5BitAction_Sum, .am_offset = 0.03125f, .fm_offset = -0.03125},
    {.am_action = Switch5BitAction_Ignore, .fm_action = Switch5BitAction_Sum, .am_offset = 0.0f, .fm_offset = 0.03125},
    {.am_action = Switch5BitAction_Ignore, .fm_action = Switch5BitAction_Ignore, .am_offset = 0.0f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Ignore, .fm_action = Switch5BitAction_Sum, .am_offset = 0.0f, .fm_offset = -0.03125},
    {.am_action = Switch5BitAction_Sum, .fm_action = Switch5BitAction_Sum, .am_offset = -0.03125f, .fm_offset = 0.03125},
    {.am_action = Switch5BitAction_Sum, .fm_action = Switch5BitAction_Ignore, .am_offset = -0.03125f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Sum, .fm_action = Switch5BitAction_Sum, .am_offset = -0.03125f, .fm_offset = -0.03125},
    {.am_action = Switch5BitAction_Sum, .fm_action = Switch5BitAction_Sum, .am_offset = -0.125f, .fm_offset = 0.03125},
    {.am_action = Switch5BitAction_Sum, .fm_action = Switch5BitAction_Ignore, .am_offset = -0.125f, .fm_offset = 0.0},
    {.am_action = Switch5BitAction_Sum, .fm_action = Switch5BitAction_Sum, .am_offset = -0.125f, .fm_offset = -0.03125}};

#define EXP_BASE2_RANGE_START (-8.0f)
#define EXP_BASE2_RANGE_END (2.0f)
#define EXP_BASE2_LOOKUP_RESOLUTION (1 / 32.0f)
#define EXP_BASE2_LOOKUP_LENGTH 321 //((size_t)(((EXP_BASE2_RANGE_END - EXP_BASE2_RANGE_START) + EXP_BASE2_LOOKUP_RESOLUTION) / EXP_BASE2_LOOKUP_RESOLUTION))

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

void initialize_rumble_freq_lookup(void)
{
    for (size_t i = 0; i < 128; ++i)
    {
        RumbleFreqLookup[i] = 0.03125f * i - 2.0f;
    }
}

uint32_t haptics_get_lookup_index(float input)
{
    return (uint32_t)((input - EXP_BASE2_RANGE_START) / EXP_BASE2_LOOKUP_RESOLUTION);
}

float haptics_apply_command(Switch5BitAction_t action, float offset, float current, float default_val, float min, float max)
{
    switch (action)
    {
    case Switch5BitAction_Ignore:
        return current;
    case Switch5BitAction_Substitute:
        return offset;
    case Switch5BitAction_Sum:
        return clampf(current + offset, min, max);
    default:
        return current; //default_val;
    }
}

static bool _haptics_init = false;
// Call this function to initialize all lookup tables
void hatptics_initialize_lookup_tables(void)
{
    initialize_exp_base2_lookup();
    initialize_rumble_amp_lookup();
    initialize_rumble_freq_lookup();
}

bool haptics_disabled_check(uint8_t *data)
{
    if (data[0] & 0b00000001)
    {
        if (data[3] & 0b01000000)
        {
            return true;
        }
    }
    return false;
}

void haptics_linear_set_default(hoja_haptic_frame_linear_s *state)
{
    state->hi_amp_linear = DefaultAmplitude;
    state->lo_amp_linear = DefaultAmplitude;
    state->hi_freq_linear = DefaultFrequency;
    state->lo_freq_linear = DefaultFrequency;
}

// Functionally does what GetOutputValue does in the original documentation
void haptics_linear_to_normal(hoja_haptic_frame_linear_s *linear, hoja_haptic_frame_s *decoded)
{
    decoded->high_frequency = ExpBase2Lookup[haptics_get_lookup_index(linear->hi_freq_linear)] * CenterFreqHigh;
    decoded->low_frequency  = ExpBase2Lookup[haptics_get_lookup_index(linear->lo_freq_linear)] * CenterFreqLow;
    decoded->high_amplitude = ExpBase2Lookup[haptics_get_lookup_index(linear->hi_amp_linear)];
    decoded->low_amplitude  = ExpBase2Lookup[haptics_get_lookup_index(linear->lo_amp_linear)];
}

// Different decoding algorithms
void DecodeOne5Bit(const SwitchHapticPacket_s *encoded, hoja_rumble_msg_s *decoded)
{
    decoded->count = 1;
    Switch5BitCommand_s hi_cmd = {0};
    Switch5BitCommand_s low_cmd = {0};

    hi_cmd = CommandTable[encoded->one5bit.amfm_5bit_hi];
    decoded->linear.hi_freq_linear = haptics_apply_command(hi_cmd.fm_action, hi_cmd.fm_offset,
                                                           decoded->linear.hi_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
    decoded->linear.hi_amp_linear = haptics_apply_command(hi_cmd.am_action, hi_cmd.am_offset,
                                                          decoded->linear.hi_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

    low_cmd = CommandTable[encoded->one5bit.amfm_5bit_lo];
    decoded->linear.lo_freq_linear = haptics_apply_command(low_cmd.fm_action, low_cmd.fm_offset,
                                                           decoded->linear.lo_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
    decoded->linear.lo_amp_linear = haptics_apply_command(low_cmd.am_action, low_cmd.am_offset,
                                                          decoded->linear.lo_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

    haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[0]));
}

void DecodeOne7Bit(const SwitchHapticPacket_s *encoded, hoja_rumble_msg_s *decoded)
{
    decoded->count = 1;

    decoded->linear.hi_freq_linear = RumbleFreqLookup[encoded->one7bit.fm_7bit_hi];
    decoded->linear.lo_freq_linear = RumbleFreqLookup[encoded->one7bit.fm_7bit_lo];
    decoded->linear.hi_amp_linear = RumbleAmpLookup[encoded->one7bit.am_7bit_hi];
    decoded->linear.lo_amp_linear = RumbleAmpLookup[encoded->one7bit.am_7bit_lo];

    haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[0]));
}

void DecodeTwo5Bit(const SwitchHapticPacket_s *encoded, hoja_rumble_msg_s *decoded)
{
    decoded->count = 2;
    Switch5BitCommand_s hi_cmd = {0};
    Switch5BitCommand_s low_cmd = {0};

    // Decode sample 0
    {
        hi_cmd = CommandTable[encoded->two5bit.amfm_5bit_hi_0];
        decoded->linear.hi_freq_linear = haptics_apply_command(hi_cmd.fm_action, hi_cmd.fm_offset,
                                                               decoded->linear.hi_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.hi_amp_linear = haptics_apply_command(hi_cmd.am_action, hi_cmd.am_offset,
                                                              decoded->linear.hi_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        low_cmd = CommandTable[encoded->two5bit.amfm_5bit_lo_0];
        decoded->linear.lo_freq_linear = haptics_apply_command(low_cmd.fm_action, low_cmd.fm_offset,
                                                               decoded->linear.lo_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.lo_amp_linear = haptics_apply_command(low_cmd.am_action, low_cmd.am_offset,
                                                              decoded->linear.lo_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[0]));
    }

    // Decode sample 1
    {
        hi_cmd = CommandTable[encoded->two5bit.amfm_5bit_hi_1];
        decoded->linear.hi_freq_linear = haptics_apply_command(hi_cmd.fm_action, hi_cmd.fm_offset,
                                                               decoded->linear.hi_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.hi_amp_linear = haptics_apply_command(hi_cmd.am_action, hi_cmd.am_offset,
                                                              decoded->linear.hi_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        low_cmd = CommandTable[encoded->two5bit.amfm_5bit_lo_1];
        decoded->linear.lo_freq_linear = haptics_apply_command(low_cmd.fm_action, low_cmd.fm_offset,
                                                               decoded->linear.lo_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.lo_amp_linear = haptics_apply_command(low_cmd.am_action, low_cmd.am_offset,
                                                              decoded->linear.lo_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[1]));
    }
}

void DecodeTwo7Bit(const SwitchHapticPacket_s *encoded, hoja_rumble_msg_s *decoded)
{
    decoded->count = 2;
    Switch5BitCommand_s hi_cmd = {0};
    Switch5BitCommand_s low_cmd = {0};

    // Decode sample 0
    {
        if (encoded->two7bit.high_select)
        {
            decoded->linear.hi_freq_linear  = RumbleFreqLookup[encoded->two7bit.fm_7bit_xx];
            decoded->linear.hi_amp_linear   = RumbleAmpLookup[encoded->two7bit.am_7bit_xx];

            low_cmd = CommandTable[encoded->two7bit.am_7bit_xx];
            decoded->linear.lo_freq_linear = haptics_apply_command(low_cmd.fm_action, low_cmd.fm_offset,
                                                                   decoded->linear.lo_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
            decoded->linear.lo_amp_linear = haptics_apply_command(low_cmd.am_action, low_cmd.am_offset,
                                                                  decoded->linear.lo_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);
        }
        else
        {
            decoded->linear.lo_freq_linear = RumbleFreqLookup[encoded->two7bit.fm_7bit_xx];
            decoded->linear.lo_amp_linear = RumbleAmpLookup[encoded->two7bit.am_7bit_xx];

            hi_cmd = CommandTable[encoded->two7bit.am_7bit_xx];
            decoded->linear.hi_freq_linear = haptics_apply_command(hi_cmd.fm_action, hi_cmd.fm_offset,
                                                                   decoded->linear.hi_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
            decoded->linear.hi_amp_linear = haptics_apply_command(hi_cmd.am_action, hi_cmd.am_offset,
                                                                  decoded->linear.hi_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);
        }
        haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[0]));
    }

    // Decode sample 1
    {
        hi_cmd = CommandTable[encoded->two7bit.amfm_5bit_hi_1];
        decoded->linear.hi_freq_linear = haptics_apply_command(hi_cmd.fm_action, hi_cmd.fm_offset,
                                                               decoded->linear.hi_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.hi_amp_linear = haptics_apply_command(hi_cmd.am_action, hi_cmd.am_offset,
                                                              decoded->linear.hi_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        low_cmd = CommandTable[encoded->two7bit.amfm_5bit_lo_1];
        decoded->linear.lo_freq_linear = haptics_apply_command(low_cmd.fm_action, low_cmd.fm_offset,
                                                               decoded->linear.lo_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.lo_amp_linear = haptics_apply_command(low_cmd.am_action, low_cmd.am_offset,
                                                              decoded->linear.lo_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[1]));
    }
}

void DecodeThree5Bit(const SwitchHapticPacket_s *encoded, hoja_rumble_msg_s *decoded)
{
    decoded->count = 3;
    Switch5BitCommand_s hi_cmd = {0};
    Switch5BitCommand_s low_cmd = {0};

    // Decode sample 0
    {
        hi_cmd = CommandTable[encoded->three5bit.amfm_5bit_hi_0];
        decoded->linear.hi_freq_linear = haptics_apply_command(hi_cmd.fm_action, hi_cmd.fm_offset,
                                                               decoded->linear.hi_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.hi_amp_linear = haptics_apply_command(hi_cmd.am_action, hi_cmd.am_offset,
                                                              decoded->linear.hi_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        low_cmd = CommandTable[encoded->three5bit.amfm_5bit_lo_0];
        decoded->linear.lo_freq_linear = haptics_apply_command(low_cmd.fm_action, low_cmd.fm_offset,
                                                               decoded->linear.lo_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.lo_amp_linear = haptics_apply_command(low_cmd.am_action, low_cmd.am_offset,
                                                              decoded->linear.lo_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[0]));
    }

    // Decode sample 1
    {
        hi_cmd = CommandTable[encoded->three5bit.amfm_5bit_hi_1];
        decoded->linear.hi_freq_linear = haptics_apply_command(hi_cmd.fm_action, hi_cmd.fm_offset,
                                                               decoded->linear.hi_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.hi_amp_linear = haptics_apply_command(hi_cmd.am_action, hi_cmd.am_offset,
                                                              decoded->linear.hi_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        low_cmd = CommandTable[encoded->three5bit.amfm_5bit_lo_1];
        decoded->linear.lo_freq_linear = haptics_apply_command(low_cmd.fm_action, low_cmd.fm_offset,
                                                               decoded->linear.lo_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.lo_amp_linear = haptics_apply_command(low_cmd.am_action, low_cmd.am_offset,
                                                              decoded->linear.lo_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[1]));
    }

    // Decode sample 2
    {
        hi_cmd = CommandTable[encoded->three5bit.amfm_5bit_hi_2];
        decoded->linear.hi_freq_linear = haptics_apply_command(hi_cmd.fm_action, hi_cmd.fm_offset,
                                                               decoded->linear.hi_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.hi_amp_linear = haptics_apply_command(hi_cmd.am_action, hi_cmd.am_offset,
                                                              decoded->linear.hi_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        low_cmd = CommandTable[encoded->three5bit.amfm_5bit_lo_2];
        decoded->linear.lo_freq_linear = haptics_apply_command(low_cmd.fm_action, low_cmd.fm_offset,
                                                               decoded->linear.lo_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.lo_amp_linear = haptics_apply_command(low_cmd.am_action, low_cmd.am_offset,
                                                              decoded->linear.lo_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[2]));
    }
}

void DecodeThree7Bit(const SwitchHapticPacket_s *encoded, hoja_rumble_msg_s *decoded)
{
    decoded->count = 3;
    Switch5BitCommand_s hi_cmd = {0};
    Switch5BitCommand_s low_cmd = {0};

    // Decode sample 0
    {
        if (encoded->three7bit.high_select)
        {
            if (encoded->three7bit.freq_select)
            {
                decoded->linear.hi_freq_linear = RumbleFreqLookup[encoded->three7bit.xx_7bit_xx];
            }
            else
            {
                decoded->linear.hi_amp_linear = RumbleAmpLookup[encoded->three7bit.xx_7bit_xx];
            }
        }
        else
        {
            if (encoded->three7bit.freq_select)
            {
                decoded->linear.lo_freq_linear = RumbleFreqLookup[encoded->three7bit.xx_7bit_xx];
            }
            else
            {
                decoded->linear.lo_amp_linear = RumbleAmpLookup[encoded->three7bit.xx_7bit_xx];
            }
        }

        haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[0]));
    }

    // Decode sample 1
    {
        hi_cmd = CommandTable[encoded->three7bit.amfm_5bit_hi_1];
        decoded->linear.hi_freq_linear = haptics_apply_command(hi_cmd.fm_action, hi_cmd.fm_offset,
                                                               decoded->linear.hi_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.hi_amp_linear = haptics_apply_command(hi_cmd.am_action, hi_cmd.am_offset,
                                                              decoded->linear.hi_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        low_cmd = CommandTable[encoded->three7bit.amfm_5bit_lo_1];
        decoded->linear.lo_freq_linear = haptics_apply_command(low_cmd.fm_action, low_cmd.fm_offset,
                                                               decoded->linear.lo_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.lo_amp_linear = haptics_apply_command(low_cmd.am_action, low_cmd.am_offset,
                                                              decoded->linear.lo_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[1]));
    }

    // Decode sample 2
    {
        hi_cmd = CommandTable[encoded->three7bit.amfm_5bit_hi_2];
        decoded->linear.hi_freq_linear = haptics_apply_command(hi_cmd.fm_action, hi_cmd.fm_offset,
                                                               decoded->linear.hi_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.hi_amp_linear = haptics_apply_command(hi_cmd.am_action, hi_cmd.am_offset,
                                                              decoded->linear.hi_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        low_cmd = CommandTable[encoded->three7bit.amfm_5bit_lo_2];
        decoded->linear.lo_freq_linear = haptics_apply_command(low_cmd.fm_action, low_cmd.fm_offset,
                                                               decoded->linear.lo_freq_linear, DefaultFrequency, MinFrequency, MaxFrequency);
        decoded->linear.lo_amp_linear = haptics_apply_command(low_cmd.am_action, low_cmd.am_offset,
                                                              decoded->linear.lo_amp_linear, DefaultAmplitude, MinAmplitude, MaxAmplitude);

        haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[2]));
    }
}

// This will detect and call the appropriate decoding schema
void haptics_decode_samples(const SwitchHapticPacket_s *encoded,
                            hoja_rumble_msg_s *decoded)
{
    switch (encoded->packet_type)
    {
    case 0:
        decoded->count = 0;
        haptics_linear_to_normal(&(decoded->linear), &(decoded->frames[0]));
        break;
    case 1:
        if ((encoded->data & 0xFFFFF) == 0)
        {
            DecodeOne5Bit(encoded, decoded);
        }
        else if ((encoded->data & 0x3) == 0)
        {
            DecodeOne7Bit(encoded, decoded);
        }
        else if ((encoded->data & 0x2) == 2)
        {
            DecodeThree7Bit(encoded, decoded);
        }
        break;
    case 2:
        if ((encoded->data & 0x3FF) == 0)
        {
            DecodeTwo5Bit(encoded, decoded);
        }
        else
        {
            DecodeTwo7Bit(encoded, decoded);
        }
        break;
    case 3:
        break;
        DecodeThree5Bit(encoded, decoded);
        break;
    };
}

// Translate and handle rumble
// Big thanks to hexkyz for some info on Discord
void haptics_rumble_translate(const uint8_t *data)
{
    if(!_haptics_init)
    {
        hatptics_initialize_lookup_tables();
        _haptics_init = true;
    }

    static hoja_rumble_msg_s internal_left = {0};
    static hoja_rumble_msg_s internal_right = {0};

    // Decode left
    haptics_decode_samples((const SwitchHapticPacket_s *) data, &internal_left);
    // Decode right
    haptics_decode_samples((const SwitchHapticPacket_s *) &(data[4]), &internal_right);

    if(internal_left.count>0)
    {
        uint8_t idx = internal_left.count-1;
        cb_hoja_rumble_set(&(internal_left.frames[0]), &(internal_left.frames[0]));
    }
    
    // Forward the data to the HOJA core
    //hoja_rumble_set(&internal_left, &internal_right);
}

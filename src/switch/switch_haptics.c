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
        if (f >= EXP_BASE2_RANGE_START)
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
        return CLAMP(current + offset, min, max);
    default:
        return default_val;
    }
}

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

void haptics_linear_set_default(SwitchLinearVibrationState_s *state)
{
    state->hi_amp_linear = DefaultAmplitude;
    state->lo_amp_linear = DefaultAmplitude;
    state->hi_freq_linear = DefaultFrequency;
    state->lo_freq_linear = DefaultFrequency;
}

// Functionally does what GetOutputValue does in the original documentation
void haptics_linear_to_normal(SwitchLinearVibrationState_s *linear, SwitchDecodedVibrationValues_s *decoded)
{
    decoded->high_band_freq = ExpBase2Lookup[haptics_get_lookup_index(linear->hi_freq_linear)] * CenterFreqHigh;
    decoded->low_band_freq = ExpBase2Lookup[haptics_get_lookup_index(linear->lo_freq_linear)] * CenterFreqLow;
    decoded->high_band_amp = ExpBase2Lookup[haptics_get_lookup_index(linear->hi_amp_linear)];
    decoded->low_band_amp = ExpBase2Lookup[haptics_get_lookup_index(linear->lo_amp_linear)];
}

// Different decoding algorithms
void DecodeOne5Bit(const SwitchEncodedVibrationSamples_s *encoded, SwitchDecodedVibrationSamples_s *decoded)
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

    haptics_linear_to_normal(&(decoded->linear), &(decoded->samples[0]));

    // Set samples as unread so they are actionable
    decoded->samples[0].unread = true;
}

void DecodeOne7Bit(const SwitchEncodedVibrationSamples_s *encoded, SwitchDecodedVibrationSamples_s *decoded)
{
    decoded->count = 1;

    decoded->linear.hi_freq_linear = RumbleFreqLookup[encoded->one7bit.fm_7bit_hi];
    decoded->linear.lo_freq_linear = RumbleFreqLookup[encoded->one7bit.fm_7bit_lo];
    decoded->linear.hi_amp_linear = RumbleAmpLookup[encoded->one7bit.am_7bit_hi];
    decoded->linear.lo_amp_linear = RumbleAmpLookup[encoded->one7bit.am_7bit_lo];

    haptics_linear_to_normal(&(decoded->linear), &(decoded->samples[0]));

    // Set samples as unread so they are actionable
    decoded->samples[0].unread = true;
}

void DecodeTwo5Bit(const SwitchEncodedVibrationSamples_s *encoded, SwitchDecodedVibrationSamples_s *decoded)
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

        haptics_linear_to_normal(&(decoded->linear), &(decoded->samples[0]));
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

        haptics_linear_to_normal(&(decoded->linear), &(decoded->samples[1]));
    }

    // Set samples as unread so they are actionable
    decoded->samples[0].unread = true;
    decoded->samples[1].unread = true;
}

void DecodeTwo7Bit(const SwitchEncodedVibrationSamples_s *encoded, SwitchDecodedVibrationSamples_s *decoded)
{
    decoded->count = 2;
    Switch5BitCommand_s hi_cmd = {0};
    Switch5BitCommand_s low_cmd = {0};

    // Decode sample 0
    {
        if (encoded->two7bit.high_select)
        {
            decoded->linear.hi_freq_linear = RumbleFreqLookup[encoded->two7bit.fm_7bit_xx];
            decoded->linear.hi_amp_linear = RumbleAmpLookup[encoded->two7bit.am_7bit_xx];

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
        haptics_linear_to_normal(&(decoded->linear), &(decoded->samples[0]));
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

        haptics_linear_to_normal(&(decoded->linear), &(decoded->samples[1]));
    }

    // Set samples as unread so they are actionable
    decoded->samples[0].unread = true;
    decoded->samples[1].unread = true;
}

void DecodeThree5Bit(const SwitchEncodedVibrationSamples_s *encoded, SwitchDecodedVibrationSamples_s *decoded)
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

        haptics_linear_to_normal(&(decoded->linear), &(decoded->samples[0]));
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

        haptics_linear_to_normal(&(decoded->linear), &(decoded->samples[1]));
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

        haptics_linear_to_normal(&(decoded->linear), &(decoded->samples[2]));
    }

    // Set samples as unread so they are actionable
    decoded->samples[0].unread = true;
    decoded->samples[1].unread = true;
    decoded->samples[2].unread = true;
}

void DecodeThree7Bit(const SwitchEncodedVibrationSamples_s *encoded, SwitchDecodedVibrationSamples_s *decoded)
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

        haptics_linear_to_normal(&(decoded->linear), &(decoded->samples[0]));
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

        haptics_linear_to_normal(&(decoded->linear), &(decoded->samples[1]));
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

        haptics_linear_to_normal(&(decoded->linear), &(decoded->samples[2]));
    }

    // Set samples as unread so they are actionable
    decoded->samples[0].unread = true;
    decoded->samples[1].unread = true;
    decoded->samples[2].unread = true;
}

// This will detect and call the appropriate decoding schema
void haptics_decode_samples(const SwitchEncodedVibrationSamples_s *encoded,
                            SwitchDecodedVibrationSamples_s *decoded)
{
    switch (encoded->packet_type)
    {
    case 0:
        decoded->count = 0;
        haptics_decode_samples(encoded, decoded);
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
        DecodeThree5Bit(encoded, decoded);
        break;
    };
}

//void haptics_decode_all(SwitchEncodedLeftRight_s *encoded, SwitchDecodedLeftRight_s *decoded)
//{
//    haptics_decode_samples(encoded->left_samples, decoded->left_samples);
//    haptics_decode_samples(encoded->right_samples, decoded->right_samples);
//}

// Translate and handle rumble
// Big thanks to hexkyz for some info on Discord
void haptics_rumble_translate(const uint8_t *data)
{

    static rumble_data_s output_data = {0};

    hoja_rumble_set(output_data.frequency_high, output_data.amplitude_high, output_data.frequency_low, output_data.amplitude_low);
}

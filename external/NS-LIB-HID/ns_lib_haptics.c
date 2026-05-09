/**
 * @file ns_lib_haptics.c
 * @brief HD rumble: decode 4-byte wire words to amplitude/frequency indices; optional float reference tables.
 *
 * Copyright (c) 2026 Mitchell Cairns, Hand Held Legend, LLC.
 *
 * Licensed under the Creative Commons Attribution-NonCommercial 4.0 International License
 * (CC BY-NC 4.0). Non-commercial use with attribution; commercial licensing from Hand Held Legend.
 * Full terms: https://creativecommons.org/licenses/by-nc/4.0/legalcode — see LICENSING.md in this folder
 *
 * SPDX-License-Identifier: CC-BY-NC-4.0
 *
 * TRADEMARK AND AFFILIATION DISCLAIMER:
 * This library is not affiliated, associated, authorized, endorsed by, or in any way officially
 * connected with Nintendo Co., Ltd., or any of its subsidiaries or its affiliates. Nintendo and
 * related marks are trademarks of their respective owners.
 */

#include "ns_lib_haptics.h"

#include <math.h>
#include <string.h>

void ns_set_haptic_indices_cb(const ns_lib_haptic_raw_sample_s *pairs, uint8_t pair_count);

typedef struct
{
    int16_t default_amplitude;
    int16_t min_amplitude;
    int16_t max_amplitude;
    int16_t starting_amplitude;
    uint8_t default_frequency;
    uint8_t min_frequency;
    uint8_t max_frequency;
} ns_haptic_defaults_s;

static ns_haptic_defaults_s _ns_haptic_defaults = {
    .default_amplitude  = 0,
    .min_amplitude      = 0,
    .max_amplitude      = 255,
    .starting_amplitude = 2,
    .default_frequency  = 64,
    .min_frequency      = 0,
    .max_frequency      = 127
};

#define AMPLITUDE_RANGE_START    -8.0f
#define AMPLITUDE_INTERVAL       0.03125f
#define STARTING_AMPLITUDE_FLOAT -7.9375f

void ns_lib_haptics_build_raw_tables(ns_lib_haptics_tables_s *out)
{
    if (out == NULL)
    {
        return;
    }

    for (uint16_t i = 0; i < NS_LIB_HAPTICS_AMP_LUT_LEN; i++)
    {
        float f = AMPLITUDE_RANGE_START + (float)i * AMPLITUDE_INTERVAL;
        out->amplitude_linear[i] = (f >= STARTING_AMPLITUDE_FLOAT) ? exp2f(f) : 0.f;
    }

    for (unsigned i = 0; i < NS_LIB_HAPTICS_FREQ_LUT_LEN; i++)
    {
        double linear = 0.03125 * (double)i - 2.0;
        out->frequency_hz_lo[i] = (float)(160.0 * exp2(linear));
        out->frequency_hz_hi[i] = (float)(320.0 * exp2(linear));
    }
}

float ns_lib_haptics_freq_index_to_hz_hi(uint8_t freq_idx)
{
    unsigned i = freq_idx;
    if (i >= NS_LIB_HAPTICS_FREQ_LUT_LEN)
    {
        i = NS_LIB_HAPTICS_FREQ_LUT_LEN - 1u;
    }
    double linear = 0.03125 * (double)i - 2.0;
    return (float)(320.0 * exp2(linear));
}

float ns_lib_haptics_freq_index_to_hz_lo(uint8_t freq_idx)
{
    unsigned i = freq_idx;
    if (i >= NS_LIB_HAPTICS_FREQ_LUT_LEN)
    {
        i = NS_LIB_HAPTICS_FREQ_LUT_LEN - 1u;
    }
    double linear = 0.03125 * (double)i - 2.0;
    return (float)(160.0 * exp2(linear));
}

uint8_t ns_lib_haptics_host_amp_index_to_exp_lut_index(uint8_t host_amp_idx)
{
    unsigned i = host_amp_idx & 127u;
    float tmp;

    if (i == 0)
    {
        tmp = -8.0f;
    }
    else if (i < 16)
    {
        tmp = 0.25f * (float)i - 7.75f;
    }
    else if (i < 32)
    {
        tmp = 0.0625f * (float)i - 4.9375f;
    }
    else
    {
        tmp = 0.03125f * (float)i - 3.96875f;
    }

    float fidx = tmp / AMPLITUDE_INTERVAL;
    int idx = 255 + (int)fidx;

    if (i == 0)
    {
        idx = 0;
    }
    if (idx > 255)
    {
        idx = 255;
    }
    if (idx < 0)
    {
        idx = 0;
    }
    return (uint8_t)idx;
}

float ns_lib_haptics_exp_lut_index_to_amplitude_linear(uint16_t exp_lut_idx)
{
    if (exp_lut_idx >= NS_LIB_HAPTICS_AMP_LUT_LEN)
    {
        return 0.f;
    }
    float f = AMPLITUDE_RANGE_START + (float)exp_lut_idx * AMPLITUDE_INTERVAL;
    if (f < STARTING_AMPLITUDE_FLOAT)
    {
        return 0.f;
    }
    return exp2f(f);
}

float ns_lib_haptics_host_amp_index_to_amplitude_linear(uint8_t host_amp_idx)
{
    uint8_t row = ns_lib_haptics_host_amp_index_to_exp_lut_index(host_amp_idx);
    return ns_lib_haptics_exp_lut_index_to_amplitude_linear(row);
}

uint16_t ns_lib_haptics_hz_to_phase_increment_fp(float frequency_hz)
{
    float increment =
        (frequency_hz * (float)NS_LIB_HAPTICS_SINE_LUT_SIZE) / (float)NS_LIB_HAPTICS_PCM_RATE_HZ;
    return (uint16_t)(increment * (float)NS_LIB_HAPTICS_FREQ_FP_SCALE + 0.5f);
}

uint16_t ns_lib_haptics_amplitude_linear_to_q15(float linear)
{
    const float scale = (float)(1u << NS_LIB_HAPTICS_AMP_FP_SHIFT);
    uint16_t tmp = (uint16_t)(linear * scale);
    if (linear > 0.0f && !tmp)
    {
        tmp = 1;
    }
    return tmp;
}

float ns_lib_haptics_frequency_increment_to_hz(uint16_t phase_increment_fp)
{
    if (phase_increment_fp == 0)
    {
        return 0.f;
    }
    float inc = (float)phase_increment_fp / (float)NS_LIB_HAPTICS_FREQ_FP_SCALE;
    return inc * (float)NS_LIB_HAPTICS_PCM_RATE_HZ / (float)NS_LIB_HAPTICS_SINE_LUT_SIZE;
}

float ns_lib_haptics_amplitude_fixed_to_gain(uint16_t amplitude_fixed)
{
    return (float)amplitude_fixed / (float)(1u << NS_LIB_HAPTICS_AMP_FP_SHIFT);
}

static ns_lib_haptics_tables_s _ns_tables;
static uint8_t _ns_amp_index[128];
static uint64_t _ns_haptic_packet_counter;

static void ns_lib_haptics_install_builtin_tables(void)
{
    ns_lib_haptics_build_raw_tables(&_ns_tables);
    for (unsigned i = 0; i < 128u; i++)
    {
        _ns_amp_index[i] = ns_lib_haptics_host_amp_index_to_exp_lut_index((uint8_t)i);
    }
}

static unsigned ns_haptics_clamp_amp_row(uint16_t idx)
{
    return (unsigned)idx >= NS_LIB_HAPTICS_AMP_LUT_LEN ? NS_LIB_HAPTICS_AMP_LUT_LEN - 1u : (unsigned)idx;
}

static unsigned ns_haptics_clamp_freq_row(uint8_t idx)
{
    return (unsigned)idx >= NS_LIB_HAPTICS_FREQ_LUT_LEN ? NS_LIB_HAPTICS_FREQ_LUT_LEN - 1u : (unsigned)idx;
}

void ns_lib_haptics_get_basic(uint8_t exp_lut_row, uint8_t freq_idx, float *out_amplitude_linear, float *out_freq_hz_lo)
{
    unsigned er = ns_haptics_clamp_amp_row(exp_lut_row);
    unsigned fr = ns_haptics_clamp_freq_row(freq_idx);
    if (out_amplitude_linear != NULL)
    {
        *out_amplitude_linear = _ns_tables.amplitude_linear[er];
    }
    if (out_freq_hz_lo != NULL)
    {
        *out_freq_hz_lo = _ns_tables.frequency_hz_lo[fr];
    }
}

void ns_lib_haptics_packet_pair_physical(const ns_haptic_packet_s *packet, unsigned pair_index, float *out_hi_hz,
                                         float *out_lo_hz, float *out_hi_amplitude, float *out_lo_amplitude)
{
    if (packet == NULL || pair_index >= 3u || pair_index >= (unsigned)packet->count)
    {
        return;
    }
    const ns_haptic_processed_s *p = &packet->pairs[pair_index];
    if (out_hi_hz != NULL)
    {
        *out_hi_hz = p->hi_frequency_hz;
    }
    if (out_lo_hz != NULL)
    {
        *out_lo_hz = p->lo_frequency_hz;
    }
    if (out_hi_amplitude != NULL)
    {
        *out_hi_amplitude = p->hi_amplitude;
    }
    if (out_lo_amplitude != NULL)
    {
        *out_lo_amplitude = p->lo_amplitude;
    }
}

const ns_lib_haptic_5bit_cmd_s ns_lib_haptic_cmd_table[] = {
    {.am_action = NS_LIB_HAPTIC_ACTION_DEFAULT, .fm_action = NS_LIB_HAPTIC_ACTION_DEFAULT, .am_offset = 0, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 0, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 240, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 224, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 208, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 192, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 176, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 160, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 144, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 128, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 112, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 96, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_IGNORE, .fm_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .am_offset = 0, .fm_offset = 5},
    {.am_action = NS_LIB_HAPTIC_ACTION_IGNORE, .fm_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .am_offset = 0, .fm_offset = 5},
    {.am_action = NS_LIB_HAPTIC_ACTION_IGNORE, .fm_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .am_offset = 0, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_IGNORE, .fm_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .am_offset = 0, .fm_offset = 7},
    {.am_action = NS_LIB_HAPTIC_ACTION_IGNORE, .fm_action = NS_LIB_HAPTIC_ACTION_SUBSTITUTE, .am_offset = 0, .fm_offset = 7},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUM, .fm_action = NS_LIB_HAPTIC_ACTION_SUM, .am_offset = 4, .fm_offset = 1},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUM, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 4, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUM, .fm_action = NS_LIB_HAPTIC_ACTION_SUM, .am_offset = 4, .fm_offset = -1},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUM, .fm_action = NS_LIB_HAPTIC_ACTION_SUM, .am_offset = 1, .fm_offset = 1},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUM, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 1, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUM, .fm_action = NS_LIB_HAPTIC_ACTION_SUM, .am_offset = 1, .fm_offset = -1},
    {.am_action = NS_LIB_HAPTIC_ACTION_IGNORE, .fm_action = NS_LIB_HAPTIC_ACTION_SUM, .am_offset = 0, .fm_offset = 1},
    {.am_action = NS_LIB_HAPTIC_ACTION_IGNORE, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = 0, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_IGNORE, .fm_action = NS_LIB_HAPTIC_ACTION_SUM, .am_offset = 0, .fm_offset = -1},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUM, .fm_action = NS_LIB_HAPTIC_ACTION_SUM, .am_offset = -1, .fm_offset = 1},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUM, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = -1, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUM, .fm_action = NS_LIB_HAPTIC_ACTION_SUM, .am_offset = -1, .fm_offset = -1},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUM, .fm_action = NS_LIB_HAPTIC_ACTION_SUM, .am_offset = -4, .fm_offset = 1},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUM, .fm_action = NS_LIB_HAPTIC_ACTION_IGNORE, .am_offset = -4, .fm_offset = 0},
    {.am_action = NS_LIB_HAPTIC_ACTION_SUM, .fm_action = NS_LIB_HAPTIC_ACTION_SUM, .am_offset = -4, .fm_offset = -1},
};

static inline uint8_t _apply_command_amp(ns_lib_haptic_action_t action, int16_t offset, uint8_t current)
{
    int32_t result;

    switch (action)
    {
        case NS_LIB_HAPTIC_ACTION_IGNORE:
            return current;

        case NS_LIB_HAPTIC_ACTION_SUBSTITUTE:
            return (uint8_t)offset;

        case NS_LIB_HAPTIC_ACTION_SUM:
            result = (int32_t)current + offset;
            result = NS_LIB_HAPTIC_CLAMP(result, _ns_haptic_defaults.min_amplitude, _ns_haptic_defaults.max_amplitude);
            return (uint8_t)result;

        default:
            return (uint8_t)_ns_haptic_defaults.default_amplitude;
    }
}

static inline uint8_t _apply_command_freq(ns_lib_haptic_action_t action, int16_t offset, uint8_t current)
{
    int32_t result;

    switch (action)
    {
        case NS_LIB_HAPTIC_ACTION_IGNORE:
            return current;

        case NS_LIB_HAPTIC_ACTION_SUBSTITUTE:
            return (uint8_t)offset;

        case NS_LIB_HAPTIC_ACTION_SUM:
            result = (int32_t)current + offset;
            result = NS_LIB_HAPTIC_CLAMP(result, _ns_haptic_defaults.min_frequency, _ns_haptic_defaults.max_frequency);
            return (uint8_t)result;

        default:
            return (uint8_t)_ns_haptic_defaults.default_frequency;
    }
}

static void _haptics_decode_type_1(const ns_lib_haptic_wire_u *encoded, ns_lib_haptic_raw_state_s *out)
{
    uint8_t samples = encoded->frame_count;
    out->sample_count = samples;

    ns_lib_haptic_5bit_cmd_s hi_cmd  = {0};
    ns_lib_haptic_5bit_cmd_s low_cmd = {0};

    if (samples > 0)
    {
        hi_cmd                      = ns_lib_haptic_cmd_table[encoded->type1.cmd_hi_0];
        out->state.hi_frequency_idx = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        low_cmd                     = ns_lib_haptic_cmd_table[encoded->type1.cmd_lo_0];
        out->state.lo_frequency_idx = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        memcpy(&out->samples[0], &out->state, sizeof(ns_lib_haptic_raw_sample_s));
    }

    if (samples > 1)
    {
        hi_cmd                      = ns_lib_haptic_cmd_table[encoded->type1.cmd_hi_1];
        out->state.hi_frequency_idx = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        low_cmd                     = ns_lib_haptic_cmd_table[encoded->type1.cmd_lo_1];
        out->state.lo_frequency_idx = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        memcpy(&out->samples[1], &out->state, sizeof(ns_lib_haptic_raw_sample_s));
    }

    if (samples > 2)
    {
        hi_cmd                      = ns_lib_haptic_cmd_table[encoded->type1.cmd_hi_2];
        out->state.hi_frequency_idx = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        low_cmd                     = ns_lib_haptic_cmd_table[encoded->type1.cmd_lo_2];
        out->state.lo_frequency_idx = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        memcpy(&out->samples[2], &out->state, sizeof(ns_lib_haptic_raw_sample_s));
    }
}

static void _haptics_decode_type_2(const ns_lib_haptic_wire_u *encoded, ns_lib_haptic_raw_state_s *out)
{
    uint8_t samples = encoded->frame_count;
    out->sample_count = samples;

    out->state.hi_frequency_idx = (uint8_t)encoded->type2.freq_hi;
    out->state.lo_frequency_idx = (uint8_t)encoded->type2.freq_lo;
    out->state.hi_amplitude_idx = _ns_amp_index[encoded->type2.amp_hi & 127u];
    out->state.lo_amplitude_idx = _ns_amp_index[encoded->type2.amp_lo & 127u];

    memcpy(&out->samples[0], &out->state, sizeof(ns_lib_haptic_raw_sample_s));
}

static void _haptics_decode_type_3(const ns_lib_haptic_wire_u *encoded, ns_lib_haptic_raw_state_s *out)
{
    ns_lib_haptic_5bit_cmd_s hi_cmd = {0};
    ns_lib_haptic_5bit_cmd_s low_cmd = {0};

    uint8_t samples = encoded->frame_count;
    out->sample_count = samples;

    if (samples > 0)
    {
        if (encoded->type3.high_select)
        {
            out->state.hi_frequency_idx = (uint8_t)encoded->type3.freq_xx_0;
            out->state.hi_amplitude_idx = _ns_amp_index[encoded->type3.amp_xx_0 & 127u];

            low_cmd                     = ns_lib_haptic_cmd_table[encoded->type3.cmd_xx_0];
            out->state.lo_frequency_idx = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
            out->state.lo_amplitude_idx = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);
        }
        else
        {
            out->state.lo_frequency_idx = (uint8_t)encoded->type3.freq_xx_0;
            out->state.lo_amplitude_idx = _ns_amp_index[encoded->type3.amp_xx_0 & 127u];

            hi_cmd                      = ns_lib_haptic_cmd_table[encoded->type3.cmd_xx_0];
            out->state.hi_frequency_idx = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
            out->state.hi_amplitude_idx = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);
        }

        memcpy(&out->samples[0], &out->state, sizeof(ns_lib_haptic_raw_sample_s));
    }

    if (samples > 1)
    {
        hi_cmd                      = ns_lib_haptic_cmd_table[encoded->type3.cmd_hi_1];
        out->state.hi_frequency_idx = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        low_cmd                     = ns_lib_haptic_cmd_table[encoded->type3.cmd_lo_1];
        out->state.lo_frequency_idx = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        memcpy(&out->samples[1], &out->state, sizeof(ns_lib_haptic_raw_sample_s));
    }
}

static void _haptics_decode_type_4(const ns_lib_haptic_wire_u *encoded, ns_lib_haptic_raw_state_s *out)
{
    ns_lib_haptic_5bit_cmd_s hi_cmd = {0};
    ns_lib_haptic_5bit_cmd_s low_cmd = {0};

    uint8_t samples = encoded->frame_count;
    out->sample_count = samples;

    if (samples > 0)
    {
        if (encoded->type4.high_select)
        {
            if (encoded->type4.freq_select)
            {
                out->state.hi_frequency_idx = (uint8_t)encoded->type4.xx_xx_0;
            }
            else
            {
                out->state.hi_amplitude_idx = _ns_amp_index[encoded->type4.xx_xx_0 & 127u];
            }
        }
        else
        {
            if (encoded->type4.freq_select)
            {
                out->state.lo_frequency_idx = (uint8_t)encoded->type4.xx_xx_0;
            }
            else
            {
                out->state.lo_amplitude_idx = _ns_amp_index[encoded->type4.xx_xx_0 & 127u];
            }
        }

        memcpy(&out->samples[0], &out->state, sizeof(ns_lib_haptic_raw_sample_s));
    }

    if (samples > 1)
    {
        hi_cmd                      = ns_lib_haptic_cmd_table[encoded->type4.cmd_hi_1];
        out->state.hi_frequency_idx = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        low_cmd                     = ns_lib_haptic_cmd_table[encoded->type4.cmd_lo_1];
        out->state.lo_frequency_idx = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        memcpy(&out->samples[1], &out->state, sizeof(ns_lib_haptic_raw_sample_s));
    }

    if (samples > 2)
    {
        hi_cmd                      = ns_lib_haptic_cmd_table[encoded->type4.cmd_hi_2];
        out->state.hi_frequency_idx = _apply_command_freq(hi_cmd.fm_action, hi_cmd.fm_offset, out->state.hi_frequency_idx);
        out->state.hi_amplitude_idx = _apply_command_amp(hi_cmd.am_action, hi_cmd.am_offset, out->state.hi_amplitude_idx);

        low_cmd                     = ns_lib_haptic_cmd_table[encoded->type4.cmd_lo_2];
        out->state.lo_frequency_idx = _apply_command_freq(low_cmd.fm_action, low_cmd.fm_offset, out->state.lo_frequency_idx);
        out->state.lo_amplitude_idx = _apply_command_amp(low_cmd.am_action, low_cmd.am_offset, out->state.lo_amplitude_idx);

        memcpy(&out->samples[2], &out->state, sizeof(ns_lib_haptic_raw_sample_s));
    }
}

static ns_lib_haptic_raw_state_s _ns_raw_state;

static void _haptics_decode_samples_apply(const ns_lib_haptic_wire_u *encoded, ns_lib_haptic_raw_state_s *out)
{
    switch (encoded->frame_count)
    {
        case 0:
            out->state.hi_amplitude_idx = 0;
            out->sample_count = 0;
            break;
        case 1:
            if ((encoded->data & 0xFFFFF) == 0)
            {
                _haptics_decode_type_1(encoded, out);
            }
            else if ((encoded->data & 0x3) == 0)
            {
                _haptics_decode_type_2(encoded, out);
            }
            else if ((encoded->data & 0x2) == 2)
            {
                _haptics_decode_type_4(encoded, out);
            }
            break;
        case 2:
            if ((encoded->data & 0x3FF) == 0)
            {
                _haptics_decode_type_1(encoded, out);
            }
            else
            {
                _haptics_decode_type_3(encoded, out);
            }
            break;
        case 3:
            _haptics_decode_type_1(encoded, out);
            break;
        default:
            break;
    }
}

void ns_lib_haptics_init(void)
{
    ns_lib_haptics_install_builtin_tables();

    _ns_raw_state.state.hi_amplitude_idx = (uint8_t)_ns_haptic_defaults.starting_amplitude;
    _ns_raw_state.state.lo_amplitude_idx = (uint8_t)_ns_haptic_defaults.starting_amplitude;
    _ns_raw_state.state.hi_frequency_idx = _ns_haptic_defaults.default_frequency;
    _ns_raw_state.state.lo_frequency_idx = _ns_haptic_defaults.default_frequency;

    for (int i = 0; i < 3; i++)
    {
        _ns_raw_state.samples[i].hi_amplitude_idx = (uint8_t)_ns_haptic_defaults.starting_amplitude;
        _ns_raw_state.samples[i].lo_amplitude_idx = (uint8_t)_ns_haptic_defaults.starting_amplitude;
        _ns_raw_state.samples[i].hi_frequency_idx = _ns_haptic_defaults.default_frequency;
        _ns_raw_state.samples[i].lo_frequency_idx = _ns_haptic_defaults.default_frequency;
    }
}

void ns_lib_haptics_arbitrary_playback(uint8_t intensity)
{
    uint8_t scale = intensity >> 3;

    uint8_t translated_intensity = scale;
    uint8_t translated_intensity_lo = scale + 2;

    if (translated_intensity_lo > 127)
    {
        translated_intensity_lo = 127;
    }

    uint8_t target_freq = 64;

    if (translated_intensity == 36)
    {
        /* center */
    }
    else if (translated_intensity < 36)
    {
        uint8_t remainder = (uint8_t)(36 - translated_intensity);
        uint8_t freq_offset = remainder > 36 ? 36 : remainder;
        target_freq = (uint8_t)(target_freq - freq_offset);
    }
    else
    {
        uint8_t remainder = (uint8_t)(translated_intensity - 36);
        uint8_t freq_offset = remainder > 36 ? 36 : remainder;
        target_freq = (uint8_t)(target_freq + freq_offset);
    }

    uint8_t dbg_ahi = _ns_amp_index[translated_intensity];
    uint8_t dbg_alo = _ns_amp_index[translated_intensity_lo];
    uint8_t dbg_hi = target_freq;
    uint8_t dbg_lo = target_freq;

    ns_haptic_packet_s packet = {0};
    packet.count = 1;
    packet.counter = ++_ns_haptic_packet_counter;

    if (!intensity)
    {
        packet.pairs[0].hi_amplitude = 0.f;
        packet.pairs[0].lo_amplitude = 0.f;
        packet.pairs[0].hi_frequency_hz = 0.f;
        packet.pairs[0].lo_frequency_hz = 0.f;
        ns_lib_haptics_dispatch_hd(&packet);
        return;
    }

    {
        unsigned ahi = ns_haptics_clamp_amp_row(dbg_ahi);
        unsigned alo = ns_haptics_clamp_amp_row(dbg_alo);
        unsigned fhi = ns_haptics_clamp_freq_row(dbg_hi);
        unsigned flo = ns_haptics_clamp_freq_row(dbg_lo);
        packet.pairs[0].hi_amplitude = _ns_tables.amplitude_linear[ahi];
        packet.pairs[0].lo_amplitude = _ns_tables.amplitude_linear[alo];
        packet.pairs[0].hi_frequency_hz = _ns_tables.frequency_hz_hi[fhi];
        packet.pairs[0].lo_frequency_hz = _ns_tables.frequency_hz_lo[flo];
    }

    ns_lib_haptics_dispatch_hd(&packet);
}

void ns_lib_haptics_rumble_translate(const uint8_t *data)
{
    /*
     * Main integration path:
     *   1) receive raw 4-byte rumble word from host
     *   2) decode into _ns_raw_state.samples[0..2] (INDEXES ONLY)
     *   3) call ns_set_haptic_indices_cb(samples, sample_count)
     *
     * sample_count is 0..3 and each sample carries:
     *   hi_amplitude_idx, lo_amplitude_idx, hi_frequency_idx, lo_frequency_idx
     *
     * These are table indices (not physical amplitudes/frequencies). The firmware
     * using this library maps those indices to whatever representation it prefers.
     */
    if (ns_lib_haptics_webusb_blocks_rumble())
    {
        return;
    }

    const ns_lib_haptic_wire_u *encoded = (const ns_lib_haptic_wire_u *)data;
    static uint32_t last_wire_data = 0;
    if (encoded->data != last_wire_data)
    {
        last_wire_data = encoded->data;
        _haptics_decode_samples_apply(encoded, &_ns_raw_state);
    }

    uint8_t n = _ns_raw_state.sample_count;
    if (n > 3u)
    {
        n = 3u;
    }
    ns_set_haptic_indices_cb(_ns_raw_state.samples, n);
}

__attribute__((weak)) void ns_lib_haptics_dispatch_hd(ns_haptic_packet_s *packet)
{
    (void)packet;
}

__attribute__((weak)) int ns_lib_haptics_webusb_blocks_rumble(void)
{
    return 0;
}

/**
 * @file ns_lib_haptics.h
 * @brief HD rumble decode + float reference lookup tables (indices → Hz / amplitude).
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

#ifndef NS_LIB_HAPTICS_H
#define NS_LIB_HAPTICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NS_LIB_HAPTIC_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

typedef enum
{
    NS_LIB_HAPTIC_ACTION_IGNORE = 0x0,
    NS_LIB_HAPTIC_ACTION_DEFAULT = 0x1,
    NS_LIB_HAPTIC_ACTION_SUBSTITUTE = 0x2,
    NS_LIB_HAPTIC_ACTION_SUM = 0x3,
} ns_lib_haptic_action_t;

typedef struct
{
    ns_lib_haptic_action_t am_action : 8;
    ns_lib_haptic_action_t fm_action : 8;
    int16_t am_offset;
    int16_t fm_offset;
} ns_lib_haptic_5bit_cmd_s;

typedef union
{
    struct
    {
        uint32_t data : 30;
        uint32_t frame_count : 2;
    };

    struct
    {
        uint32_t cmd_hi_2 : 5;
        uint32_t cmd_lo_2 : 5;
        uint32_t cmd_hi_1 : 5;
        uint32_t cmd_lo_1 : 5;
        uint32_t cmd_hi_0 : 5;
        uint32_t cmd_lo_0 : 5;
        uint32_t frame_count : 2;
    } type1;

    struct
    {
        uint32_t padding : 2;
        uint32_t freq_hi : 7;
        uint32_t amp_hi : 7;
        uint32_t freq_lo : 7;
        uint32_t amp_lo : 7;
        uint32_t frame_count : 2;
    } type2;

    struct
    {
        uint32_t high_select : 1;
        uint32_t freq_xx_0 : 7;
        uint32_t cmd_hi_1 : 5;
        uint32_t cmd_lo_1 : 5;
        uint32_t cmd_xx_0 : 5;
        uint32_t amp_xx_0 : 7;
        uint32_t frame_count : 2;
    } type3;

    struct
    {
        uint32_t high_select : 1;
        uint32_t blank : 1;
        uint32_t freq_select : 1;
        uint32_t cmd_hi_2 : 5;
        uint32_t cmd_lo_2 : 5;
        uint32_t cmd_hi_1 : 5;
        uint32_t cmd_lo_1 : 5;
        uint32_t xx_xx_0 : 7;
        uint32_t frame_count : 2;
    } type4;
} __attribute__((packed)) ns_lib_haptic_wire_u;

/** One decoded time slice: indices into ns_lib_haptics_tables_s (amplitude rows / frequency rows). */
typedef struct
{
    uint8_t hi_amplitude_idx;
    uint8_t lo_amplitude_idx;
    uint8_t hi_frequency_idx;
    uint8_t lo_frequency_idx;
} ns_lib_haptic_raw_sample_s;

typedef struct
{
    uint8_t sample_count;
    ns_lib_haptic_raw_sample_s state;
    ns_lib_haptic_raw_sample_s samples[3];
} ns_lib_haptic_raw_state_s;

/** Floats derived from the reference tables (PCM / fixed-point conversion belongs in your driver). */
typedef struct
{
    float hi_amplitude;
    float lo_amplitude;
    float hi_frequency_hz;
    float lo_frequency_hz;
} ns_haptic_processed_s;

typedef struct
{
    ns_haptic_processed_s pairs[3];
    uint8_t count;
    uint64_t counter;
} ns_haptic_packet_s;

/** Rows in the exp₂ amplitude envelope LUT. */
#define NS_LIB_HAPTICS_AMP_LUT_LEN 256u

/** Entries per rumble frequency axis (high / low band share index range 0–127). */
#define NS_LIB_HAPTICS_FREQ_LUT_LEN 128u

/** Reference PCM stepping (optional helpers for your driver). */
#define NS_LIB_HAPTICS_PCM_RATE_HZ 8000u

#define NS_LIB_HAPTICS_SINE_LUT_SIZE 4096u

#define NS_LIB_HAPTICS_FREQ_FP_SCALE 128u

#define NS_LIB_HAPTICS_AMP_FP_SHIFT 15u

/**
 * @brief Reference lookup tables (floats). Use ns_lib_haptics_build_raw_tables() to fill.
 *
 * PCM phase increments and Q-format amplitudes are not stored here — generate those in your PCM driver
 * from frequency_hz_* and amplitude_linear if needed.
 */
typedef struct ns_lib_haptics_tables_s
{
    /** Unitless exp₂ envelope sample per row (0 below library cutoff). Indexed by decoded amplitude index. */
    float amplitude_linear[NS_LIB_HAPTICS_AMP_LUT_LEN];
    /** Synthesized sine frequency (Hz) for the high haptic band. */
    float frequency_hz_hi[NS_LIB_HAPTICS_FREQ_LUT_LEN];
    /** Synthesized sine frequency (Hz) for the low haptic band. */
    float frequency_hz_lo[NS_LIB_HAPTICS_FREQ_LUT_LEN];
} ns_lib_haptics_tables_s;

/** Populate @p out with the same reference curves ns_lib_haptics_init() uses internally. */
void ns_lib_haptics_build_raw_tables(ns_lib_haptics_tables_s *out);

/** Frequency LUT index (0–127) → Hz (high band curve). */
float ns_lib_haptics_freq_index_to_hz_hi(uint8_t freq_idx);

/** Frequency LUT index (0–127) → Hz (low band curve). */
float ns_lib_haptics_freq_index_to_hz_lo(uint8_t freq_idx);

/** Exp-LUT row (0–255) → amplitude_linear value (same as amplitude_linear[row] after build_tables). */
float ns_lib_haptics_exp_lut_index_to_amplitude_linear(uint16_t exp_lut_idx);

/** Host amplitude index (0–127) → row into amplitude_linear[]. */
uint8_t ns_lib_haptics_host_amp_index_to_exp_lut_index(uint8_t host_amp_idx);

/** Host amplitude index → composed amplitude_linear (via exp row). */
float ns_lib_haptics_host_amp_index_to_amplitude_linear(uint8_t host_amp_idx);

/**
 * @brief Optional: convert Hz to phase increment for an 8 kHz, 4096-sample sine LUT with scale 128.
 *
 * For PCM drivers that keep the same stepping convention as the companion `ns_hid_lib` tree; not used inside the decoder.
 */
uint16_t ns_lib_haptics_hz_to_phase_increment_fp(float frequency_hz);

/**
 * @brief Optional: convert linear amplitude (e.g. exp₂ envelope sample) to Q1.15 for PCM.
 *
 * Not used inside the decoder.
 */
uint16_t ns_lib_haptics_amplitude_linear_to_q15(float linear);

/**
 * @brief Inverse of ns_lib_haptics_hz_to_phase_increment_fp (0 increment → 0 Hz).
 */
float ns_lib_haptics_frequency_increment_to_hz(uint16_t phase_increment_fp);

/**
 * @brief Q1.15 word → linear gain (÷ 32768).
 */
float ns_lib_haptics_amplitude_fixed_to_gain(uint16_t amplitude_fixed);

/**
 * @brief Copy floats from one decoded pair (after ns_lib_haptics_dispatch_hd packet build).
 */
void ns_lib_haptics_packet_pair_physical(const ns_haptic_packet_s *packet, unsigned pair_index,
                                         float *out_hi_hz, float *out_lo_hz, float *out_hi_amplitude,
                                         float *out_lo_amplitude);

void ns_lib_haptics_init(void);

/**
 * @brief Look up reference floats: @p exp_lut_row into amplitude_linear[], @p freq_idx into low-band Hz.
 */
void ns_lib_haptics_get_basic(uint8_t exp_lut_row, uint8_t freq_idx, float *out_amplitude_linear,
                              float *out_freq_hz_lo);

void ns_lib_haptics_arbitrary_playback(uint8_t intensity);

void ns_lib_haptics_rumble_translate(const uint8_t *data);

void ns_lib_haptics_dispatch_hd(ns_haptic_packet_s *packet);

int ns_lib_haptics_webusb_blocks_rumble(void);

#ifdef __cplusplus
}
#endif

#endif /* NS_LIB_HAPTICS_H */

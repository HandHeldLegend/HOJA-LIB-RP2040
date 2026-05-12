#ifndef CORES_SWITCH_H
#define CORES_SWITCH_H

#include <stdint.h>

#include "cores/cores.h"

typedef struct
{
    union
    {
        struct
        {
            // Y and C-Up (N64)
            uint8_t b_y       : 1;

            // X and C-Left (N64)
            uint8_t b_x       : 1;

            uint8_t b_b       : 1;
            uint8_t b_a       : 1;
            uint8_t t_r_sr    : 1;
            uint8_t t_r_sl    : 1;
            uint8_t t_r       : 1;

            // ZR and C-Down (N64)
            uint8_t t_zr      : 1;
        };
        uint8_t right_buttons;
    };
    union
    {
        struct
        {
            // Minus and C-Right (N64)
            uint8_t b_minus     : 1;

            // Plus and Start
            uint8_t b_plus      : 1;

            uint8_t sb_right    : 1;
            uint8_t sb_left     : 1;
            uint8_t b_home      : 1;
            uint8_t b_capture   : 1;
            uint8_t none        : 1;
            uint8_t charge_grip_active : 1;
        };
        uint8_t shared_buttons;
    };
    union
    {
        struct
        {
            uint8_t d_down    : 1;
            uint8_t d_up      : 1;
            uint8_t d_right   : 1;
            uint8_t d_left    : 1;
            uint8_t t_l_sr    : 1;
            uint8_t t_l_sl    : 1;
            uint8_t t_l       : 1;

            // ZL and Z (N64)
            uint8_t t_zl      : 1;

        };
        uint8_t left_buttons;
    };
} core_switch_report_s;

bool core_switch_init(core_params_s *params);

/** NS-LIB: feed decoded 4-byte HD rumble wire word (ESP32 / coprocessor path). */
void core_switch_ns_feed_hd_rumble_wire4(const uint8_t *data);

/** NS-LIB: forward raw host OUT bytes into the protocol engine (ESP32 full OUT path). */
void core_switch_ns_output_tunnel(const uint8_t *data, uint16_t len);

/** NS-LIB: mutable SPI analog calibration blob (18 bytes). */
uint8_t *core_switch_ns_analog_calibration_blob(void);
void core_switch_ns_analog_calibration_reset_defaults(void);

/** Optional hook: reserved for future NS quaternion fusion on IMU cadence. */
void core_switch_ns_motion_quat_step(void);

/** Legacy ERM-simulator / test path mapped onto NS-LIB fixed-point haptics tables. */
void core_switch_ns_arbitrary_playback(uint8_t intensity);

#endif
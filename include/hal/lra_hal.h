#ifndef HOJA_LRA_HAL_H
#define HOJA_LRA_HAL_H

#include "hoja_bsp.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "board_config.h"
#include "devices_shared_types.h"

#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER==HAPTICS_DRIVER_LRA_HAL)

// LRA haptics config. PWM slices + DMA channels are allocated dynamically at
// init, so only GPIO pins are supplied. channel_b is the optional second motor
// (set channel_b_enable=false for single-channel boards). The intensity_* values
// are 0..1 ratios; leave any at 0.0f to use the library default.
typedef struct
{
    uint8_t channel_a_pin;     // primary haptic PWM GPIO (required)
    uint8_t channel_b_pin;     // secondary haptic PWM GPIO (dual-channel)
    bool    channel_b_enable;  // drive a second channel
    bool    channel_swap;      // swap left/right routing
    float   intensity_max;     // amplitude ceiling (0.0f = library default)
    float   intensity_min_lo;  // low-frequency amplitude floor (0.0f = default)
    float   intensity_min_hi;  // high-frequency amplitude floor (0.0f = default)
} lra_hal_cfg_s;

#define HOJA_HAPTICS_SET_STD(intensity, brake) lra_hal_set_standard(intensity, brake)

#define HOJA_HAPTICS_PUSH_AMFM(input) lra_hal_push_amfm(input)

#define HOJA_HAPTICS_INIT(intensity) lra_hal_init(intensity)
#define HOJA_HAPTICS_TASK(timestamp) lra_hal_task(timestamp)
#define HOJA_HAPTICS_STOP() lra_hal_stop()

void lra_hal_stop();
bool lra_hal_init(uint8_t intensity);
void lra_hal_task(uint64_t timestamp);
void lra_hal_push_amfm(haptic_packet_s *packet);
void lra_hal_set_standard(uint8_t intensity, bool brake);
#endif

#endif
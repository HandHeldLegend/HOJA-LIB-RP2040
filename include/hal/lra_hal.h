#ifndef HOJA_LRA_HAL_H
#define HOJA_LRA_HAL_H

#include "hoja_bsp.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "board_config.h"
#include "devices_shared_types.h"

#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER==HAPTICS_DRIVER_LRA_HAL)

// Optional DRV2605L front-end (declare HOJA_LRA_HAL_ENABLE_DRV2605L in
// board_config.h). Configures the external LRA driver IC over I2C; the RP2040
// still generates PCM via PWM through lra_hal.
#if defined(HOJA_LRA_HAL_ENABLE_DRV2605L)
typedef struct
{
    uint8_t i2c_instance; // I2C bus the DRV2605L is on (required)
    uint8_t od_clamp;     // open-loop OD clamp (0 = library default)
} lra_drv2605l_cfg_s;
#endif

#if (HOJA_BSP_CHIPSET == CHIPSET_NRF52)
// LRA haptics config on the nRF52: the two motors behind a stereo I2S amplifier (16 bits a channel at
// the PCM engine's rate). enable_pin switches the amplifier on (high), or NRF52_PIN_NONE. The
// intensity_* values are 0..1 ratios of the amplifier's full scale; leave any at 0.0f to use the
// library default.
typedef struct
{
    uint8_t sck_pin;           // I2S bit clock
    uint8_t lrck_pin;          // I2S left/right clock
    uint8_t sdout_pin;         // I2S data to the amplifier
    uint8_t enable_pin;        // amplifier enable
    bool    channel_b_enable;  // drive a second channel (the right one)
    bool    channel_swap;      // swap left/right routing
    float   intensity_max;     // amplitude ceiling (0.0f = library default)
    float   intensity_min_lo;  // low-frequency amplitude floor (0.0f = default)
    float   intensity_min_hi;  // high-frequency amplitude floor (0.0f = default)
} lra_hal_cfg_s;
#else
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
#if defined(HOJA_LRA_HAL_ENABLE_DRV2605L)
    lra_drv2605l_cfg_s drv2605l;
#endif
} lra_hal_cfg_s;
#endif

#define HOJA_HAPTICS_SET_STD(intensity, brake) lra_hal_set_standard(intensity, brake)

#define HOJA_HAPTICS_PUSH_AMFM(input) lra_hal_push_amfm(input)

#define HOJA_HAPTICS_INIT(intensity) lra_hal_init(intensity)
#define HOJA_HAPTICS_TASK(timestamp) lra_hal_task(timestamp)
#define HOJA_HAPTICS_STOP() lra_hal_stop()

void lra_hal_stop();
bool lra_hal_init(uint8_t intensity);
// Every pass: renders the next PCM buffer into the output once a half is free
void lra_hal_task(uint64_t timestamp);
void lra_hal_push_amfm(haptic_packet_s *packet);
void lra_hal_set_standard(uint8_t intensity, bool brake);
#endif

#endif
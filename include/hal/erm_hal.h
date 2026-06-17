#ifndef HOJA_ERM_HAL_H
#define HOJA_ERM_HAL_H

#include "hoja_bsp.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "board_config.h"
#include "devices_shared_types.h"

#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER==HAPTICS_DRIVER_ERM_HAL)

// ERM haptics config. Single-channel PWM drive with an optional active-brake
// line. intensity_max (0..1) scales the safe PWM ceiling; 0.0f keeps full range.
typedef struct
{
    uint8_t channel_a_pin;  // drive PWM GPIO (required)
    uint8_t brake_pin;      // active-brake GPIO (optional)
    bool    brake_enable;   // use the brake line
    float   intensity_max;  // PWM ceiling ratio (0.0f = full range)
} erm_hal_cfg_s;

#define HOJA_HAPTICS_SET_STD(intensity, brake) erm_hal_set_standard(intensity, brake)

#define HOJA_HAPTICS_PUSH_AMFM(input) erm_hal_push_amfm(input)

#define HOJA_HAPTICS_INIT(intensity) erm_hal_init(intensity)
#define HOJA_HAPTICS_TASK(timestamp) erm_hal_task(timestamp)
#define HOJA_HAPTICS_STOP() erm_hal_stop()

void erm_hal_stop();
bool erm_hal_init(uint8_t intensity);
void erm_hal_task(uint64_t timestamp);
void erm_hal_push_amfm(haptic_packet_s *packet);
void erm_hal_set_standard(uint8_t intensity, bool brake);

#endif

#endif
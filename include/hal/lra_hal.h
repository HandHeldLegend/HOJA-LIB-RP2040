#ifndef HOJA_LRA_HAL_H
#define HOJA_LRA_HAL_H

#include "hoja_bsp.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "board_config.h"
#include "devices_shared_types.h"

#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER==HAPTICS_DRIVER_LRA_HAL)

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
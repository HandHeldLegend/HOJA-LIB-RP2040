#ifndef HOJA_HDRUMBLE_HAL_H
#define HOJA_HDRUMBLE_HAL_H

#include "hoja_bsp.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef HOJA_CONFIG_HDRUMBLE
#if (HOJA_CONFIG_HDRUMBLE==1)

#define SAMPLE_RATE 12000
#define REPETITION_RATE 4
#define BUFFER_SIZE 255
#define SAMPLE_TRANSITION 30
#define PWM_WRAP BUFFER_SIZE

bool hdrumble_hal_init();

#endif
#endif
#endif
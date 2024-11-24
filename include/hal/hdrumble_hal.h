#ifndef HOJA_HDRUMBLE_HAL_H
#define HOJA_HDRUMBLE_HAL_H

#include "hoja_bsp.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "board_config.h"

#ifdef HOJA_CONFIG_HDRUMBLE
#if (HOJA_CONFIG_HDRUMBLE==1)

bool hdrumble_hal_init();

void hdrumble_hal_task(uint32_t timestamp);

void hdrumble_hal_test();

#endif
#endif

#endif
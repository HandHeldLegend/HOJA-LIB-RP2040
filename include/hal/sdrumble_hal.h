#ifndef HOJA_SDRUMBLE_HAL_H
#define HOJA_SDRUMBLE_HAL_H

#include "hoja_bsp.h"

#ifdef HOJA_CONFIG_SDRUMBLE
#if (HOJA_CONFIG_SDRUMBLE==1)

bool sdrumble_hal_init();

#endif
#endif
#endif
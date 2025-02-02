#ifndef HOJA_ADC_HAL_H
#define HOJA_ADC_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_bsp.h"
#include "board_config.h"

bool        adc_hal_init_channel(adc_channel_cfg_s *cfg);
uint16_t    adc_hal_read_channel(adc_channel_cfg_s *cfg);

#endif
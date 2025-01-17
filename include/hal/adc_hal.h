#ifndef HOJA_ADC_HAL_H
#define HOJA_ADC_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_bsp.h"
#include "board_config.h"

uint16_t adc_hal_read(uint8_t ch_local, uint8_t driver_instance);
bool adc_hal_register_driver(uint8_t ch_local, adc_driver_cfg_s *cfg);

#endif
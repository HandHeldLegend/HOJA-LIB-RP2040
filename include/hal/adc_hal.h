#ifndef HOJA_ADC_HAL_H
#define HOJA_ADC_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_bsp.h"
#include "board_config.h"

bool adc_hal_read(adc_hal_driver_s *driver);
bool adc_hal_init(adc_hal_driver_s *driver);

#endif
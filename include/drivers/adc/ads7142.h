#ifndef DRIVERS_ADC_ADS7142_H
#define DRIVERS_ADC_ADS7142_H

#include "board_config.h"
#include "hoja_bsp.h"
#include <stdint.h>
#include <stdbool.h>

#if (HOJA_BSP_HAS_I2C==0)
    #error "ADS7142 driver requires SPI." 
#endif

bool        ads7142_init_channel(adc_channel_cfg_s *cfg);
uint16_t    ads7142_read_channel(adc_channel_cfg_s *cfg);

#endif
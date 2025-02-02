#ifndef DRIVERS_ADC_MCP3002_H
#define DRIVERS_ADC_MCP3002_H

#include "board_config.h"
#include "hoja_bsp.h"
#include <stdint.h>
#include <stdbool.h>

#if (HOJA_BSP_HAS_SPI==0)
    #error "MCP3002 driver requires SPI." 
#endif

bool        mcp3002_init_channel(adc_channel_cfg_s *cfg);
uint16_t    mcp3002_read_channel(adc_channel_cfg_s *cfg);

#endif
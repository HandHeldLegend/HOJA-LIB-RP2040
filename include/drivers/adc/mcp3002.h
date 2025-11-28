#ifndef DRIVERS_ADC_MCP3002_H
#define DRIVERS_ADC_MCP3002_H

#include "board_config.h"
#include "hoja_bsp.h"
#include <stdint.h>
#include <stdbool.h>
#include "driver_define_helper.h"

#if (HOJA_BSP_HAS_SPI==0)
    #error "MCP3002 driver requires SPI." 
#endif

bool mcp3002_init(adc_mcp3002_driver_s *driver);
bool mcp3002_read(adc_mcp3002_driver_s *driver);

#endif
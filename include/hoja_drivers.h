#ifndef HOJA_DRIVERS_H
#define HOJA_DRIVERS_H

#include "board_config.h"

// This header file determines which drivers to include at compile time

#ifdef ADC_DRIVER_MCP3002
#if (ADC_DRIVER_MCP3002 > 0)
    #include "drivers/adc/mcp3002.h"
#endif
#endif

#ifdef IMU_DRIVER_LSM6DSR
#if (IMU_DRIVER_LSM6DSR > 0)
    #include "drivers/imu/lsm6dsr.h"
#endif
#endif

#endif
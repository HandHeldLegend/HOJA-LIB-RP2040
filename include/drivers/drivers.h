#ifndef HOJA_DRIVERS_H
#define HOJA_DRIVERS_H

#include "board_config.h"

#if defined(ADC_DRIVER_MCP3002) && (ADC_DRIVER_MCP3002 > 0)
#include "drivers/adc/mcp3002.h"
#endif

#if defined(IMU_DRIVER_LSM6DSR) && (IMU_DRIVER_LSM6DSR > 0)
#include "drivers/imu/lsm6dsr.h"
#endif

#if defined(HAPTIC_DRIVER_DRV2605L) && (HAPTIC_DRIVER_DRV2605L > 0)
#include "drivers/haptic/drv2605l.h"
#endif

#if defined(BLUETOOTH_DRIVER_ESP32HOJA) && (BLUETOOTH_DRIVER_ESP32HOJA>0)
#include "drivers/bluetooth/esp32_hojabaseband.h"
#endif

void drivers_setup();

#endif
#ifndef HOJA_DRIVERS_H
#define HOJA_DRIVERS_H

#include "board_config.h"

// 10 bit ADC chip with 2 channels
#if defined(ADC_DRIVER_MCP3002) && (ADC_DRIVER_MCP3002 > 0)
#include "drivers/adc/mcp3002.h"
#endif

// IMU gyro/accelerometer sensor chip
#if defined(IMU_DRIVER_LSM6DSR) && (IMU_DRIVER_LSM6DSR > 0)
#include "drivers/imu/lsm6dsr.h"
#endif

// Haptic driver chip. Is fed with HD Rumble HAL
#if defined(HAPTIC_DRIVER_DRV2605L) && (HAPTIC_DRIVER_DRV2605L > 0)
#include "drivers/haptic/drv2605l.h"
#endif

#if defined(BLUETOOTH_DRIVER_ESP32HOJA) && (BLUETOOTH_DRIVER_ESP32HOJA>0)
#include "drivers/bluetooth/esp32_hojabaseband.h"
#endif

// USB Mux switch chip
#if defined(USB_MUX_DRIVER_PI3USB4000A) && (USB_MUX_DRIVER_PI3USB4000A>0)
#include "drivers/mux/pi3usb4000a.h"
#endif

void drivers_setup();

#endif
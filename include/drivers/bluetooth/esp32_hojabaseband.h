#ifndef HOJA_DRIVER_BLUETOOTH_H
#define HOJA_DRIVER_BLUETOOTH_H

// Requires I2C setup

#include <stdint.h>
#include <stdbool.h>

void bluetooth_driver_init();

void bluetooth_driver_start_mode(uint8_t mode, bool pair);

void bluetooth_driver_stop();

void bluetooth_driver_task();

int bluetooth_driver_hwtest();

uint16_t bluetooth_driver_get_version();

#endif
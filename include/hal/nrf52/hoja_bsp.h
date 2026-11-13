#ifndef HOJA_BSP_H
#define HOJA_BSP_H

// Nordic nRF52 (nRF52833): one Cortex-M4F core at 64 MHz, USB device, BLE and 2.4 GHz radio (no
// Bluetooth Classic)

#define CHIPSET_RP2040 0xF001
#define CHIPSET_NRF52  0xF002

#define HOJA_BSP_CHIPSET CHIPSET_NRF52
#define HOJA_BSP_CLOCK_SPEED_HZ 64000000

// One core: the two task loops take turns (hoja.c)
#define HOJA_BSP_CORES 1

// TWIM0 and TWIM1
#define HOJA_BSP_HAS_I2C 2

// SPIM2 and SPIM3 (SPIM0/1 share their hardware with the two TWIMs)
#define HOJA_BSP_HAS_SPI 2

#define HOJA_BSP_HAS_USB 1

// SAADC inputs AIN0-AIN7
#define HOJA_BSP_HAS_ADC 8

// One RGB(W) status LED on a PWM unit
#define HOJA_BSP_HAS_RGB 1

// A GPIO number is port * 32 + pin: P0.xx is xx, P1.xx is 32 + xx
#define NRF52_PIN(port, pin) ((uint8_t)(((port) * 32u) + (pin)))
#define NRF52_PIN_NONE 0xFFu

#endif

#ifndef HOJA_BSP_H
#define HOJA_BSP_H

#define CHIPSET_RP2040 0xF001

#define HOJA_BSP_CHIPSET CHIPSET_RP2040
//#define HOJA_BSP_CLOCK_SPEED_HZ 200000000 // 200 Mhz
#define HOJA_BSP_CLOCK_SPEED_HZ 133000000 // 133 Mhz
// How many I2C peripheral channels there are
#define HOJA_BSP_HAS_I2C 2

// Specific to Pico platforms, how many PIO modules there are
#define HOJA_BSP_HAS_PIO 2

// How many native USB peripherals there are
#define HOJA_BSP_HAS_USB 1

// How many SPI peripheral channels there are 
#define HOJA_BSP_HAS_SPI 2

// How many ADC peripheral channels there are
#define HOJA_BSP_HAS_ADC 4

// How many native/software RGB drivers are available
#define HOJA_BSP_HAS_RGB 1

// How many native/software Joybus GC drivers are available
#define HOJA_BSP_HAS_JOYBUS_GC 1
#define HOJA_BSP_HAS_JOYBUS_N64 1
#define HOJA_BSP_HAS_NESBUS 1

#define HOJA_BSP_HAS_HDRUMBLE 1

#endif
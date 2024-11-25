#ifndef HOJA_BSP_H
#define HOJA_BSP_H

#define HOJA_BSP_CHIPSET CHIPSET_RP2040

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
#define HOJA_BSP_HAS_GCJOYBUS 1
#define HOJA_BSP_HAS_N64JOYBUS 1
#define HOJA_BSP_HAS_NESBUS 1

#define HOJA_BSP_HAS_HDRUMBLE 1

#endif
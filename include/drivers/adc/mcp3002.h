#ifndef DRIVERS_ADC_MCP3002_H
#define DRIVERS_ADC_MCP3002_H

#include "board_config.h"
#include "hoja_bsp.h"
#include <stdint.h>
#include <stdbool.h>

// Only 2 channels per ADC
#define MCP3002_MAX_CHANNELS 1

#ifdef HOJA_ADC_LX_DRIVER
#if HOJA_ADC_LX_DRIVER==ADC_DRIVER_MCP3002
    #ifndef HOJA_ADC_LX_CHANNEL
        #error "Define HOJA_ADC_LX_CHANNEL (0 or 1 for MCP3002) in board_config.h" 
    #endif

    #if HOJA_ADC_LX_CHANNEL > MCP3002_MAX_CHANNELS
        #error "Only 2 channels for MCP3002." 
    #endif

    #ifndef HOJA_ADC_LX_SPI_INSTANCE
        #error "Define HOJA_ADC_LX_SPI_INSTANCE in board_config.h" 
    #endif

    #ifndef HOJA_ADC_LX_CS_PIN
        #error "Define HOJA_ADC_LX_CS_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_LX_READ() mcp3002_read(HOJA_ADC_LX_CHANNEL, HOJA_ADC_LX_CS_PIN, HOJA_ADC_LX_SPI_INSTANCE)
    #define HOJA_ADC_CHAN_LX_INIT() mcp3002_init(HOJA_ADC_LX_CS_PIN)
#endif
#endif

#ifdef HOJA_ADC_LY_DRIVER
#if HOJA_ADC_LY_DRIVER==ADC_DRIVER_MCP3002
    #ifndef HOJA_ADC_LY_CHANNEL
        #error "Define HOJA_ADC_LY_CHANNEL (0 or 1 for MCP3002) in board_config.h" 
    #endif

    #if HOJA_ADC_LY_CHANNEL > MCP3002_MAX_CHANNELS
        #error "Only 2 channels for MCP3002." 
    #endif

    #ifndef HOJA_ADC_LY_SPI_INSTANCE
        #error "Define HOJA_ADC_LY_SPI_INSTANCE in board_config.h" 
    #endif

    #ifndef HOJA_ADC_LY_CS_PIN
        #error "Define HOJA_ADC_LY_CS_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_LY_READ() mcp3002_read(HOJA_ADC_LY_CHANNEL, HOJA_ADC_LY_CS_PIN, HOJA_ADC_LY_SPI_INSTANCE)
    #define HOJA_ADC_CHAN_LY_INIT() mcp3002_init(HOJA_ADC_LY_CS_PIN)
#endif
#endif

#ifdef HOJA_ADC_RX_DRIVER
#if HOJA_ADC_RX_DRIVER==ADC_DRIVER_MCP3002
    #ifndef HOJA_ADC_RX_CHANNEL
        #error "Define HOJA_ADC_RX_CHANNEL (0 or 1 for MCP3002) in board_config.h" 
    #endif

    #if HOJA_ADC_RX_CHANNEL > MCP3002_MAX_CHANNELS
        #error "Only 2 channels for MCP3002." 
    #endif

    #ifndef HOJA_ADC_RX_SPI_INSTANCE
        #error "Define HOJA_ADC_RX_SPI_INSTANCE in board_config.h" 
    #endif

    #ifndef HOJA_ADC_RX_CS_PIN
        #error "Define HOJA_ADC_RX_CS_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_RX_READ() mcp3002_read(HOJA_ADC_RX_CHANNEL, HOJA_ADC_RX_CS_PIN, HOJA_ADC_RX_SPI_INSTANCE)
    #define HOJA_ADC_CHAN_RX_INIT() mcp3002_init(HOJA_ADC_RX_CS_PIN)
#endif
#endif

#ifdef HOJA_ADC_RY_DRIVER
#if HOJA_ADC_RY_DRIVER==ADC_DRIVER_MCP3002
    #ifndef HOJA_ADC_RY_CHANNEL
        #error "Define HOJA_ADC_RY_CHANNEL (0 or 1 for MCP3002) in board_config.h" 
    #endif

    #if HOJA_ADC_RY_CHANNEL > MCP3002_MAX_CHANNELS
        #error "Only 2 channels for MCP3002." 
    #endif

    #ifndef HOJA_ADC_RY_SPI_INSTANCE
        #error "Define HOJA_ADC_RY_SPI_INSTANCE in board_config.h" 
    #endif

    #ifndef HOJA_ADC_RY_CS_PIN
        #error "Define HOJA_ADC_RY_CS_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_RY_READ() mcp3002_read(HOJA_ADC_RY_CHANNEL, HOJA_ADC_RY_CS_PIN, HOJA_ADC_RY_SPI_INSTANCE)
    #define HOJA_ADC_CHAN_RY_INIT() mcp3002_init(HOJA_ADC_RY_CS_PIN)
#endif
#endif

#ifdef HOJA_ADC_LT_DRIVER
#if HOJA_ADC_LT_DRIVER==ADC_DRIVER_MCP3002
    #ifndef HOJA_ADC_LT_CHANNEL
        #error "Define HOJA_ADC_LT_CHANNEL (0 or 1 for MCP3002) in board_config.h" 
    #endif

    #if HOJA_ADC_LT_CHANNEL > MCP3002_MAX_CHANNELS
        #error "Only 2 channels for MCP3002." 
    #endif

    #ifndef HOJA_ADC_LT_SPI_INSTANCE
        #error "Define HOJA_ADC_LT_SPI_INSTANCE in board_config.h" 
    #endif

    #ifndef HOJA_ADC_LT_CS_PIN
        #error "Define HOJA_ADC_LT_CS_PIN in board_config.h" 
    #endif

    #ifndef HOJA_ADC_LT_CS_PIN
        #error "Define HOJA_ADC_LT_CS_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_LT_READ() mcp3002_read(HOJA_ADC_LT_CHANNEL, HOJA_ADC_LT_CS_PIN, HOJA_ADC_LT_SPI_INSTANCE)
    #define HOJA_ADC_CHAN_LT_INIT() mcp3002_init(HOJA_ADC_LT_CS_PIN)
#endif
#endif

#ifdef HOJA_ADC_RT_DRIVER
#if HOJA_ADC_RT_DRIVER==ADC_DRIVER_MCP3002
    #ifndef HOJA_ADC_RT_CHANNEL
        #error "Define HOJA_ADC_RT_CHANNEL (0 or 1 for MCP3002) in board_config.h" 
    #endif

    #if HOJA_ADC_RT_CHANNEL > MCP3002_MAX_CHANNELS
        #error "Only 2 channels for MCP3002." 
    #endif

    #ifndef HOJA_ADC_RT_SPI_INSTANCE
        #error "Define HOJA_ADC_RT_SPI_INSTANCE in board_config.h" 
    #endif

    #ifndef HOJA_ADC_RT_CS_PIN
        #error "Define HOJA_ADC_RT_CS_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_RT_READ() mcp3002_read(HOJA_ADC_RT_CHANNEL, HOJA_ADC_RT_CS_PIN, HOJA_ADC_RT_SPI_INSTANCE)
    #define HOJA_ADC_CHAN_RT_INIT() mcp3002_init(HOJA_ADC_RT_CS_PIN)
#endif
#endif

#ifdef HOJA_ADC_BATTERY_DRIVER
#if HOJA_ADC_BATTERY_DRIVER==ADC_DRIVER_MCP3002
    #ifndef HOJA_ADC_BATTERY_CHANNEL
        #error "Define HOJA_ADC_BATTERY_CHANNEL number for HAL in board_config.h" 
    #endif

    #if HOJA_ADC_BATTERY_CHANNEL > HOJA_BSP_HAS_ADC
        #error "Only 2 channels for MCP3002." 
    #endif

    #ifndef HOJA_ADC_BATTERY_SPI_INSTANCE
        #error "Define HOJA_ADC_BATTERY_SPI_INSTANCE in board_config.h" 
    #endif

    #ifndef HOJA_ADC_BATTERY_CS_PIN
        #error "Define HOJA_ADC_BATTERY_CS_PIN in board_config.h" 
    #endif

    #define HOJA_ADC_CHAN_BATTERY_READ() mcp3002_read(HOJA_ADC_BATTERY_CHANNEL, HOJA_ADC_BATTERY_CS_PIN, HOJA_ADC_BATTERY_SPI_INSTANCE)
    #define HOJA_ADC_CHAN_BATTERY_INIT() mcp3002_init(HOJA_ADC_BATTERY_CS_PIN)
#endif
#endif

#if (HOJA_BSP_HAS_SPI==0)
    #error "MCP3002 driver requires SPI." 
#endif

uint16_t    mcp3002_read(bool channel, uint32_t cs_gpio, uint8_t spi_instance);
bool        mcp3002_init(uint32_t cs_gpio);

#endif
#ifndef DRIVER_DEFINE_HELPER_H
#define DRIVER_DEFINE_HELPER_H

#include <stdint.h>
#include "input/mapper.h"

    typedef enum {
        INPUT_TYPE_UNUSED,
        INPUT_TYPE_DIGITAL,
        INPUT_TYPE_HOVER,
        INPUT_TYPE_JOYSTICK
    } input_type_t;

    typedef struct {
        uint8_t  gpio;
        uint8_t  ch; // READ ONLY
        bool initialized; // READ ONLY
        uint16_t output; // READ ONLY
    } adc_hal_driver_s;

    typedef struct {
        uint8_t  cs_gpio;
        uint8_t  spi_instance; 
        uint16_t output_ch_0; // READ ONLY
        uint16_t output_ch_1; // READ ONLY
        bool initialized; // READ ONLY
    } adc_mcp3002_driver_s;

    typedef struct {
        uint8_t a0_gpio; 
        uint8_t a1_gpio; 
        bool initialized; // READ ONLY
    } mux_tmux1204_driver_s;

    // ------------------------------
    // Helper macro: convert name to mask
    // ------------------------------
    #define HOJA_MASK(btn) (1u << HOJA_BTN_##btn)

    // ------------------------------
    // Macro to define input mask
    // Usage: just list buttons you want enabled
    // ------------------------------
    #define HOJA_INPUT_MASK(...) (0u __VA_ARGS__)

    // Helper to expand a list of buttons
    #define HOJA_ENABLE_INPUT(code) | HOJA_MASK(code)
    
    // IMU Drivers
    #define IMU_DRIVER_LSM6DSR_SPI 1
    #define IMU_DRIVER_LSM6DSR_I2C 2

    // Haptic helper Drivers 
    #define HAPTIC_HELPER_DRIVER_DRV2605L 1

    // Battery Drivers
    #define BATTERY_DRIVER_BQ25180 1

    // Fuel Gauge Drivers
    #define FUELGAUGE_DRIVER_ADC 1
    #define FUELGAUGE_DRIVER_ESP32 2
    #define FUELGAUGE_DRIVER_BQ27621G1 3

    // Bluetooth Drivers 
    #define BLUETOOTH_DRIVER_HAL 1
    #define BLUETOOTH_DRIVER_ESP32HOJA 2

    // Mux (USB) Drivers
    #define USB_MUX_DRIVER_PI3USB4000A 1

    // RGB Drivers
    #define RGB_DRIVER_HAL 1

    // RGB type stuff
    #define RGB_ORDER_RGB 0
    #define RGB_ORDER_GRB 1
    #define RGB_MAX_LEDS_PER_GROUP 4
    #define RGB_MAX_GROUP_NAME_LEN 8

    // Drivers for haptics
    #define HAPTICS_DRIVER_LRA_HAL 1
    #define HAPTICS_DRIVER_ERM_HAL 2

    // Haptix full/half duplex
    #define HAPTICS_DUPLEX_HALF 1
    #define HAPTICS_DUPLEX_FULL 2

    // NESBus Drivers
    #define NESBUS_DRIVER_HAL 1

    // Joybus Drivers
    #define JOYBUS_N64_DRIVER_HAL 1

    #define JOYBUS_GC_DRIVER_HAL 1

    // Button layout for SEWN types
    #define SEWN_LAYOUT_ABXY 0 // Xbox Style
    #define SEWN_LAYOUT_BAYX 1 // Nintendo Style
    #define SEWN_LAYOUT_AXBY 2 // GameCube Style

#endif
#ifndef DRIVER_DEFINE_HELPER_H
#define DRIVER_DEFINE_HELPER_H

#include <stdint.h>
#include "input/mapper.h"

    typedef enum {
        ADC_CH_LX,
        ADC_CH_LY,
        ADC_CH_RX,
        ADC_CH_RY,
        ADC_CH_LT,
        ADC_CH_RT,
        ADC_CH_BAT,
        ADC_CH_MAX,
    } adc_ch_t;

    typedef enum {
        ADC_DRIVER_NONE,
        ADC_DRIVER_HAL,
        ADC_DRIVER_MCP3002,
        ADC_DRIVER_TMUX1204,
        ADC_DRIVER_ADS7142
    } adc_driver_t;

    /**
     * A driver may exist more than once,
     * but we take care to isolate driver instances 
     * from our channel configuration. A channel configuration
     * points to a driver configuration instance
    **/

    typedef struct {
        int8_t  gpio;
    } adc_hal_cfg_t;

    typedef struct {
        int8_t  cs_gpio;
        int8_t  spi_instance;
    } adc_mcp3002_cfg_t;

    typedef struct { 
        int8_t i2c_instance;
    } adc_ads7142_cfg_t;

    // Forward declare the driver config struct
    typedef struct adc_driver_cfg_s adc_driver_cfg_s;
    typedef struct adc_channel_cfg_s adc_channel_cfg_s;

    typedef struct {
        adc_driver_cfg_s *host_cfg; // Configuration of host driver (TMUX1204 needs another ADC to send data to)
        int8_t          host_ch_local; // Local channel for host driver. IE if the output is going to ADC HAL ch 0, use ch 0!
        int8_t          a0_gpio;
        int8_t          a1_gpio;
    } adc_tmux1204_cfg_t;

    // ADC Drivers
    struct adc_driver_cfg_s
    {
        adc_driver_t    driver_type;        // Type of ADC driver
        int8_t          driver_instance;    // Instance of driver

        // CFG data for specific driver
        union 
        {
            adc_hal_cfg_t       hal_cfg;
            adc_mcp3002_cfg_t   mcp3002_cfg;
            adc_tmux1204_cfg_t  tmux1204_cfg;
            adc_ads7142_cfg_t   ads7142_cfg;
        };
    };

    struct adc_channel_cfg_s {
        int8_t  ch_local; // Local channel for the given driver
        uint8_t ch_invert;
        adc_driver_cfg_s *driver_cfg;
    };


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


    typedef uint16_t(*adc_read_fn_t)(adc_channel_cfg_s *cfg);
    
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
    #define SEWN_LAYOUT_ABXY 0 //Xbox Style
    #define SEWN_LAYOUT_BAYX 1 // Nintendo Style
    #define SEWN_LAYOUT_AXBY 2 // GameCube Style

#endif
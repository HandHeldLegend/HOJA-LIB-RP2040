#ifndef DRIVER_DEFINE_HELPER_H
#define DRIVER_DEFINE_HELPER_H

#include <stdint.h>
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

    // Forward declare the driver config struct
    typedef struct adc_driver_cfg_s adc_driver_cfg_s;

    typedef struct {
        adc_driver_cfg_s *host_cfg;
        int8_t          host_ch_local; // Local channel for host driver
        int8_t          en_gpio;
        int8_t          a1_gpio;
        int8_t          a2_gpio;
    } adc_tmux1204_cfg_t;

    // ADC Drivers
    struct adc_driver_cfg_s
    {
        adc_driver_t    driver_type;    // Type of ADC driver
        int8_t          driver_instance; // Driver number used to read 
        union 
        {
            adc_hal_cfg_t       hal_cfg;
            adc_mcp3002_cfg_t   mcp3002_cfg;
            adc_tmux1204_cfg_t  tmux1204_cfg;
        };
    };

    typedef struct {
        int8_t  ch_local; // Local channel for the given driver
        uint8_t ch_invert;
        adc_driver_cfg_s *driver_cfg;
    } adc_channel_cfg_s;

    typedef uint16_t(*adc_read_fn_t)(uint8_t ch_local, uint8_t driver_instance);

    typedef struct 
    {
        int8_t          driver_instance; // Driver number to pass to our read function
        int8_t          ch_local;   // Local channel for the driver
        adc_read_fn_t   read_fn;    // Function pointer to read ADC data
    } adc_local_read_s;
    
    // IMU Drivers
    #define IMU_DRIVER_LSM6DSR 1

    // Haptic helper Drivers 
    #define HAPTIC_HELPER_DRIVER_DRV2605L 1

    // Battery Drivers
    #define BATTERY_DRIVER_BQ25180 1

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

    // HD and SD drivers for haptics
    #define HD_HAPTICS_DRIVER_HAL 1

    #define SD_HAPTICS_DRIVER_HAL 1

    // NESBus Drivers
    #define NESBUS_DRIVER_HAL 1

    // Joybus Drivers
    #define JOYBUS_N64_DRIVER_HAL 1

    #define JOYBUS_GC_DRIVER_HAL 1

#endif
#ifndef DRIVER_DEFINE_HELPER_H
#define DRIVER_DEFINE_HELPER_H

    // ADC Drivers
    #define ADC_DRIVER_HAL 1
    #define ADC_DRIVER_MCP3002 2

    // RGB Drivers
    #define RGB_DRIVER_HAL 1 
    
    // IMU Drivers
    #define IMU_DRIVER_LSM6DSR 1

    // Haptic Drivers 
    #define HAPTIC_DRIVER_DRV2605L 1

    // Battery Drivers
    #define BATTERY_DRIVER_BQ25180 1

    // Bluetooth Drivers 
    #define BLUETOOTH_DRIVER_HAL 1
    #define BLUETOOTH_DRIVER_ESP32HOJA 2

    // Mux (USB) Drivers
    #define USB_MUX_DRIVER_PI3USB4000A 1

    // RGB type stuff
    #define RGB_ORDER_RGB 0
    #define RGB_ORDER_GRB 1
    #define RGB_MAX_LEDS_PER_GROUP 4
    #define RGB_MAX_GROUP_NAME_LEN 8

#endif
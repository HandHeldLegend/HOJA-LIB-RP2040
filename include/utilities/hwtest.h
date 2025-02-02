#ifndef UTILITIES_HWTEST_H
#define UTILITIES_HWTEST_H

#include <stdint.h>
#include <stdbool.h>

// HW Test union type
typedef union
{
    struct
    {
        bool data_pin   : 1;
        bool latch_pin  : 1;
        bool clock_pin  : 1;
        bool rgb_pin    : 1;
        bool analog     : 1;
        bool imu        : 1;
        bool bluetooth  : 1;
        bool battery    : 1;
        bool rumble     : 1;

        uint8_t empty : (8);
    };
    uint16_t val;
    
} hoja_hw_test_u;

#endif
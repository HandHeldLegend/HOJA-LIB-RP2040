#ifndef DEVICES_SHARED_TYTPES_H 
#define DEVICES_SHARED_TYTPES_H

#include <stdint.h>
#include "hoja_system.h"

// Handle RGB mode choosing compiler side
#if (RGB_DEVICE_ENABLED==1)
#if (RGB_DRIVER_ORDER == RGB_ORDER_GRB)
typedef struct
{
    union
    {
        struct
        {
            uint8_t padding : 8;
            uint8_t b : 8;
            uint8_t r : 8;
            uint8_t g : 8;
        };
        uint32_t color;
    };
} rgb_s;
#else 
typedef struct
{
    union
    {
        struct
        {
            uint8_t padding : 8;
            uint8_t b : 8;
            uint8_t g : 8;
            uint8_t r : 8;
        };
        uint32_t color;
    };
} rgb_s;
#endif 
#endif

#endif
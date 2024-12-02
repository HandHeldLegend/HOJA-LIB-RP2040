#ifndef DEVICES_SHARED_TYTPES_H 
#define DEVICES_SHARED_TYTPES_H

#include <stdint.h>

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

#endif
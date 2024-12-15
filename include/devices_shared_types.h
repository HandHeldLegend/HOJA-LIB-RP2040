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

typedef struct
{
    uint16_t hue;
    uint16_t saturation;
    uint16_t value;
} hsv_s;

#define COLOR_RED    (rgb_s) {.r = 0xFF, .g = 0x00, .b = 0x00}
#define COLOR_ORANGE (rgb_s) {.r = 0xFF, .g = 0x4d, .b = 0x00}
#define COLOR_YELLOW (rgb_s) {.r = 0xFF, .g = 0xE6, .b = 0x00}
#define COLOR_GREEN  (rgb_s) {.r = 0x00, .g = 0xff, .b = 0x00}
#define COLOR_BLUE   (rgb_s) {.r = 0x00, .g = 0x00, .b = 0xFF}
#define COLOR_CYAN   (rgb_s) {.r = 0x15, .g = 0xFF, .b = 0xF1}
#define COLOR_PURPLE (rgb_s) {.r = 0x61, .g = 0x00, .b = 0xff}
#define COLOR_PINK   (rgb_s) {.r = 0xff, .g = 0x2B, .b = 0xD0}
#define COLOR_WHITE  (rgb_s) {.r = 0xa1, .g = 0xa1, .b = 0xa1}
#define COLOR_BLACK  (rgb_s) {.r = 0x00, .g = 0x00, .b = 0x00}

#define COLORS_RAINBOW {COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN, COLOR_BLUE, COLOR_PURPLE}

#endif
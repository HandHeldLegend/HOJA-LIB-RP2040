#ifndef DEVICES_SHARED_TYTPES_H 
#define DEVICES_SHARED_TYTPES_H

#include <stdint.h>
#include "board_config.h"

// RGB driver defaults. board_config.h only needs to override these (the color
// order shapes rgb_s below, so it must be known at compile time; LED count sizes
// the animation buffers). Both are optional in board_config.h.
#ifndef RGB_DRIVER_ORDER
#define RGB_DRIVER_ORDER RGB_ORDER_GRB
#endif
#ifndef RGB_DRIVER_LED_COUNT
#define RGB_DRIVER_LED_COUNT 32
#endif
// LED frames per animation frame (optional). Above 1, the LED driver plays RGB_DITHER_FRAMES frames in
// turn at that rate from hardware, each channel dithered between its two nearest levels, so fades on
// dim LEDs don't step. The chain must fit in one frame with its latch gap (~30 us per LED + 300 us).
#ifndef RGB_DRIVER_OUTPUT_FRAMES
#define RGB_DRIVER_OUTPUT_FRAMES 1
#endif
// The dither cycle: each channel's level is kept in 1/RGB_DITHER_FRAMES steps
#define RGB_DITHER_FRAMES 8

// Handle RGB mode choosing compiler side
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

typedef struct
{
    uint16_t hue;
    uint16_t saturation;
    uint16_t value;
} hsv_s;

typedef union
{
    struct
    {
        uint16_t    hi_amplitude_fixed;
        uint16_t    lo_amplitude_fixed;
        uint16_t    hi_frequency_increment;
        uint16_t    lo_frequency_increment;
    };
    uint64_t value; 
} haptic_processed_s;

typedef struct
{
    haptic_processed_s pairs[3];
    uint8_t count;
} haptic_packet_s;

typedef union
{
    struct
    {
        uint8_t power_source : 1;
        uint8_t connection : 2;
        uint8_t reserved : 1;
        uint8_t charging : 1;
        uint8_t bat_lvl : 3;
    };
    uint8_t val;
} bat_status_u;

typedef union
{
    struct
    {
        uint8_t power_source : 1;
        uint8_t connection : 2;
        uint8_t reserved : 1;
        uint8_t charging : 1;
        uint8_t bat_lvl : 3;
    };
    uint8_t val;
} esp32_battery_status_u;

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
#define COLORS_RAINBOW_CYCLE {COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_BLUE}

#define HAPTICS_BASE_HFREQ 320
#define HAPTICS_BASE_LFREQ 130
#define HAPTICS_BASE_AMP 0.55f

#endif
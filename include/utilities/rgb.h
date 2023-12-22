#ifndef RGB_F_H
#define RGB_F_H

#include "hoja_includes.h"
#include "interval.h"

//#define RGB_S_COLOR(r, g, b) ((rgb_s) { .color = ((r << 16) | (g << 8) | (b))})

#define COLOR_RED    (rgb_s) {.r = 0xFF, .g = 0x00, .b = 0x00}
#define COLOR_ORANGE (rgb_s) {.r = 0xFF, .g = 0x4d, .b=0x00}
#define COLOR_YELLOW (rgb_s) {.r = 0xFF, .g = 0xE6, .b=0x00}
#define COLOR_GREEN  (rgb_s) {.r = 0x00, .g = 0xff, .b=0x00}
#define COLOR_BLUE   (rgb_s) {.r = 0x00, .g = 0x00, .b=0xFF}
#define COLOR_CYAN   (rgb_s) {.r = 0x15, .g = 0xFF, .b=0xF1}
#define COLOR_PURPLE (rgb_s) {.r = 0x61, .g = 0x00, .b=0xff}
#define COLOR_PINK   (rgb_s) {.r = 0xff, .g = 0x2B, .b=0xD0}
#define COLOR_WHITE  (rgb_s) {.r = 0xa1, .g = 0xa1, .b=0xa1}
#define COLOR_BLACK  (rgb_s) {.r = 0x00, .g = 0x00, .b=0x00}

typedef struct
{
    uint16_t hue;
    uint16_t saturation;
    uint16_t value;
} hsv_s;

typedef enum {
    RGB_MODE_PRESET = 0,
    RGB_MODE_RAINBOW,
    RGB_MODE_RAINBOWOFFSET,
    RGB_MODE_CYCLE,
    RGB_MODE_CYCLEOFFSET,
    RGB_MODE_FLASH,
    RGB_MODE_REACTIVE,
    RGB_MODE_MAX,
} rgb_mode_t;

typedef enum{
    RGB_GROUP_RS = 0,
    RGB_GROUP_LS = 1,
    RGB_GROUP_DPAD = 2,
    RGB_GROUP_MINUS = 3,
    RGB_GROUP_CAPTURE = 4,
    RGB_GROUP_HOME = 5,
    RGB_GROUP_PLUS = 6,
    RGB_GROUP_Y = 7,
    RGB_GROUP_X = 8,
    RGB_GROUP_A = 9,
    RGB_GROUP_B = 10,
    RGB_GROUP_MAX,
} rgb_group_t;

// GC Mode Preset Defines
#define PRESET_GC_A     (rgb_s) {.r = 0x00, .g = 0x18, .b = 0x0A}
#define PRESET_GC_B     (rgb_s) {.r = 0x0F, .g = 0x02, .b = 0x02}
#define PRESET_GC_C     (rgb_s) {.r = 0x14, .g = 0x0B, .b = 0x00}
#define PRESET_GC_OTHER (rgb_s) {.r = 0x04, .g = 0x04, .b = 0x04}

/**
 * @brief Typedef for the callback function used in RGB animations.
 * 
 * This typedef defines a function pointer type named `rgb_anim_cb` that points to a function
 * with a void return type and no parameters. This callback function is used in RGB animations
 * to perform custom actions at specific points in the animation sequence.
 */
typedef void (*rgb_anim_cb)(void);

typedef bool (*rgb_override_anim_cb)(void);

void rgb_update_speed(uint8_t speed);

void rgb_shutdown_start();

void rgb_indicate(uint32_t color);

void rgb_flash(uint32_t color);

void rgb_set_group(rgb_group_t group, uint32_t color, bool instant);

void rgb_init(rgb_mode_t mode, int brightness);

void rgb_task(uint32_t timestamp);

#endif

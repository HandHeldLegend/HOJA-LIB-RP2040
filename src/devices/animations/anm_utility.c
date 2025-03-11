#include "devices/animations/anm_utility.h"
#include "devices/animations/anm_handler.h"
#include "devices/rgb.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

#define CAPUINT8(value) ((value > 255) ? 255 : value)

#define DITHER_MAX_LOOP 12
#define DITHER_TABLE_SIZE 6
#define DITHER_BRIGHT_MAX (255*DITHER_TABLE_SIZE)
uint16_t _dithering_table[6] = {
    0b000000000000, // 1524 idx 0
    0b100000100000, // 1525 idx 1
    0b100100100100, // 1526 idx 2
    0b101010101010, // 1527 idx 3
    0b110110110110, // 1528 idx 4
    0b111110111110, // 1529 idx 5
};

const uint8_t gamma8[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

uint32_t _stored_fixed_point_time = 0;



void anm_utility_apply_color_safety(rgb_s *color)
{
    if (!color->color)
        return;

    float total = (float)(color->r + color->g + color->b);

    float rf = (float)color->r / total;
    float gf = (float)color->g / total;
    float bf = (float)color->b / total;

    color->r = (float)color->r * rf;
    color->g = gf * (float)color->g;
    color->b = bf * (float)color->b;
}

uint32_t anm_utility_pack_local_color(rgb_s color)
{
    return  
    (color.r << 16) |
    (color.g << 8) |
    (color.b);
}

rgb_s anm_utility_unpack_local_color(uint32_t color)
{
    rgb_s out = {
        .r = (color & 0xFF0000) >> 16,
        .g = (color & 0xFF00) >> 8,
        .b = (color & 0xFF)
    };
    return out;
}

// Get our current animation fade fixed point time
uint32_t anm_utility_get_time_fixedpoint()
{
    return _stored_fixed_point_time;
}

// Set our animation step fade time in ms
void anm_utility_set_time_ms(uint32_t time_ms)
{
    uint32_t us = MS_TO_US(time_ms);
    uint32_t ticks = us / RGB_TASK_INTERVAL;
    _stored_fixed_point_time = RGB_FLOAT_TO_FIXED(1.0f / (float)ticks);
}

// Brightness is 0 to 1275 range
void anm_utility_process(rgb_s *in, rgb_s *out, uint16_t brightness)
{
    static uint8_t repeating = 0;
    brightness = (brightness>DITHER_BRIGHT_MAX) ? DITHER_BRIGHT_MAX : brightness;
    if(brightness)
        brightness = (brightness<50) ? 50 : brightness;
    uint16_t    dither_idx = brightness % DITHER_TABLE_SIZE;
    uint8_t     dither_amt = (_dithering_table[dither_idx] >> repeating) & 0x1;

    uint8_t brightness_u8 = brightness/6;

    // If we have 0 brightness, always illuminate our player
    // status LEDs
    if(!brightness)
    {
        // Set all out to 0
        for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
        {
            out[i].color = 0;
        }

        #if defined(HOJA_RGB_PLAYER_GROUP_IDX)
        for(int i = 0; i < HOJA_RGB_PLAYER_GROUP_SIZE; i++)
        {
            uint8_t w = rgb_led_groups[HOJA_RGB_PLAYER_GROUP_IDX][i];

            out[w].r = (in[w].r * 8 + 127) >> 8;
            out[w].r += dither_amt;

            out[w].g = (in[w].g * 8 + 127) >> 8;
            out[w].g += dither_amt;

            out[w].b = (in[w].b * 8 + 127) >> 8;
            out[w].b += dither_amt;
        }
        #endif 

        #if defined(HOJA_RGB_NOTIF_GROUP_IDX)
        for(int i = 0; i < HOJA_RGB_NOTIF_GROUP_SIZE; i++)
        {
            uint8_t w = rgb_led_groups[HOJA_RGB_NOTIF_GROUP_IDX][i];

            out[w].r = (in[w].r * 8 + 127) >> 8;
            out[w].r += dither_amt;

            out[w].g = (in[w].g * 8 + 127) >> 8;
            out[w].g += dither_amt;

            out[w].b = (in[w].b * 8 + 127) >> 8;
            out[w].b += dither_amt;
        }
        #endif 

        repeating = (repeating+1)%DITHER_MAX_LOOP;
        return;
    }

    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
    {
        // Only apply if above 0
        if(in[i].r)
        {
            // First apply gamma
            //out[i].r = gamma8[in[i].r];
            // Apply brightness u8
            out[i].r = (in[i].r * brightness_u8 + 127) >> 8;
            // Apply dither
            out[i].r += dither_amt;
        }
        else out[i].r = 0;
        
        if(in[i].g)
        {
            //out[i].g = gamma8[in[i].g];
            out[i].g = (in[i].g * brightness_u8 + 127) >> 8;
            out[i].g += dither_amt;
        }
        else out[i].g = 0;

        if(in[i].b)
        {
            //out[i].b = gamma8[in[i].b];
            out[i].b = (in[i].b * brightness_u8 + 127) >> 8;
            out[i].b += dither_amt;
        }
        else out[i].b = 0;
    }

    repeating = (repeating+1)%DITHER_MAX_LOOP;
}

// Blend two RGB values together with a fixed point val (0 to (1<<16))
uint32_t anm_utility_blend(rgb_s *from, rgb_s *target, uint32_t blend)
{
    if(blend >= RGB_FADE_FIXED_MULT) blend = RGB_FADE_FIXED_MULT;

    int32_t or = (int32_t)from->r;
    int32_t og = (int32_t)from->g;
    int32_t ob = (int32_t)from->b;
    int32_t nr = (int32_t)target->r;
    int32_t ng = (int32_t)target->g;
    int32_t nb = (int32_t)target->b;

    // Do the blend calculation with proper fixed-point math
    uint32_t outr = (uint32_t)(or * (RGB_FADE_FIXED_MULT - blend) + nr * blend) >> RGB_FADE_FIXED_SHIFT;
    uint32_t outg = (uint32_t)(og * (RGB_FADE_FIXED_MULT - blend) + ng * blend) >> RGB_FADE_FIXED_SHIFT;
    uint32_t outb = (uint32_t)(ob * (RGB_FADE_FIXED_MULT - blend) + nb * blend) >> RGB_FADE_FIXED_SHIFT;

    rgb_s col = {
        .r = (uint8_t)outr,
        .g = (uint8_t)outg,
        .b = (uint8_t)outb
    };
    return col.color;
}

// Convert milliseconds to frames according to our refresh interval
uint32_t anm_utility_ms_to_frames(uint32_t time_ms)
{
    int calc = (1000 * time_ms) / RGB_TASK_INTERVAL;

    return (uint32_t) calc;
}

// Public CHSV color functions
#define HSV_SECTION_6 (0x20)
#define HSV_SECTION_3 (0x40)
// Function taken from https://github.com/FastLED/FastLED/blob/master/src/hsv2rgb.cpp
/**
 * @brief Take in HSV and return rgb
 * @param h Hue
 * @param s Saturation
 * @param v Value (Brightness)
 */
void anm_utility_hsv_to_rgb(hsv_s *hsv, rgb_s *out)
{
    float scale = (float)hsv->hue / 255;
    float hf = scale * 191;
    uint8_t hue = (uint8_t)hf;

    rgb_s color_out = {
        .r = 0x00,
        .g = 0x00,
        .b = 0x00,
    };

    // Convert hue, saturation and brightness ( HSV/HSB ) to RGB
    // "Dimming" is used on saturation and brightness to make
    // the output more visually linear.

    // Apply dimming curves
    uint8_t value = hsv->value;
    uint8_t saturation = hsv->saturation;

    // The brightness floor is minimum number that all of
    // R, G, and B will be set to.
    uint8_t invsat = 255 - saturation;
    uint8_t brightness_floor = (value * invsat) / 256;

    // The color amplitude is the maximum amount of R, G, and B
    // that will be added on top of the brightness_floor to
    // create the specific hue desired.
    uint8_t color_amplitude = value - brightness_floor;

    // Figure out which section of the hue wheel we're in,
    // and how far offset we are withing that section
    uint8_t section = hue / HSV_SECTION_3; // 0..2
    uint8_t offset = hue % HSV_SECTION_3;  // 0..63

    uint8_t rampup = offset;                         // 0..63
    uint8_t rampdown = (HSV_SECTION_3 - 1) - offset; // 63..0

    // compute color-amplitude-scaled-down versions of rampup and rampdown
    uint8_t rampup_amp_adj = (rampup * color_amplitude) / (256 / 4);
    uint8_t rampdown_amp_adj = (rampdown * color_amplitude) / (256 / 4);

    // add brightness_floor offset to everything
    uint8_t rampup_adj_with_floor = rampup_amp_adj + brightness_floor;
    uint8_t rampdown_adj_with_floor = rampdown_amp_adj + brightness_floor;

    if (section)
    {
        if (section == 1)
        {
            // section 1: 0x40..0x7F
            color_out.r = brightness_floor;
            color_out.g = rampdown_adj_with_floor;
            color_out.b = rampup_adj_with_floor;
        }
        else
        {
            // section 2; 0x80..0xBF
            color_out.r = rampup_adj_with_floor;
            color_out.g = brightness_floor;
            color_out.b = rampdown_adj_with_floor;
        }
    }
    else
    {
        // section 0: 0x00..0x3F
        color_out.r = rampdown_adj_with_floor;
        color_out.g = rampup_adj_with_floor;
        color_out.b = brightness_floor;
    }

    out->color = color_out.color;
}

static inline uint32_t _urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) |
           ((uint32_t)(g) << 16) |
           (uint32_t)(b);
}
#endif
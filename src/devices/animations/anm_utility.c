#include "devices/animations/anm_utility.h"
#include "devices/animations/anm_handler.h"
#include "devices/rgb.h"
#include "hoja.h"

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

// Below this output level a channel is rounded, not dithered: +1 on a few is a 15-30% step, and at
// the LEDs' frame rate (60-90 Hz) the dither patterns repeat at 10-45 Hz, a visible flicker on dim
// LEDs. From here up a step is 3% or less and the dither only smooths fades.
#define DITHER_MIN_LEVEL 32u
// Brightness 0 still lights the player and notification LEDs, at this fraction of 256
#define STATUS_ONLY_SCALE 8u

// One channel at brightness (0 to DITHER_BRIGHT_MAX): the exact level, dithered between its two
// nearest steps when bright enough to hide it, else rounded and held
static inline uint8_t _anm_utility_channel(uint8_t in, uint32_t brightness, uint8_t phase)
{
    if (!in)
        return 0;
    uint32_t v = (uint32_t)in * brightness;
    uint32_t base = v / DITHER_BRIGHT_MAX;
    if (base < DITHER_MIN_LEVEL)
        return (uint8_t)CAPUINT8((v + DITHER_BRIGHT_MAX / 2u) / DITHER_BRIGHT_MAX);
    uint32_t idx = ((v % DITHER_BRIGHT_MAX) * DITHER_TABLE_SIZE) / DITHER_BRIGHT_MAX;
    return (uint8_t)CAPUINT8(base + ((_dithering_table[idx] >> phase) & 0x1u));
}

#if (RGB_DRIVER_OUTPUT_FRAMES > 1)
// out holds RGB_DITHER_FRAMES frames back to back, played in turn by the LED driver
#define ANM_OUT_FRAMES RGB_DITHER_FRAMES

// Which of the frames take the upper level, for a channel n/8 of the way there: spread evenly, so
// even 1/8 repeats at RGB_DRIVER_REFRESHRATE * RGB_DRIVER_OUTPUT_FRAMES / 8 (90 Hz at 60 x 12),
// too fast to flicker (the 10-45 Hz patterns at a 60 Hz refresh did)
static const uint8_t _anm_dither_masks[RGB_DITHER_FRAMES] = {
    0x00, 0x80, 0x88, 0xA4, 0xAA, 0xDA, 0xEE, 0xFE
};

// One channel across the frames: its two nearest levels in the right proportion. A channel on a
// whole level gets the same value in every frame.
static inline void _anm_utility_dither(uint8_t in, uint32_t brightness, uint8_t rotate, uint8_t *base, uint8_t *mask)
{
    uint32_t eighths = ((uint32_t)in * brightness * RGB_DITHER_FRAMES + DITHER_BRIGHT_MAX / 2u) / DITHER_BRIGHT_MAX;
    uint32_t m = _anm_dither_masks[eighths % RGB_DITHER_FRAMES];
    *base = (uint8_t)CAPUINT8(eighths / RGB_DITHER_FRAMES);
    *mask = (uint8_t)((m << rotate) | (m >> ((RGB_DITHER_FRAMES - rotate) % RGB_DITHER_FRAMES)));
}
#else
#define ANM_OUT_FRAMES 1
#endif

// One LED at brightness: its output, or with output frames its value in each of them
static inline void _anm_utility_led(const rgb_s *in, rgb_s *out, int i, uint32_t brightness, uint8_t phase)
{
    #if (RGB_DRIVER_OUTPUT_FRAMES > 1)
    (void)phase;
    // Each LED's pattern starts at its own frame, so a dimmed pad doesn't pulse in step
    const uint8_t rotate = (uint8_t)((i * 3u) % RGB_DITHER_FRAMES);
    uint8_t r, g, b, mr, mg, mb;
    _anm_utility_dither(in[i].r, brightness, rotate, &r, &mr);
    _anm_utility_dither(in[i].g, brightness, rotate, &g, &mg);
    _anm_utility_dither(in[i].b, brightness, rotate, &b, &mb);
    for (int f = 0; f < RGB_DITHER_FRAMES; f++)
    {
        rgb_s *o = &out[f * RGB_DRIVER_LED_COUNT + i];
        o->r = (uint8_t)(r + ((mr >> f) & 1u));
        o->g = (uint8_t)(g + ((mg >> f) & 1u));
        o->b = (uint8_t)(b + ((mb >> f) & 1u));
    }
    #else
    out[i].r = _anm_utility_channel(in[i].r, brightness, phase);
    out[i].g = _anm_utility_channel(in[i].g, brightness, phase);
    out[i].b = _anm_utility_channel(in[i].b, brightness, phase);
    #endif
}

// Brightness is 0 to DITHER_BRIGHT_MAX (1530)
void anm_utility_process(rgb_s *in, rgb_s *out, uint16_t brightness)
{
    static uint8_t repeating = 0;
    brightness = (brightness>DITHER_BRIGHT_MAX) ? DITHER_BRIGHT_MAX : brightness;
    if(brightness)
        brightness = (brightness<50) ? 50 : brightness;

    // If we have 0 brightness, always illuminate our player
    // status LEDs
    if(!brightness)
    {
        // Set all out to 0
        for(int i = 0; i < RGB_DRIVER_LED_COUNT * ANM_OUT_FRAMES; i++)
        {
            out[i].color = 0;
        }

        const hoja_rgb_cfg_s *rcfg = &hoja_config_get()->rgb;
        const uint32_t status_bright = (DITHER_BRIGHT_MAX * STATUS_ONLY_SCALE) / 256u;

        if(rcfg->player_group_index >= 0)
        {
            for(int i = 0; i < RGB_PLAYER_GROUP_SIZE; i++)
            {
                int8_t w = rgb_led_groups[rcfg->player_group_index][i];
                if(w < 0) continue;
                _anm_utility_led(in, out, w, status_bright, repeating);
            }
        }

        if(rcfg->notification_group_index >= 0)
        {
            for(int i = 0; i < rcfg->notification_group_size && i < RGB_MAX_LEDS_PER_GROUP; i++)
            {
                int8_t w = rgb_led_groups[rcfg->notification_group_index][i];
                if(w < 0) continue;
                _anm_utility_led(in, out, w, status_bright, repeating);
            }
        }

        repeating = (repeating+1)%DITHER_MAX_LOOP;
        return;
    }

    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
    {
        // Each LED's dither runs at its own offset, so a dimmed pad doesn't pulse in step
        uint8_t phase = (uint8_t)((repeating + (uint32_t)i * 5u) % DITHER_MAX_LOOP);
        _anm_utility_led(in, out, i, brightness, phase);
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
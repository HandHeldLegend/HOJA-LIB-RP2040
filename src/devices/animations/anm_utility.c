#include "devices/animations/anm_utility.h"
#include "devices/animations/anm_handler.h"

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

// Brightness is 0 to 1275 range
void anm_utility_process(rgb_s *leds, uint16_t brightness)
{
    static uint8_t repeating = 0;
    brightness = (brightness>DITHER_BRIGHT_MAX) ? DITHER_BRIGHT_MAX : brightness;
    uint16_t    dither_idx = brightness % DITHER_TABLE_SIZE;
    uint8_t     dither_amt = (_dithering_table[dither_idx] >> repeating) & 0x1;

    uint8_t brightness_u8 = brightness/6;

    for(int i = 0; i < RGB_DRIVER_COUNT; i++)
    {
        // First apply gamma
        leds[i].r = gamma8[leds[i].r];
        leds[i].g = gamma8[leds[i].g];
        leds[i].b = gamma8[leds[i].b];
        
        // Apply brightness u8
        leds[i].r = (leds[i].r * brightness_u8 + 127) >> 8;
        leds[i].g = (leds[i].g * brightness_u8 + 127) >> 8;
        leds[i].b = (leds[i].b * brightness_u8 + 127) >> 8;

        // Apply dither
        leds[i].r += dither_amt;
        leds[i].g += dither_amt;
        leds[i].b += dither_amt;
    }

    repeating = (repeating+1)%DITHER_MAX_LOOP;
}

// Blend two RGB values together with a float (0.0 to 1.0)
uint32_t anm_utility_blend(rgb_s *from, rgb_s *target, float blend)
{
    float or = (float)from->r;
    float og = (float)from->g;
    float ob = (float)from->b;
    float nr = (float)target->r;
    float ng = (float)target->g;
    float nb = (float)target->b;
    float outr = or +((nr - or) * blend);
    float outg = og + ((ng - og) * blend);
    float outb = ob + ((nb - ob) * blend);
    uint8_t outr_int = (uint8_t)outr;
    uint8_t outg_int = (uint8_t)outg;
    uint8_t outb_int = (uint8_t)outb;
    rgb_s col = {
        .r = outr_int,
        .g = outg_int,
        .b = outb_int};
    return col.color;
}

// Convert milliseconds to frames according to our refresh interval
uint32_t anm_utility_ms_to_frames(uint32_t time_ms)
{
    int calc = (1000 * time_ms) / INTERVAL_US_PER_FRAME;

    return (uint32_t) calc;
}
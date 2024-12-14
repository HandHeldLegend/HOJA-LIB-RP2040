#include "devices/animations/anm_utility.h"
#include "devices/animations/anm_handler.h"

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
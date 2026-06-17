#ifndef RGB_MODES_H
#define RGB_MODES_H

#include <stdint.h>

// RGB animation mode IDs (stored in rgb_config->rgb_mode).
// Keep in sync with the config app mode list.
typedef enum
{
    RGB_ANIM_AUTHENTIC = 0, // Era-matched colors; follows input remaps
    RGB_ANIM_NONE      = 1, // Static user-configured colors
    RGB_ANIM_RAINBOW   = 2,
    RGB_ANIM_REACT     = 3,
    RGB_ANIM_FAIRY     = 4,
    RGB_ANIM_BREATHE   = 5, // Reserved; not wired yet
    RGB_ANIM_IDLE      = 6,
    RGB_MODE_COUNT,
} rgb_anim_t;

#endif

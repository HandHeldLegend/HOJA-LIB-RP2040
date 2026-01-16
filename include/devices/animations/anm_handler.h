#ifndef ANM_HANDLER_H
#define ANM_HANDLER_H
#include <stdint.h>
#include "utilities/callback.h"

typedef enum
{
    RGB_ANIM_NONE,      // Show stored colors static
    RGB_ANIM_RAINBOW,   // Fade through the RGB rainbow
    RGB_ANIM_REACT,     // React to user input
    RGB_ANIM_FAIRY,     // Play a random animation between user preset colors
    RGB_ANIM_BREATHE,   // Looping animation using stored colors
    RGB_ANIM_IDLE,      // Idle animation where no LEDs are shown
    RGB_ANIM_EXTERNAL,  // Control rgb externally
} rgb_anim_t;

typedef enum
{
    RGB_OVERRIDE_FLASH,
    RGB_OVERRIDE_INDICATE,
    RGB_OVERRIDE_SHUTDOWN,
} rgb_override_t;

void anm_handler_shutdown(callback_t cb);
void anm_set_idle_enable(bool enable);
void anm_set_brightness(uint16_t brightness);
void anm_handler_setup_mode(uint8_t rgb_mode, uint16_t brightness, uint32_t animation_time_ms);
void anm_handler_tick();

#endif
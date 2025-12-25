#include "devices/animations/anm_idle.h"
#include "devices/animations/anm_utility.h"
#include "devices/rgb.h"
#include "board_config.h"
#include "utilities/settings.h"
#include "devices/battery.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

// Timing Constants
#define IDLE_TEST_CYCLE_MS      2000  // Change "status" every 2 seconds
#define IDLE_FADE_DURATION_MS   800   // Smooth fade over 0.8 seconds

// Animation State
static rgb_s _idle_start_color   = {0};
static rgb_s _idle_target_color  = {0};
static rgb_s _idle_current_color = {0};

static uint32_t _idle_blend_val      = RGB_FADE_FIXED_MULT; //
static uint32_t _idle_blend_step     = 0;
static uint32_t _test_frame_counter  = 0;
 
// Helper to pick test colors
static rgb_s _get_status_color() {
    static uint8_t  _idle_status_state   = 0;
    static battery_status_s batstat = {0};
    battery_get_status(&batstat);

    if(batstat.charging_done)
    {
        _idle_status_state = 2;
    }
    else if(batstat.charging)
    {
        _idle_status_state = 1;
    }
    else _idle_status_state = 0;

    if(rgb_config->rgb_idle_glow == 1)
    {
        _idle_status_state = 3;
    }

    rgb_s color;

    if (_idle_status_state == 1) color = COLOR_ORANGE; // Charging
    else if (_idle_status_state == 2) color = COLOR_GREEN;  // Done
    else if (_idle_status_state == 0) color = COLOR_CYAN;                 // Not Charging
    else if (_idle_status_state == 3) color = COLOR_BLACK; // Disabled

    color.r >>= 1;
    color.g >>= 1;
    color.b >>= 1;
    return color;
}

bool anm_idle_get_state(rgb_s *output)
{
    // Initialize everything to off/black first
    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++) {
        output[i].color = 0x00;
    }

    // Set initial state
    _idle_current_color  = _get_status_color();
    _idle_target_color   = _idle_current_color;
    _idle_blend_val      = RGB_FADE_FIXED_MULT;

    // Pre-calculate step based on refresh rate
    uint32_t fade_frames = anm_utility_ms_to_frames(IDLE_FADE_DURATION_MS);
    _idle_blend_step     = (fade_frames > 0) ? (RGB_FADE_FIXED_MULT / fade_frames) : RGB_FADE_FIXED_MULT;

    return true;
}

bool anm_idle_handler(rgb_s* output)
{
    bool needs_update = false;
    _test_frame_counter++;

    // Initialize everything to off/black first
    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++) {
        output[i].color = 0x00;
    }

    // 1. TEST SEQUENCE LOGIC: Cycle status every 5 seconds
    if (_test_frame_counter >= anm_utility_ms_to_frames(IDLE_TEST_CYCLE_MS)) {
        _test_frame_counter = 0;

        // Trigger a new fade transition
        _idle_start_color  = _idle_current_color;
        _idle_target_color = _get_status_color();
        _idle_blend_val    = 0; 
    }

    // 2. FADE ENGINE: Transition current color toward target
    if (_idle_blend_val < RGB_FADE_FIXED_MULT) {
        _idle_blend_val += _idle_blend_step;
        
        if (_idle_blend_val > RGB_FADE_FIXED_MULT) {
            _idle_blend_val = RGB_FADE_FIXED_MULT;
        }

        // Use the utility blend function to calculate the intermediate color
        _idle_current_color.color = anm_utility_blend(&_idle_start_color, &_idle_target_color, _idle_blend_val);
        needs_update = true;
    }

    // 3. RENDER: Apply current color to the notification group
    if(rgb_config->rgb_idle_glow != 1)
    {
        #if defined(HOJA_RGB_NOTIF_GROUP_IDX)
        for(int j = 0; j < RGB_MAX_LEDS_PER_GROUP; j++) {
            int index_out = rgb_led_groups[HOJA_RGB_NOTIF_GROUP_IDX][j];
            if(index_out >= 0) {
                output[index_out].color = _idle_current_color.color;
            }
        }
        #endif
    }
    

    return true;
}
#endif
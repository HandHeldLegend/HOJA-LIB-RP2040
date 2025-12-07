#include "devices/rgb.h"

#include <math.h>

#include "utilities/interval.h"
#include "utilities/settings.h"

#include "board_config.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER==RGB_DRIVER_HAL)
    #include "hal/rgb_hal.h"
#endif

#include "devices/animations/anm_handler.h"
#include "devices/animations/anm_utility.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)
int8_t rgb_led_groups[RGB_MAX_GROUPS][RGB_MAX_LEDS_PER_GROUP] = HOJA_RGB_GROUPINGS;
rgb_s  rgb_colors_safe[RGB_MAX_GROUPS] = {0};

// Perform a fade animation to black, then call our callback
void rgb_deinit(callback_t cb)
{
    anm_handler_shutdown(cb);
}

const rgb_s _rainbow[] = COLORS_RAINBOW; 

// Function to compute an exponential ramp
float _exponentialRamp(float input) {
    if (input < 0.0f) input = 0.0f;
    if (input > 1.0f) input = 1.0f;

    // Exponential curve factor, adjust for steepness
    const float exponent = 1.5f; // Higher values increase steepness
    return powf(input, exponent);
}
#endif

void rgb_set_idle(bool enable)
{
    #if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)
    anm_set_idle_enable(enable);
    #endif
}

void rgb_init(int mode, int brightness)
{
    #if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)
    uint8_t set_mode = 0;
    uint16_t set_brightness = 0;
    uint16_t loaded_brightness = 0;
    uint16_t set_speed = 650;

    if(rgb_config->rgb_speed < 300)
    {
        rgb_config->rgb_speed = 300;
    }
    else if (rgb_config->rgb_speed > 5000)
    {
        rgb_config->rgb_speed = 5000;
    }

    set_speed = rgb_config->rgb_speed;

    if(mode < 0)
    {
        set_mode = rgb_config->rgb_mode;
    }
    else {
        set_mode = mode;
    }

    if(brightness < 0)
    {
        loaded_brightness = rgb_config->rgb_brightness;
    }
    else {
        loaded_brightness = brightness;
    }

    // Handle defaulting if we don't have colors
    if(rgb_config->rgb_config_version != CFG_BLOCK_RGB_VERSION)
    {
        rgb_config->rgb_config_version = CFG_BLOCK_RGB_VERSION;

        rgb_config->rgb_brightness = 2800;
        loaded_brightness = rgb_config->rgb_brightness;

        rgb_config->rgb_mode = 0;
        set_mode = 0;

        uint8_t col = 0;
        const uint8_t color_count = 7;
        for(int i = 0; i < RGB_MAX_GROUPS; i++)
        {
            rgb_config->rgb_colors[i] = anm_utility_pack_local_color(_rainbow[col]);
            col = (col+1) % color_count;
        }

        #if defined(HOJA_RGB_GROUP_DEFAULTS)
        rgb_s default_colors[HOJA_RGB_GROUPS_NUM] = HOJA_RGB_GROUP_DEFAULTS;
        for(int i = 0; i < HOJA_RGB_GROUPS_NUM; i++)
        {
            rgb_s col_tmp = default_colors[i];
            rgb_config->rgb_colors[i] = anm_utility_pack_local_color(col_tmp);
        }   
        #endif

        rgb_config->rgb_speed = 650;
        set_speed = rgb_config->rgb_speed;
    }

    // DEBUG
    //set_mode = 2;

    // Load local colors
    for(int i = 0; i < RGB_MAX_GROUPS; i++)
    {
        rgb_s col_tmp = anm_utility_unpack_local_color(rgb_config->rgb_colors[i]);
        anm_utility_apply_color_safety(&col_tmp);
        rgb_colors_safe[i] = col_tmp;
    }

    // Clamp our stored brightness
    float cbright = (float) loaded_brightness;
    cbright = cbright > 4096 ? 4096 : cbright;

    float cbrightratio = cbright/4096.0f;
    float nratio = _exponentialRamp(cbrightratio);
    set_brightness = (uint16_t) (1530.0f * nratio);
    
    static bool _rgb_ll_init = false;
    if(!_rgb_ll_init)
    {
        RGB_DRIVER_INIT();
        _rgb_ll_init = true;
    }

    anm_handler_setup_mode(set_mode, set_brightness, set_speed);

    #endif
}

// One tick of RGB logic
// only performs actions if necessary
void rgb_task(uint64_t timestamp)
{
    #if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)
    static interval_s interval = {0};

    if (interval_run(timestamp, RGB_TASK_INTERVAL, &interval))
    {
        anm_handler_tick();
    }
    #endif
}

void rgb_config_command(rgb_cmd_t cmd)
{
    switch(cmd)
    {
        default:
        break;

        case RGB_CMD_REFRESH:
            rgb_init(-1, -1);
        break;
    }
}

#include "rgb.h"

#if (HOJA_CAPABILITY_RGB == 1)
// 21 steps is about 0.35 seconds
// Formula is time period us / 16666us (60hz)
// #define RGB_FADE_STEPS 21
#define RGB_DEFAULT_FADE_STEPS 21 // 21
// 60hz
#define RGB_TASK_INTERVAL 16666

uint8_t _rgb_anim_steps = 0;
uint8_t _rgb_anim_steps_new = 0;
bool _rgb_out_dirty = false;

uint32_t _rgb_flash_color = 0x00;
rgb_s _rgb_next[HOJA_RGB_COUNT] = {0};
rgb_s _rgb_current[HOJA_RGB_COUNT] = {0};
rgb_s _rgb_last[HOJA_RGB_COUNT] = {0};
rgb_preset_t *_rgb_preset;
rgb_preset_t _rgb_preset_loaded;
uint8_t _rgb_brightness = 100;
bool _rgb_task_constant = false;

bool _rgb_mode_setup = false;

uint8_t _rgb_rainbow_step = 5;

hsv_s _rainbow_hsv = {.hue = 0, .saturation = 255, .value = 255};
uint8_t _rainbowoffset_hsv[HOJA_RGB_COUNT] = {0};

uint8_t _cycle_idx = 0;
uint8_t _cycle_offset_idx[HOJA_RGB_COUNT] = {0};

rgb_s _rainbow_next = {0};

const int8_t _rgb_group_rs[] = HOJA_RGB_GROUP_RS;
const uint8_t _rs_size = sizeof(_rgb_group_rs) / sizeof(_rgb_group_rs[0]);

const int8_t _rgb_group_ls[] = HOJA_RGB_GROUP_LS;
const uint8_t _ls_size = sizeof(_rgb_group_ls) / sizeof(_rgb_group_ls[0]);

const int8_t _rgb_group_dpad[] = HOJA_RGB_GROUP_DPAD;
const uint8_t _dpad_size = sizeof(_rgb_group_dpad) / sizeof(_rgb_group_dpad[0]);

const int8_t _rgb_group_minus[] = HOJA_RGB_GROUP_MINUS;
const uint8_t _minus_size = sizeof(_rgb_group_minus) / sizeof(_rgb_group_minus[0]);

const int8_t _rgb_group_capture[] = HOJA_RGB_GROUP_CAPTURE;
const uint8_t _capture_size = sizeof(_rgb_group_capture) / sizeof(_rgb_group_capture[0]);

const int8_t _rgb_group_home[] = HOJA_RGB_GROUP_HOME;
const uint8_t _home_size = sizeof(_rgb_group_home) / sizeof(_rgb_group_home[0]);

const int8_t _rgb_group_plus[] = HOJA_RGB_GROUP_PLUS;
const uint8_t _plus_size = sizeof(_rgb_group_plus) / sizeof(_rgb_group_plus[0]);

const int8_t _rgb_group_y[] = HOJA_RGB_GROUP_Y;
const uint8_t _y_size = sizeof(_rgb_group_y) / sizeof(_rgb_group_y[0]);

const int8_t _rgb_group_x[] = HOJA_RGB_GROUP_X;
const uint8_t _x_size = sizeof(_rgb_group_x) / sizeof(_rgb_group_x[0]);

const int8_t _rgb_group_a[] = HOJA_RGB_GROUP_A;
const uint8_t _a_size = sizeof(_rgb_group_a) / sizeof(_rgb_group_a[0]);

const int8_t _rgb_group_b[] = HOJA_RGB_GROUP_B;
const uint8_t _b_size = sizeof(_rgb_group_b) / sizeof(_rgb_group_b[0]);

rgb_mode_t _rgb_mode = 0;

rgb_anim_cb _rgb_anim_cb = NULL;
bool _rgb_anim_override = false;
rgb_override_anim_cb _rgb_anim_override_cb = NULL;

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
void _rgb_convert_hue_saturation(hsv_s *hsv, rgb_s *out)
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

void _rgb_set_color_brightness(rgb_s *color)
{
    if (!_rgb_brightness)
    {
        color->color = 0x00;
    }
    else if (_rgb_brightness == 100)
    {
        return;
    }
    else
    {

        // Calculate relative brightness first
        uint16_t rel = color->r + color->g + color->b;
        float relfl = rel / 255.0f;

        // Get our new apparent brightness
        float r = (float)_rgb_brightness / 100.0f;

        // Don't process adjustment if the color is already
        // less bright than our adjustment
        if (relfl <= r)
            return;

        r = r / relfl;

        color->r = (float)color->r * r;
        color->g = (float)color->g * r;
        color->b = (float)color->b * r;
    }
}

void _rgb_normalize_output_power(rgb_s *color)
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

void _rgb_update_all()
{

    for (uint8_t i = 0; i < HOJA_RGB_COUNT; i++)
    {
        pio_sm_put_blocking(RGB_PIO, RGB_SM, _rgb_current[i].color);
    }
}

uint32_t _rgb_blend(rgb_s *original, rgb_s *new, float blend)
{
    float or = (float)original->r;
    float og = (float)original->g;
    float ob = (float)original->b;
    float nr = (float)new->r;
    float ng = (float)new->g;
    float nb = (float)new->b;
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

// Returns true once the animation process is completed
bool _rgb_animate_step()
{
    static uint8_t steps = 0;
    float blend_step = 1.0f / _rgb_anim_steps;
    static bool done = true;

    if (_rgb_out_dirty)
    {
        memcpy(_rgb_last, _rgb_current, sizeof(_rgb_last));
        steps = 0;
        _rgb_out_dirty = false;
        done = false;
    }

    if (steps <= _rgb_anim_steps)
    {
        float blender = blend_step * (float)steps;
        // Blend between old and next colors appropriately
        for (uint8_t i = 0; i < HOJA_RGB_COUNT; i++)
        {
            _rgb_current[i].color = _rgb_blend(&_rgb_last[i], &_rgb_next[i], blender);
        }
        steps++;
        _rgb_update_all();
    }
    else if (!done)
    {
        memcpy(_rgb_current, _rgb_next, sizeof(_rgb_next));
        _rgb_update_all();
        done = true;
        return true;
    }

    return false;
}

void _rgb_set_sequential(const uint8_t *leds, uint8_t len, rgb_s *colors, uint32_t color)
{
    for (uint8_t i = 0; i < len; i++)
    {
        if (leds[i] == -1)
            continue;

        colors[leds[i]].color = color;
        _rgb_normalize_output_power(&colors[leds[i]]);
    }
}
#endif

void _rgb_set_brightness(uint8_t brightness)
{
#if (HOJA_CAPABILITY_RGB == 1)
    _rgb_brightness = (brightness > 100) ? 100 : brightness;
#endif
}

// Enable the RGB transition to the next color
void _rgb_set_dirty()
{
#if (HOJA_CAPABILITY_RGB == 1)
    _rgb_out_dirty = true;
#endif
}

// Set all RGBs to one color
void _rgb_set_all(uint32_t color)
{
#if (HOJA_CAPABILITY_RGB == 1)
    for (uint8_t i = 0; i < HOJA_RGB_COUNT; i++)
    {
        rgb_s col = {.color = color};
        _rgb_set_color_brightness(&col);
        _rgb_next[i].color = col.color;
        _rgb_normalize_output_power(&_rgb_next[i]);
    }
#endif
}

void _rgb_preset_reload()
{
#if (HOJA_CAPABILITY_RGB == 1)
    memcpy(&_rgb_preset_loaded, _rgb_preset, sizeof(rgb_preset_t));
    uint32_t *p = (uint32_t *)&_rgb_preset_loaded;

    for (uint8_t i = 0; i < RGB_GROUP_MAX; i++)
    {
        _rgb_set_color_brightness((rgb_s *)&p[i]);
        rgb_set_group(i, p[i], false);
    }
#endif
}

void _rgb_load_preset(rgb_preset_t *preset)
{
#if (HOJA_CAPABILITY_RGB == 1)
    _rgb_preset = preset;
    _rgb_preset_reload();
#endif
}

void _rgbanim_preset_do()
{
    if (!_rgb_mode_setup)
    {
        _rgb_load_preset((rgb_preset_t *)&global_loaded_settings.rgb_colors[0]);
        _rgb_set_dirty();
        _rgb_mode_setup = true;
    }
}

void rgb_set_group(rgb_group_t group, uint32_t color, bool instant)
{
#if (HOJA_CAPABILITY_RGB == 1)

    const uint8_t *_rgb_group = _rgb_group_rs;
    uint8_t size = 1;

    switch (group)
    {
    default:
        break;

    case RGB_GROUP_RS:
        _rgb_group = (_rgb_group_rs);
        size = _rs_size;
        break;

    case RGB_GROUP_LS:
        _rgb_group = _rgb_group_ls;
        size = _ls_size;
        break;

    case RGB_GROUP_DPAD:
        _rgb_group = _rgb_group_dpad;
        size = _dpad_size;
        break;

    case RGB_GROUP_MINUS:
        _rgb_group = _rgb_group_minus;
        size = _minus_size;
        break;

    case RGB_GROUP_CAPTURE:
        _rgb_group = _rgb_group_capture;
        size = _capture_size;
        break;

    case RGB_GROUP_HOME:
        _rgb_group = _rgb_group_home;
        size = _home_size;
        break;

    case RGB_GROUP_PLUS:
        _rgb_group = _rgb_group_plus;
        size = _plus_size;
        break;

    case RGB_GROUP_Y:
        _rgb_group = _rgb_group_y;
        size = _y_size;
        break;

    case RGB_GROUP_X:
        _rgb_group = _rgb_group_x;
        size = _x_size;
        break;

    case RGB_GROUP_A:
        _rgb_group = _rgb_group_a;
        size = _a_size;
        break;

    case RGB_GROUP_B:
        _rgb_group = _rgb_group_b;
        size = _b_size;
        break;
    }

    if(instant)
    {
        _rgb_set_sequential(_rgb_group, size, _rgb_last, color);
        _rgb_set_sequential(_rgb_group, size, _rgb_next, color);
        _rgb_set_sequential(_rgb_group, size, _rgb_current, color);
    }
    else
    {
         _rgb_set_sequential(_rgb_group, size, _rgb_next, color);
         _rgb_set_dirty();
    }
#endif
}

void rgb_update_speed(uint8_t speed)
{
    if (speed != global_loaded_settings.rgb_step_speed)
    {
        if (global_loaded_settings.rgb_step_speed == 0)
        {
            global_loaded_settings.rgb_step_speed = 5;
        }
        global_loaded_settings.rgb_step_speed = speed;
    }

    _rgb_rainbow_step = global_loaded_settings.rgb_step_speed;

    uint16_t steps16 = 0;

    switch (_rgb_mode)
    {
    default:
    case RGB_MODE_PRESET:
        _rgb_anim_steps = RGB_DEFAULT_FADE_STEPS;
        _rgb_anim_steps_new = RGB_DEFAULT_FADE_STEPS;
        break;

    case RGB_MODE_RAINBOW:
        _rgb_anim_steps = RGB_DEFAULT_FADE_STEPS;
        _rgb_anim_steps_new = RGB_DEFAULT_FADE_STEPS;
        break;

    case RGB_MODE_RAINBOWOFFSET:
        _rgb_anim_steps = RGB_DEFAULT_FADE_STEPS;
        _rgb_anim_steps_new = RGB_DEFAULT_FADE_STEPS;
        break;

    case RGB_MODE_FLASH:
        _rgb_anim_steps = RGB_DEFAULT_FADE_STEPS;
        _rgb_anim_steps_new = RGB_DEFAULT_FADE_STEPS;
        break;

    case RGB_MODE_CYCLE:
        _rgb_anim_steps = RGB_DEFAULT_FADE_STEPS;
        steps16 = (_rgb_rainbow_step * 5);
        if (steps16 > 250)
            steps16 = 250;
        _rgb_anim_steps_new = 255 - (uint8_t)steps16;
        break;

    case RGB_MODE_CYCLEOFFSET:
        _rgb_anim_steps = RGB_DEFAULT_FADE_STEPS;
        steps16 = (_rgb_rainbow_step * 5);
        if (steps16 > 250)
            steps16 = 250;
        _rgb_anim_steps_new = 255 - (uint8_t)steps16;
        break;
    }
}

void _rgbanim_rainbow_do()
{
    _rainbow_hsv.hue = (_rainbow_hsv.hue + _rgb_rainbow_step) % 256;
    _rgb_convert_hue_saturation(&_rainbow_hsv, &_rainbow_next);
    _rgb_set_all(_rainbow_next.color);
    _rgb_set_dirty();
}

void _rgbanim_rainbowoffset_do()
{
    if (!_rgb_mode_setup)
    {
        for (uint8_t i = 0; i < HOJA_RGB_COUNT; i++)
        {
            _rainbowoffset_hsv[i] = get_rand_32() % 255;
        }
        _rgb_mode_setup = true;
    }

    for (uint8_t i = 0; i < HOJA_RGB_COUNT; i++)
    {
        _rainbowoffset_hsv[i] = (_rainbowoffset_hsv[i] + _rgb_rainbow_step) % 255;
        hsv_s hsv = {.hue = _rainbowoffset_hsv[i], .saturation = 255, .value = 255};
        rgb_s rgb = {0};
        _rgb_convert_hue_saturation(&hsv, &rgb);
        _rgb_set_color_brightness(&rgb);
        _rgb_next[i].color = rgb.color;
        _rgb_normalize_output_power(&_rgb_next[i]);
    }
    _rgb_set_dirty();
}

void _rgbanim_cycle_do()
{
    if (!_rgb_mode_setup)
    {
        _cycle_idx = 0;
        _rgb_mode_setup = true;
    }
    else
    {
        _rgb_anim_steps = _rgb_anim_steps_new;
    }
    _rgb_set_all(global_loaded_settings.rainbow_colors[_cycle_idx]);
    _cycle_idx = (_cycle_idx + 1) % 6;
    _rgb_set_dirty();
}

void _rgbanim_cycleoffset_do()
{
    if (!_rgb_mode_setup)
    {
        _cycle_idx = 0;

        _rgb_mode_setup = true;

        for (uint8_t i = 0; i < HOJA_RGB_COUNT; i++)
        {
            _cycle_offset_idx[i] = get_rand_32() % 6;
        }
    }
    else
    {
        _rgb_anim_steps = _rgb_anim_steps_new;
    }

    for (uint8_t i = 0; i < HOJA_RGB_COUNT; i++)
    {
        _rgb_next[i].color = global_loaded_settings.rainbow_colors[_cycle_offset_idx[i]];
        _cycle_offset_idx[i] = get_rand_32() % 6;
    }
    _rgb_set_dirty();
}

// Flash the RGBs a color
void rgb_flash(uint32_t color)
{
    _rgb_flash_color = color;
    rgb_init(RGB_MODE_FLASH, 100);
}

void _rgbanim_flash_do()
{
    static bool dir = false;

    if (dir)
    {
        _rgb_set_all(0x000000);
        _rgb_set_dirty();
        dir = false;
    }
    else
    {
        if (!_rgb_flash_color)
        {
            _rgb_flash_color = COLOR_YELLOW.color;
        }
        _rgb_set_all(_rgb_flash_color);
        _rgb_set_dirty();
        dir = true;
    }
}

uint32_t _rgb_indicate_color = 0x00;
bool _rgb_indicate_override_do()
{
    static uint8_t _rgb_indicate_steps = 0;
    _rgb_set_all(_rgb_indicate_color);
    _rgb_set_dirty();

    if (_rgb_indicate_steps++ == 1)
    {
        _rgb_indicate_steps = 0;
        return true;
    }
    return false;
}

#define INTENSITY_DEADZONE 50
#define INTENSITY_CAP 2048
float _rgb_stick_get_intensity(int x, int y)
{
    float dx = fabs((float) x - 2048);
    float dy = fabs((float) y - 2048);
    
    // Calculate the distance from the center of the stick
    float dist = sqrtf((dx * dx) + (dy * dy));

    float intensity = dist/2048.0f;
    intensity = (intensity > 1.0f) ? 1.0f : intensity;
    intensity = (intensity < 0.0f) ? 0.0f : intensity;
    return intensity;
}

void _rgbanim_reactive_do()
{
    static button_data_s current = {0};

    button_data_s *data = hoja_get_raw_button_data();
    a_data_s *analog = hoja_get_buffered_analog_data();
    bool _set = false;

    // Get distance of left and right sticks
    float _ls_intensity = _rgb_stick_get_intensity(analog->lx, analog->ly);
    float _rs_intensity = _rgb_stick_get_intensity(analog->rx, analog->ry);

    if(!_rgb_mode_setup )
    {
        _rgb_mode_setup = true;
        _rgb_task_constant = true;
    }

    if(data->buttons_all != current.buttons_all)
    {   
        if(data->button_a)
        rgb_set_group(RGB_GROUP_A, global_loaded_settings.rgb_colors[RGB_GROUP_A], true);

        if(data->button_b)
        rgb_set_group(RGB_GROUP_B, global_loaded_settings.rgb_colors[RGB_GROUP_B], true);

        if(data->button_x)
        rgb_set_group(RGB_GROUP_X, global_loaded_settings.rgb_colors[RGB_GROUP_X], true);

        if(data->button_y)
        rgb_set_group(RGB_GROUP_Y, global_loaded_settings.rgb_colors[RGB_GROUP_Y], true);

        if(data->button_plus)
        rgb_set_group(RGB_GROUP_PLUS, global_loaded_settings.rgb_colors[RGB_GROUP_PLUS], true);

        if(data->button_minus)
        rgb_set_group(RGB_GROUP_MINUS, global_loaded_settings.rgb_colors[RGB_GROUP_MINUS], true);

        if( (data->dpad_up) | (data->dpad_down) | (data->dpad_left) | (data->dpad_right))
        rgb_set_group(RGB_GROUP_DPAD, global_loaded_settings.rgb_colors[RGB_GROUP_DPAD], true);

        current.buttons_all = data->buttons_all;
        _set = true;
    }

    if(data->buttons_system != current.buttons_system)
    {


        if(data->button_home != current.button_home)
        rgb_set_group(RGB_GROUP_HOME, global_loaded_settings.rgb_colors[RGB_GROUP_HOME], true);

        if(data->button_capture != current.button_capture)
        rgb_set_group(RGB_GROUP_CAPTURE, global_loaded_settings.rgb_colors[RGB_GROUP_CAPTURE], true);

        current.buttons_system = data->buttons_system;
        _set = true;
    }

    if(_set)
    {
        rgb_set_group(RGB_GROUP_A, 0, false);
        rgb_set_group(RGB_GROUP_B, 0, false);
        rgb_set_group(RGB_GROUP_X, 0, false);
        rgb_set_group(RGB_GROUP_Y, 0, false);

        rgb_set_group(RGB_GROUP_PLUS, 0, false);
        rgb_set_group(RGB_GROUP_MINUS, 0, false);
        rgb_set_group(RGB_GROUP_DPAD, 0, false);
        rgb_set_group(RGB_GROUP_HOME, 0, false);
        rgb_set_group(RGB_GROUP_CAPTURE, 0, false);
        _rgb_set_dirty();
    }

    rgb_s _black = {.color = 0x000000};

    // Set left and right stick colors
    rgb_s _ls_setcolor = {.color = global_loaded_settings.rgb_colors[RGB_GROUP_LS]};
    rgb_s _ls_color = {.color = _rgb_blend(&_black, &_ls_setcolor, _ls_intensity)};
    rgb_set_group(RGB_GROUP_LS, _ls_color.color, true);

    rgb_s _rs_setcolor = {.color = global_loaded_settings.rgb_colors[RGB_GROUP_RS]};
    rgb_s _rs_color = {.color = _rgb_blend(&_black, &_rs_setcolor, _rs_intensity)};
    rgb_set_group(RGB_GROUP_RS, _rs_color.color, true);
    _rgb_update_all();
}

void rgb_indicate(uint32_t color)
{
    _rgb_indicate_color = color;
    _rgb_anim_override_cb = _rgb_indicate_override_do;
    _rgb_anim_override = true;
}

bool _rgb_shutdown_start_override_do()
{
    util_battery_enable_ship_mode();
    return true;
}

void rgb_shutdown_start()
{
    _rgb_anim_cb = NULL;

    _rgb_anim_steps = 10;
    _rgb_anim_steps_new = RGB_DEFAULT_FADE_STEPS;

    _rgb_anim_override_cb = _rgb_shutdown_start_override_do;
    _rgb_anim_override = true;

    _rgb_set_all(0);
    _rgb_set_dirty();
}

bool _rgb_pio_done = false;
void rgb_init(rgb_mode_t mode, int brightness)
{
#if (HOJA_CAPABILITY_RGB == 1)
    if (!_rgb_pio_done)
    {
        uint offset = pio_add_program(RGB_PIO, &ws2812_program);
        ws2812_program_init(RGB_PIO, RGB_SM, offset, HOJA_RGB_PIN, HOJA_RGBW_EN);
        _rgb_pio_done = true;
    }

    _rgb_mode = mode;
    _rgb_mode_setup = false;
    _rgb_task_constant = false;

    switch (_rgb_mode)
    {
    default:
    case RGB_MODE_PRESET:
        _rgb_anim_cb = _rgbanim_preset_do;
        break;

    case RGB_MODE_RAINBOW:
        _rgb_anim_cb = _rgbanim_rainbow_do;
        break;

    case RGB_MODE_RAINBOWOFFSET:
        _rgb_anim_cb = _rgbanim_rainbowoffset_do;
        break;

    case RGB_MODE_FLASH:
        _rgb_anim_cb = _rgbanim_flash_do;
        break;

    case RGB_MODE_CYCLE:
        _rgb_anim_cb = _rgbanim_cycle_do;
        break;

    case RGB_MODE_CYCLEOFFSET:
        _rgb_anim_cb = _rgbanim_cycleoffset_do;
        break;

    case RGB_MODE_REACTIVE:
        _rgb_anim_cb = _rgbanim_reactive_do;
        break;
    }

    rgb_update_speed(global_loaded_settings.rgb_step_speed);

    if (brightness > -1)
    {
        brightness = (brightness > 255) ? 255 : brightness;
        _rgb_set_brightness(brightness);
    }

    _rgb_set_dirty();

#endif
}

// One tick of RGB logic
// only performs actions if necessary
void rgb_task(uint32_t timestamp)
{
#if (HOJA_CAPABILITY_RGB == 1)
    if (interval_run(timestamp, RGB_TASK_INTERVAL))
    {
        bool _done = _rgb_animate_step();

        if (_done || _rgb_task_constant)
        {
            if (_rgb_anim_override)
            {
                if (_rgb_anim_override_cb())
                {
                    _rgb_anim_override = false;
                }
            }
            else if (_rgb_anim_cb != NULL)
            {

                    _rgb_anim_cb();
            }
        }

    }
#endif
}

#include "rgb.h"

#ifdef HOJA_RGB_PIN
// 21 steps is about 0.35 seconds
// Formula is time period us / 16666us (60hz)
#define RGB_FADE_STEPS 21

uint8_t _rgb_anim_steps = 0;
bool _rgb_out_dirty = false;

rgb_s _rgb_next[HOJA_RGB_COUNT]     = {0};
rgb_s _rgb_current[HOJA_RGB_COUNT]  = {0};
rgb_s _rgb_last[HOJA_RGB_COUNT]     = {0};
rgb_preset_t *_rgb_preset;
rgb_preset_t _rgb_preset_loaded;
uint8_t _rgb_brightness = 100;

const uint8_t _rgb_group_rs[] = HOJA_RGB_GROUP_RS;
const uint8_t _rgb_group_ls[] = HOJA_RGB_GROUP_LS;
const uint8_t _rgb_group_dpad[] = HOJA_RGB_GROUP_DPAD;
const uint8_t _rgb_group_minus[] = HOJA_RGB_GROUP_MINUS;
const uint8_t _rgb_group_capture[] = HOJA_RGB_GROUP_CAPTURE;
const uint8_t _rgb_group_home[] = HOJA_RGB_GROUP_HOME;
const uint8_t _rgb_group_plus[] = HOJA_RGB_GROUP_PLUS;
const uint8_t _rgb_group_y[] = HOJA_RGB_GROUP_Y;
const uint8_t _rgb_group_x[] = HOJA_RGB_GROUP_X;
const uint8_t _rgb_group_a[] = HOJA_RGB_GROUP_A;
const uint8_t _rgb_group_b[] = HOJA_RGB_GROUP_B;

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

void _rgb_set_color_brightness(rgb_s *color)
{
    if(!_rgb_brightness)
    {
        color->color = 0x00;
    }
    else if(_rgb_brightness==100)
    {
        return;
    }
    else
    {
        float r = (float) _rgb_brightness/100.0f;
        color->r = (float) color->r * r;
        color->g = (float) color->g * r;
        color->b = (float) color->b * r;
    }
}

void _rgb_normalize_output_power(rgb_s *color)
{
    if(!color->color) return;
    
    float total = (float) (color->r + color->g + color->b);
    
    float rf = (float)color->r / total;
    float gf = (float)color->g / total;
    float bf = (float)color->b / total;

    color->r = (float)color->r * rf;
    color->g = gf* (float)color->g;
    color->b = bf* (float)color->b;
}

void _rgb_update_all()
{
    
    for(uint8_t i = 0; i < HOJA_RGB_COUNT; i++)
    {
        pio_sm_put_blocking(RGB_PIO, RGB_SM, _rgb_current[i].color);
    }
}

uint32_t _rgb_blend(rgb_s *original, rgb_s *new, float blend)
{
    float or = (float) original->r;
    float og = (float) original->g;
    float ob = (float) original->b;
    float nr = (float) new->r;
    float ng = (float) new->g;
    float nb = (float) new->b;
    float outr = or + ((nr-or)*blend);
    float outg = og + ((ng-og)*blend);
    float outb = ob + ((nb-ob)*blend);
    rgb_s col = {
        .r = (uint8_t) outr,
        .g = (uint8_t) outg,
        .b = (uint8_t) outb
    };
    return col.color;
}

// Slowly fades out all LEDs evenly
void _rgb_fade_step()
{
    
}

void _rgb_animate_step()
{
    static uint8_t steps = RGB_FADE_STEPS;
    const float blend_step = 1.0f/RGB_FADE_STEPS;
    bool done = true;

    if (_rgb_out_dirty)
    {
        memcpy(_rgb_last, _rgb_current, sizeof(_rgb_last));
        steps = 0;
        _rgb_out_dirty = false;
        done = false;
    }

    if (steps <= RGB_FADE_STEPS)
    {
        float blender = blend_step * (float) steps;
        // Blend between old and next colors appropriately
        for(uint8_t i = 0; i < HOJA_RGB_COUNT; i++)
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
    }
}

void _rgb_set_sequential(const uint8_t *leds, uint8_t len, rgb_s *colors, uint32_t color)
{
    for(uint8_t i = 0; i < len; i++)
    {
        colors[leds[i]].color = color;
        _rgb_normalize_output_power(&colors[leds[i]]);
    }
}
#endif

void rgb_set_brightness(uint8_t brightness)
{
    _rgb_brightness = (brightness > 100) ? 100 : brightness;
}

// Enable the RGB transition to the next color
void rgb_set_dirty()
{
    #ifdef HOJA_RGB_PIN
    _rgb_out_dirty = true;
    #endif
}

void rgb_set_instant()
{
    #ifdef HOJA_RGB_PIN
    memcpy(_rgb_current, _rgb_next, sizeof(rgb_s)*HOJA_RGB_COUNT);
    for(uint8_t i = 0; i < HOJA_RGB_COUNT; i++)
    {
        _rgb_normalize_output_power(&_rgb_current[i]);
    }
    _rgb_update_all();
    memcpy(_rgb_last, _rgb_current, sizeof(rgb_s)*HOJA_RGB_COUNT);
    #endif
}

// Set all RGBs to one color
void rgb_set_all(uint32_t color)
{
    #ifdef HOJA_RGB_PIN
    for(uint8_t i = 0; i < HOJA_RGB_COUNT; i++)
    {
        rgb_s col = {.color = color};
        _rgb_set_color_brightness(&col);
        _rgb_next[i].color = col.color;
        _rgb_normalize_output_power(&_rgb_next[i]);
    }
    #endif
}

void rgb_preset_reload()
{
    memcpy(&_rgb_preset_loaded, _rgb_preset, sizeof(rgb_preset_t));
    uint32_t *p = (uint32_t *) &_rgb_preset_loaded;

    for(uint8_t i = 0; i < RGB_GROUP_MAX; i++)
    {
        _rgb_set_color_brightness((rgb_s *) &p[i]);
        rgb_set_group(i, p[i]);
    }
}

void rgb_load_preset(rgb_preset_t *preset)
{
    #ifdef HOJA_RGB_PIN
    _rgb_preset = preset;
    rgb_preset_reload();
    
    #endif
}

void rgb_set_group(rgb_group_t group, uint32_t color)
{
    #ifdef HOJA_RGB_PIN
    switch(group)
    {
        default:
        break;

        case RGB_GROUP_RS:
            _rgb_set_sequential(_rgb_group_rs, sizeof(_rgb_group_rs), _rgb_next, color);
            break;

        case RGB_GROUP_LS:
            _rgb_set_sequential(_rgb_group_ls, sizeof(_rgb_group_ls), _rgb_next, color);
            break;

        case RGB_GROUP_DPAD:
            _rgb_set_sequential(_rgb_group_dpad, sizeof(_rgb_group_dpad), _rgb_next, color);
            break;

        case RGB_GROUP_MINUS:
            _rgb_set_sequential(_rgb_group_minus, sizeof(_rgb_group_minus), _rgb_next, color);
            break;

        case RGB_GROUP_CAPTURE:
            _rgb_set_sequential(_rgb_group_capture, sizeof(_rgb_group_capture), _rgb_next, color);
            break;

        case RGB_GROUP_HOME:
            _rgb_set_sequential(_rgb_group_home, sizeof(_rgb_group_home), _rgb_next, color);
            break;

        case RGB_GROUP_PLUS:
            _rgb_set_sequential(_rgb_group_plus, sizeof(_rgb_group_plus), _rgb_next, color);
            break;

        case RGB_GROUP_Y:
            _rgb_set_sequential(_rgb_group_y, sizeof(_rgb_group_y), _rgb_next, color);
            break;

        case RGB_GROUP_X:
            _rgb_set_sequential(_rgb_group_x, sizeof(_rgb_group_x), _rgb_next, color);
            break;

        case RGB_GROUP_A:
            _rgb_set_sequential(_rgb_group_a, sizeof(_rgb_group_a), _rgb_next, color);
            break;

        case RGB_GROUP_B:
            _rgb_set_sequential(_rgb_group_b, sizeof(_rgb_group_b), _rgb_next, color);
            break;
    }
    #endif
}

void rgb_init()
{
    #ifdef HOJA_RGB_PIN
    uint offset = pio_add_program(RGB_PIO, &ws2812_program);
    ws2812_program_init(RGB_PIO, RGB_SM, offset, HOJA_RGB_PIN, HOJA_RGBW_EN);
    #endif
}

const uint32_t _rgb_interval = 16666;

// One tick of RGB logic
// only performs actions if necessary
void rgb_task(uint32_t timestamp)
{
    #ifdef HOJA_RGB_PIN
    if(interval_run(timestamp, _rgb_interval))
    {
        _rgb_animate_step();
    }
    #endif
}

#include "devices/animations/anm_handler.h"
#include "devices/animations/anm_utility.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hoja.h"

#include "board_config.h"

#include "devices/rgb.h"
#include "hal/rgb_hal.h"

#include "transport/transport.h"
#include "cores/cores.h"

#include "utilities/settings.h"

// Primary animation modes
#include "devices/animations/anm_none.h"
#include "devices/animations/anm_authentic.h"
#include "devices/animations/anm_breathe.h"
#include "devices/animations/anm_fairy.h"
#include "devices/animations/anm_rainbow.h"
#include "devices/animations/anm_react.h"
#include "devices/animations/anm_shutdown.h"
#include "devices/animations/anm_idle.h"

// Player LED animation modes
#include "devices/animations/ply_chase.h"
#include "devices/animations/ply_blink.h"
#include "devices/animations/ply_idle.h"
#include "devices/animations/ply_shutdown.h"

// Overrides
#include "devices/animations/or_flash.h"
#include "devices/animations/or_indicate.h"

#include "devices/animations/rgb_modes.h"
#include "utilities/autodetect.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

#define ALL_LEDS_SIZE (sizeof(uint32_t) * RGB_DRIVER_LED_COUNT)

#define FADE_LENGTH_MS 500
#define FADE_LENGTH_FRAMES  ((FADE_LENGTH_MS*1000) / RGB_TASK_INTERVAL)
#define FADE_STEP_FIXED     RGB_FLOAT_TO_FIXED(1.0f / FADE_LENGTH_FRAMES)

typedef bool (*rgb_anim_fn)(rgb_s *);
typedef void (*rgb_anim_stop_fn)(void);

rgb_anim_fn         _ani_main_fn = NULL;
rgb_anim_fn         _ani_fn_get_state = NULL;
rgb_anim_stop_fn    _ani_fn_stop = NULL;
rgb_anim_fn         _ani_override_fn = NULL;
rgb_anim_stop_fn    _ani_override_stop_fn = NULL;

typedef enum 
{
    RGB_OVERRIDE_FLASH,
    RGB_OVERRIDE_INDICATE,
    RGB_OVERRIDE_SHUTDOWN,
} rgb_override_t;

uint32_t _fade_progress  = 0;

rgb_s    _fade_start[RGB_DRIVER_LED_COUNT]   = {0};
rgb_s    _fade_end[RGB_DRIVER_LED_COUNT]     = {0};

rgb_s    _current_ani_leds[RGB_DRIVER_LED_COUNT] = {0};
rgb_s    _adjusted_ani_leds[RGB_DRIVER_LED_COUNT] = {0};


uint16_t _anim_brightness = 0;

// Brightness changes (the wireless cap after Auto picks a wireless mode, the config app) ramp over
// the same time as a color fade instead of jumping
static uint16_t _anim_brightness_target = 0;
static uint16_t _anim_brightness_step = 1;

static void _anm_brightness_set(uint16_t brightness)
{
    static bool first = true;
    _anim_brightness_target = brightness;

    // The first setting applies at once: the LEDs fade in from dark anyway
    if(first)
    {
        first = false;
        _anim_brightness = brightness;
        return;
    }

    const uint32_t delta = (brightness > _anim_brightness) ? (brightness - _anim_brightness) : (_anim_brightness - brightness);
    const uint32_t step = (delta * FADE_STEP_FIXED) / RGB_FADE_FIXED_MULT;
    _anim_brightness_step = (step > 0) ? (uint16_t)step : 1;
}

static void _anm_brightness_tick(void)
{
    if(_anim_brightness < _anim_brightness_target)
    {
        const uint32_t next = _anim_brightness + _anim_brightness_step;
        _anim_brightness = (next > _anim_brightness_target) ? _anim_brightness_target : (uint16_t)next;
    }
    else if(_anim_brightness > _anim_brightness_target)
    {
        _anim_brightness = (_anim_brightness - _anim_brightness_target > _anim_brightness_step)
                         ? (uint16_t)(_anim_brightness - _anim_brightness_step)
                         : _anim_brightness_target;
    }
}
uint32_t _anim_speed = 0;
int _current_mode = -1;

bool _fade = false;
void _ani_queue_fade_start()
{
    memcpy(_fade_start, _current_ani_leds, ALL_LEDS_SIZE);
    _ani_fn_get_state(_fade_end);
    _fade_progress = 0;
    _fade = true;

    static bool first_boot = true;
    if(first_boot)
    {
        first_boot = false;

        // On first boot, fade the notification LED up from black — except in
        // Authentic mode, where get_state already placed the user-config color.
        if(rgb_config->rgb_mode != RGB_ANIM_AUTHENTIC)
        {
            const hoja_rgb_cfg_s *rcfg = &hoja_config_get()->rgb;
            if(rcfg->notification_group_index >= 0)
            {
                for(int i = 0; i < rcfg->notification_group_size && i < RGB_MAX_LEDS_PER_GROUP; i++)
                {
                    int8_t group_idx = rgb_led_groups[rcfg->notification_group_index][i];
                    if(group_idx >= 0)
                        _fade_end[group_idx].color = 0;
                }
            }
        }
    }
}

bool _ani_queue_fade_handler()
{
    // Write the state of the current mode to our fade start
    // This allows our current main mode to keep operating
    // even during a transition

    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
    {
        _current_ani_leds[i].color = anm_utility_blend(&(_fade_start[i]), &(_fade_end[i]), _fade_progress);
    }

    _fade_progress += FADE_STEP_FIXED;
    if(_fade_progress>=RGB_FADE_FIXED_MULT)
    {
        _fade_progress = 0;
        memcpy(_current_ani_leds, _fade_end, ALL_LEDS_SIZE);
        return true;
    }

    return false;
}

bool _rgb_shutting_down = false;

void anm_handler_shutdown(callback_t cb)
{
    _rgb_shutting_down = true;
    anm_shutdown_set_cb(cb);
    _ani_main_fn = anm_shutdown_handler;
    _ani_fn_get_state = anm_shutdown_get_state;
    _ani_queue_fade_start();
}

void anm_handler_setup_mode(uint8_t rgb_mode, uint16_t brightness, uint32_t animation_time_ms)
{
    if(_rgb_shutting_down) return;
    
    _anim_speed = animation_time_ms;
    anm_utility_set_time_ms(animation_time_ms);

    _anm_brightness_set(brightness);

    _current_mode = rgb_mode;

    switch(rgb_mode)
    {
        case RGB_ANIM_AUTHENTIC:
            _ani_main_fn = anm_authentic_handler;
            _ani_fn_get_state = anm_authentic_get_state;
        break;

        case RGB_ANIM_NONE:
            _ani_main_fn = anm_none_handler;
            _ani_fn_get_state = anm_none_get_state;
        break;

        case RGB_ANIM_RAINBOW:
            _ani_main_fn = anm_rainbow_handler;
            _ani_fn_get_state = anm_rainbow_get_state;
        break;

        case RGB_ANIM_REACT:
            _ani_main_fn = anm_react_handler;
            _ani_fn_get_state = anm_react_get_state;
        break;

        case RGB_ANIM_FAIRY:
            _ani_main_fn = anm_fairy_handler;
            _ani_fn_get_state = anm_fairy_get_state;
        break;

        case RGB_ANIM_IDLE:
            _ani_main_fn = anm_idle_handler;
            _ani_fn_get_state = anm_idle_get_state;
        break;

        default:
            _ani_main_fn = anm_none_handler;
            _ani_fn_get_state = anm_none_get_state;
        break;
    }

    _ani_queue_fade_start();
}

void _notification_manager(rgb_s *output)
{
    const hoja_rgb_cfg_s *rcfg = &hoja_config_get()->rgb;
    if(rcfg->notification_group_index < 0) return;

    const int8_t notif_group = rcfg->notification_group_index;
    uint8_t notif_size = rcfg->notification_group_size;
    if(notif_size > RGB_MAX_LEDS_PER_GROUP) notif_size = RGB_MAX_LEDS_PER_GROUP;

    rgb_s notif_leds[RGB_MAX_LEDS_PER_GROUP] = {0};

    // Get the current notif LEDs
    for(int i = 0; i < notif_size; i++)
    {
        uint8_t this_idx = rgb_led_groups[notif_group][i];
        notif_leds[i] = output[this_idx];
    }

    rgb_s ss_color;
    rgb_s pulse_color;
    if(rgb_get_notification(&ss_color))
    {
        if(ply_blink_handler_ss(notif_leds, notif_size, ss_color))
        {
            rgb_clear_notification();
        }
    }
    else if(rgb_get_pulsing(&pulse_color))
    {
        if(ply_blink_handler(notif_leds, notif_size, pulse_color))
        {

        }
    }

    // Write the player LED colors to the output
    for(int i = 0; i < notif_size; i++)
    {
        uint8_t this_idx = rgb_led_groups[notif_group][i];
        output[this_idx] = notif_leds[i];
    }
}

// The LEDs that show the player: the player group (4-LED chase), else the notification group (blink)
static bool _player_leds_get(int8_t *group_idx, uint8_t *count, bool *use_chase)
{
    const hoja_rgb_cfg_s *rcfg = &hoja_config_get()->rgb;

    if(rcfg->player_group_index >= 0)
    {
        *group_idx = rcfg->player_group_index;
        *count     = RGB_PLAYER_GROUP_SIZE;
        *use_chase = true;
    }
    else if(rcfg->notification_group_index >= 0)
    {
        *group_idx = rcfg->notification_group_index;
        *count     = rcfg->notification_group_size;
        *use_chase = false;
    }
    else return false;

    if(*count > RGB_MAX_LEDS_PER_GROUP) *count = RGB_MAX_LEDS_PER_GROUP;
    return true;
}

// Plays the connecting animation (chase or blink) in color on the player LEDs
static void _player_connecting_draw(rgb_s *output, rgb_s color)
{
    int8_t  player_group_idx;
    uint8_t player_leds_count;
    bool    use_chase;
    if(!_player_leds_get(&player_group_idx, &player_leds_count, &use_chase)) return;

    rgb_s player_leds[RGB_MAX_LEDS_PER_GROUP] = {0};
    for(int i = 0; i < player_leds_count; i++)
        player_leds[i] = output[rgb_led_groups[player_group_idx][i]];

    if(use_chase)
        ply_chase_handler(player_leds, color);
    else
        ply_blink_handler(player_leds, player_leds_count, color);

    for(int i = 0; i < player_leds_count; i++)
        output[rgb_led_groups[player_group_idx][i]] = player_leds[i];
}

void _player_connection_manager(rgb_s *output) 
{
    switch(transport_current_connection())
    {
        case TP_CONNSTAT_IDLE:
        case TP_CONNSTAT_UNDEFINED:
            _player_connecting_draw(output, core_current_color_get());
        break;

        default:
        {
            int8_t  player_group_idx;
            uint8_t player_leds_count;
            bool    use_chase;
            if(!_player_leds_get(&player_group_idx, &player_leds_count, &use_chase)) return;

            // Reactive mode can leave player LEDs black if the player group
            // is not actively driven by input. Seed player LEDs with the
            // configured player-group color before applying the player mask.
            rgb_s player_leds[RGB_MAX_LEDS_PER_GROUP] = {0};
            for(int i = 0; i < player_leds_count; i++)
            {
                player_leds[i] = rgb_colors_safe[player_group_idx];
            }
            ply_idle_handler(player_leds, transport_current_player_number());

            for(int i = 0; i < player_leds_count; i++)
                output[rgb_led_groups[player_group_idx][i]] = player_leds[i];
        }
        break;
    }  
}

volatile bool _anm_idle_active = false;
void anm_set_idle_enable(bool enable)
{
    static int store_mode = 0;
    static int store_bright = 0;
    
    if(enable && !_anm_idle_active)
    {
        store_mode = _current_mode;
        store_bright = _anim_brightness_target;
        anm_handler_setup_mode(RGB_ANIM_IDLE, 500, _anim_speed);
        _anm_idle_active = true;

    }
    else if (!enable && _anm_idle_active)
    {
        anm_handler_setup_mode(store_mode, store_bright, _anim_speed);
        _anm_idle_active = false;
    }
}

// Auto mode: every LED shows the player LED color until the mode is confirmed, with the player LEDs
// playing the connecting animation. The mode's own lighting keeps running underneath (static modes
// only draw once), then fades in.
static rgb_s _anm_hold_leds[RGB_DRIVER_LED_COUNT] = {0};

static bool _anm_auto_hold(void)
{
    static bool holding = false;

    if(!autodetect_pending())
    {
        if(holding)
        {
            holding = false;
            memcpy(_fade_start, _anm_hold_leds, ALL_LEDS_SIZE);
            // A mode change mid-hold queues its own fade: keep its target, not the colors on the way
            if(!_fade)
                memcpy(_fade_end, _current_ani_leds, ALL_LEDS_SIZE);
            _fade_progress = 0;
            _fade = true;
        }
        return false;
    }

    const hoja_rgb_cfg_s *rcfg = &hoja_config_get()->rgb;
    const int8_t group = (rcfg->player_group_index >= 0) ? rcfg->player_group_index : rcfg->notification_group_index;
    rgb_s color = (group >= 0) ? rgb_colors_safe[group] : (rgb_s){0};

    // Fades in from dark, the same way the modes fade
    static uint32_t fade_in_progress = 0;
    rgb_s dark = {0};
    rgb_s shown = {.color = anm_utility_blend(&dark, &color, fade_in_progress)};
    for(int i = 0; i < RGB_DRIVER_LED_COUNT; i++)
        _anm_hold_leds[i] = shown;

    // The player LEDs show it's still connecting
    _player_connecting_draw(_anm_hold_leds, shown);

    if(fade_in_progress < RGB_FADE_FIXED_MULT)
    {
        fade_in_progress += FADE_STEP_FIXED;
        if(fade_in_progress > RGB_FADE_FIXED_MULT)
            fade_in_progress = RGB_FADE_FIXED_MULT;
    }

    holding = true;
    return true;
}

// Call this once per frame
void anm_handler_tick()
{
    // Only compile this function if we have our driver update function
    #if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER>0)
    const bool hold = _anm_auto_hold();

    if(_fade)
    {
        if(_ani_queue_fade_handler(_current_ani_leds))
        {
            _fade = false;
        }
    }
    else if(_ani_main_fn != NULL)
    {
        _ani_main_fn(_current_ani_leds);
    }

    if(!_rgb_shutting_down)
    {
        // While Auto holds, the hold plays the connecting animation itself (they share its state)
        if(!_anm_idle_active && !hold)
            _player_connection_manager(_current_ani_leds);

        _notification_manager(_current_ani_leds);
    }

    // Process brightness/gamma
    _anm_brightness_tick();
    anm_utility_process(hold ? _anm_hold_leds : _current_ani_leds, _adjusted_ani_leds, _anim_brightness);

    RGB_DRIVER_UPDATE(_adjusted_ani_leds);
    #endif
}

void ani_setup_override(rgb_override_t override, uint32_t *parameters)
{
    
}

#endif

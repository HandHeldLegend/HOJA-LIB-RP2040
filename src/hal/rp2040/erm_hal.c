#include "hal/erm_hal.h"
#include "hal/gpio_hal.h"
#include "board_config.h"

#include <string.h>
#include <math.h>
#include <float.h>

// We are in Pico land, use native APIs here :)
#include "pico/stdlib.h"

#include "utilities/settings.h"
#include "utilities/interval.h"
#include "utilities/pcm.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"

#include "devices/haptics.h"

#include "hoja.h"

#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_ERM_HAL)

uint erm_slice_l;
uint erm_slice_l_chan;

uint32_t _pwm_range = 0;
uint32_t _pwm_minimum = 0;
uint32_t _pwm_rise_amt_per_tick = 0;
uint32_t _pwm_fall_amt_per_tick = 0;

// Drive pin + optional active-brake line come from hoja_config_s.haptics.
static uint8_t _erm_chan_a_pin  = 0;
static uint8_t _erm_brake_pin   = 0;
static bool    _erm_brake_enable = false;

#define ERM_TICK_INTERVAL_US   100
#define ERM_PWM_WRAP        0xFFFF

// Maximum safe value puts out ~ 3.2v nominal
// DO NOT CHANGE UNLESS YOU KNOW WHAT YOU ARE DOING
#define ERM_PWM_SAFE_MAX    20000

// Resolved at init: ERM_PWM_SAFE_MAX scaled by haptics.intensity_max (0 = full).
static uint32_t _erm_pwm_scaled_max = ERM_PWM_SAFE_MAX;

#define ERM_STEPS 1000

#define ERM_RISE_TIME_MS 32
#define ERM_FALL_TIME_MS 128

volatile bool _erm_shutdown = false;

void erm_hal_stop()
{
    _erm_shutdown = true;
    pwm_set_enabled(erm_slice_l, false);
    gpio_hal_init(_erm_chan_a_pin, false, false);
    gpio_hal_write(_erm_chan_a_pin, false);
    if(_erm_brake_enable)
    {
        gpio_hal_init(_erm_brake_pin, false, false);
        gpio_hal_write(_erm_brake_pin, true);
    }
}

bool erm_hal_init(uint8_t intensity)
{
    static bool hal_init = false;

    const erm_hal_cfg_s *cfg = &hoja_config_get()->haptics;
    _erm_chan_a_pin   = cfg->channel_a_pin;
    _erm_brake_pin    = cfg->brake_pin;
    _erm_brake_enable = cfg->brake_enable;

    // intensity_max (0..1) scales the safe maximum; 0 keeps the full range.
    _erm_pwm_scaled_max = (cfg->intensity_max > 0.0f)
        ? (uint32_t) (cfg->intensity_max * (float) ERM_PWM_SAFE_MAX)
        : ERM_PWM_SAFE_MAX;

    float scaler = (float) intensity / 255.0f;
    float scaled_max_pwm = (float) _erm_pwm_scaled_max * scaler;
    _pwm_range = (uint32_t) scaled_max_pwm;

    _pwm_rise_amt_per_tick = (uint32_t) (_pwm_range / ((ERM_RISE_TIME_MS * 1000) / ERM_TICK_INTERVAL_US));
    _pwm_fall_amt_per_tick = (uint32_t) (_pwm_range / ((ERM_FALL_TIME_MS * 1000) / ERM_TICK_INTERVAL_US));

    if(hal_init) return true;

    if(_erm_brake_enable)
    {
        gpio_hal_init(_erm_brake_pin, false, false);
        gpio_hal_write(_erm_brake_pin, false);
    }

    // Initialize the PWM and DMA channels
    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 16);
    pwm_config_set_wrap(&config, ERM_PWM_WRAP); // Utilize full 16 bit range

    gpio_set_function(_erm_chan_a_pin, GPIO_FUNC_PWM);
    
    erm_slice_l         = pwm_gpio_to_slice_num(_erm_chan_a_pin);
    erm_slice_l_chan    = pwm_gpio_to_channel(_erm_chan_a_pin);
    pwm_init(erm_slice_l, &config, false);

    pwm_set_enabled(erm_slice_l, true);

    // Set the PWM level to half
    pwm_set_gpio_level(_erm_chan_a_pin, 0);

    hal_init = true;

    return true;
}

#define ERM_SCALE_BITS          16
#define ERM_SCALE_MULTIPLIER    (1 << ERM_SCALE_BITS)

int32_t  _current_level = 0;
uint32_t _target_level  = 0;
uint32_t _max_target_level     = 0;
bool     _max_set              = false;

void erm_hal_task(uint64_t timestamp)
{
    static interval_s interval = {0};
    static uint32_t this_level = 0;

    if(_erm_shutdown) return;

    if(interval_run(timestamp, ERM_TICK_INTERVAL_US, &interval))
    {
        if(_max_set)
        {
            if(_current_level < _max_target_level)
            {
                _current_level += _pwm_rise_amt_per_tick;
            }

            if(_current_level >= _max_target_level)
            {
                _max_target_level = 0;
                _max_set = false;
            }
        }
        
        if(!_max_set)
        {
            if(_current_level < _target_level)
            {
                _current_level += _pwm_rise_amt_per_tick;

                if(_current_level > _target_level)
                {
                    _current_level = _target_level;
                }
            }

            if(_current_level > _target_level)
            {
                _current_level -= _pwm_fall_amt_per_tick;

                if(_current_level < _target_level)
                {
                    _current_level = _target_level;
                }
            }
        }

        _current_level = (_current_level < 0) ? 0 : _current_level;

        if(_current_level != this_level)
        {
            pwm_set_gpio_level(_erm_chan_a_pin, (uint32_t) _current_level);
        }

        this_level = _current_level;
    }
}

void erm_hal_set_standard(uint8_t intensity, bool brake)
{
    if(_erm_shutdown) return;

    (void) brake;
    if(!intensity) 
    {
        _target_level = 0;
        return;
    }

    uint32_t i = ((uint32_t) intensity+1) * _pwm_range;
    i >>= 8;

    uint32_t target = i;

    if(target > _max_target_level)
    {
        _max_target_level = target;
        _max_set = true;
    }

    _target_level = target;
}

void erm_hal_push_amfm(haptic_packet_s *packet)
{
    if(_erm_shutdown) return;
    
    // Use the first pair (or find max amplitude across all pairs)
    uint16_t working_amp = 0;
    for(int i = 0; i < packet->count && i < 3; i++)
    {
        uint16_t pair_amp = (packet->pairs[i].hi_amplitude_fixed > packet->pairs[i].lo_amplitude_fixed)
            ? packet->pairs[i].hi_amplitude_fixed
            : packet->pairs[i].lo_amplitude_fixed;
        if(pair_amp > working_amp) working_amp = pair_amp;
    }

    if(!working_amp)
    {
        erm_hal_set_standard(0, false);
        return;
    }

    uint32_t new_intensity = ((uint32_t) working_amp * 100) >> PCM_AMPLITUDE_BIT_SCALE;
    new_intensity += 155;
    new_intensity = new_intensity > 255 ? 255 : new_intensity;

    erm_hal_set_standard((uint8_t) new_intensity, false);
}

#endif
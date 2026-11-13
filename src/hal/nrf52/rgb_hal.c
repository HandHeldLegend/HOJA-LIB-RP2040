#include "hal/rgb_hal.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER == RGB_DRIVER_HAL)

#include "hal/gpio_hal.h"
#include "hoja.h"
#include "nrf52_hal.h"

// One RGB status LED on PWM1, a pin per colour (the library's first LED; a chain has no meaning
// here). A white LED beside it is held off.

#define RGB_HAL_PWM         NRF_PWM1
// 16 MHz / 800 = 20 kHz, well clear of anything a camera or the eye picks up
#define RGB_HAL_PWM_TOP     800u
#define RGB_HAL_CHANNELS    4u

// One compare value per channel, read by the PWM unit itself: must stay in RAM
static uint16_t _rgb_hal_compare[RGB_HAL_CHANNELS];
static bool _rgb_hal_running = false;

// The count the pin spends in its "on" level. Without the polarity bit a channel starts each period
// low and goes high at the compare value: what an active-low LED needs. Active-high LEDs set the bit.
static uint16_t _rgb_hal_level(uint8_t brightness, bool active_low)
{
    const uint16_t on = (uint16_t)(((uint32_t)brightness * RGB_HAL_PWM_TOP) / 255u);
    return active_low ? on : (uint16_t)(on | 0x8000u);
}

static void _rgb_hal_pin_off(uint8_t gpio, bool active_low)
{
    if (gpio == NRF52_PIN_NONE)
        return;
    gpio_hal_init(gpio, false, false);
    gpio_hal_write(gpio, active_low);
}

void rgb_hal_init()
{
    const rgb_hal_cfg_s *cfg = &hoja_config_get()->rgb_driver;
    const uint8_t pins[RGB_HAL_CHANNELS] = {cfg->pin_r, cfg->pin_g, cfg->pin_b, cfg->pin_w};

    RGB_HAL_PWM->ENABLE = PWM_ENABLE_ENABLE_Disabled;

    for (uint8_t i = 0; i < RGB_HAL_CHANNELS; i++)
    {
        _rgb_hal_pin_off(pins[i], cfg->active_low);
        RGB_HAL_PWM->PSEL.OUT[i] = (pins[i] == NRF52_PIN_NONE) ? NRF52_PSEL_DISCONNECTED : nrf52_psel(pins[i]);
        _rgb_hal_compare[i] = _rgb_hal_level(0, cfg->active_low);
    }

    RGB_HAL_PWM->MODE = PWM_MODE_UPDOWN_Up;
    RGB_HAL_PWM->PRESCALER = PWM_PRESCALER_PRESCALER_DIV_1;
    RGB_HAL_PWM->COUNTERTOP = RGB_HAL_PWM_TOP;
    RGB_HAL_PWM->LOOP = 0;
    RGB_HAL_PWM->DECODER = (PWM_DECODER_LOAD_Individual << PWM_DECODER_LOAD_Pos) |
                           (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);
    RGB_HAL_PWM->SEQ[0].PTR = (uint32_t)_rgb_hal_compare;
    RGB_HAL_PWM->SEQ[0].CNT = RGB_HAL_CHANNELS;
    RGB_HAL_PWM->SEQ[0].REFRESH = 0;
    RGB_HAL_PWM->SEQ[0].ENDDELAY = 0;
    RGB_HAL_PWM->ENABLE = PWM_ENABLE_ENABLE_Enabled;

    _rgb_hal_running = true;
    RGB_HAL_PWM->TASKS_SEQSTART[0] = 1;
}

void rgb_hal_deinit()
{
    if (!_rgb_hal_running)
        return;
    _rgb_hal_running = false;

    const rgb_hal_cfg_s *cfg = &hoja_config_get()->rgb_driver;
    const uint8_t pins[RGB_HAL_CHANNELS] = {cfg->pin_r, cfg->pin_g, cfg->pin_b, cfg->pin_w};

    // The pins fall back to their GPIO levels: off
    RGB_HAL_PWM->ENABLE = PWM_ENABLE_ENABLE_Disabled;
    for (uint8_t i = 0; i < RGB_HAL_CHANNELS; i++)
    {
        RGB_HAL_PWM->PSEL.OUT[i] = NRF52_PSEL_DISCONNECTED;
        _rgb_hal_pin_off(pins[i], cfg->active_low);
    }
}

void rgb_hal_update(rgb_s *data)
{
    if (!_rgb_hal_running || (data == NULL))
        return;

    const bool active_low = hoja_config_get()->rgb_driver.active_low;
    _rgb_hal_compare[0] = _rgb_hal_level(data[0].r, active_low);
    _rgb_hal_compare[1] = _rgb_hal_level(data[0].g, active_low);
    _rgb_hal_compare[2] = _rgb_hal_level(data[0].b, active_low);

    // The values are held until the next start
    RGB_HAL_PWM->TASKS_SEQSTART[0] = 1;
}

#if (RGB_DRIVER_OUTPUT_FRAMES > 1)
#error "nrf52/rgb_hal.c plays one frame: leave RGB_DRIVER_OUTPUT_FRAMES at 1"
#endif

#endif

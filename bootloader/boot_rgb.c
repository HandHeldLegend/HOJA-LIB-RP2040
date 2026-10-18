#include "boot_rgb.h"

#if defined(BOOT_RGB_PIN)

#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"

#include "ws2812.pio.h"

#ifndef BOOT_RGB_LEDS
#define BOOT_RGB_LEDS 32
#endif

#define BOOT_RGB_PIO          pio0
#define BOOT_RGB_SM           0
#define BOOT_RGB_BIT_HZ       12800000u // The ws2812 program's clock: 16 cycles a bit at 800 kHz
#define BOOT_RGB_PERIOD_MS    2400u     // One slow breath
#define BOOT_RGB_FRAME_MS     20u
// A quarter of full brightness at the top of a breath: easy on the battery, plain in daylight. Never
// quite dark at the bottom, so a breath doesn't read as the LEDs going out.
#define BOOT_RGB_PEAK         64u
#define BOOT_RGB_FLOOR        16u // Of 256

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} boot_rgb_color_s;

static const boot_rgb_color_s _boot_rgb_colors[] = {
    [BOOT_RGB_UPDATE_MODE] = {.r = 0,   .g = 0,   .b = 255},
    [BOOT_RGB_WRITING]     = {.r = 255, .g = 110, .b = 0},
};

static bool _boot_rgb_ready;
static uint32_t _boot_rgb_frame_at_ms;

static void _boot_rgb_init(void)
{
    uint offset = pio_add_program(BOOT_RGB_PIO, &ws2812_program);
    pio_sm_config c = ws2812_program_get_default_config(offset);

    pio_gpio_init(BOOT_RGB_PIO, BOOT_RGB_PIN);
    pio_sm_set_consecutive_pindirs(BOOT_RGB_PIO, BOOT_RGB_SM, BOOT_RGB_PIN, 1, true);
    sm_config_set_sideset_pins(&c, BOOT_RGB_PIN);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // The divider in 1/256ths, in integers (the bootloader has no float support)
    uint32_t div256 = (uint32_t)(((uint64_t)clock_get_hz(clk_sys) * 256u) / BOOT_RGB_BIT_HZ);
    sm_config_set_clkdiv_int_frac(&c, (uint16_t)(div256 >> 8), (uint8_t)(div256 & 0xFFu));

    pio_sm_init(BOOT_RGB_PIO, BOOT_RGB_SM, offset, &c);
    pio_sm_set_enabled(BOOT_RGB_PIO, BOOT_RGB_SM, true);
    _boot_rgb_ready = true;
}

// 0..256 over a breath: a triangle, squared so it lingers dim and swells, as eyes judge brightness
static uint32_t _boot_rgb_level(uint32_t now_ms)
{
    uint32_t half = BOOT_RGB_PERIOD_MS / 2u;
    uint32_t phase = now_ms % BOOT_RGB_PERIOD_MS;
    uint32_t ramp = (phase < half) ? phase : (BOOT_RGB_PERIOD_MS - phase);
    uint32_t linear = (ramp * 256u) / half;
    return BOOT_RGB_FLOOR + ((linear * linear) / 256u) * (256u - BOOT_RGB_FLOOR) / 256u;
}

static uint32_t _boot_rgb_scale(uint8_t channel, uint32_t level)
{
    return (channel * level * BOOT_RGB_PEAK) / (256u * 255u);
}

void boot_rgb_task(boot_rgb_state_t state)
{
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    if (!_boot_rgb_ready)
        _boot_rgb_init();
    else if (now_ms - _boot_rgb_frame_at_ms < BOOT_RGB_FRAME_MS)
        return;
    _boot_rgb_frame_at_ms = now_ms;

    const boot_rgb_color_s *color = &_boot_rgb_colors[state];
    uint32_t level = _boot_rgb_level(now_ms);

    // GRB, the top 24 bits shifted out first
    uint32_t word = (_boot_rgb_scale(color->g, level) << 24) | (_boot_rgb_scale(color->r, level) << 16) |
                    (_boot_rgb_scale(color->b, level) << 8);

    for (uint32_t i = 0; i < BOOT_RGB_LEDS; i++)
        pio_sm_put_blocking(BOOT_RGB_PIO, BOOT_RGB_SM, word);
}

#else

void boot_rgb_task(boot_rgb_state_t state)
{
    (void)state;
}

#endif

#include "hal/rgb_hal.h"

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "generated/ws2812.pio.h"

#include "hoja.h"
#include "devices/rgb.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER == RGB_DRIVER_HAL)

// PIO block + state machine are allocated dynamically at init.
static PIO _rgb_pio = NULL;
int _rgb_state_machine = -1;
int _rgb_dma_chan;
uint32_t _rgb_states[RGB_DRIVER_LED_COUNT] = {0};

#if (RGB_DRIVER_OUTPUT_FRAMES > 1)
// The dither frames go out in turn without the CPU: a spare PWM slice wraps at the output rate, and
// each wrap has a second DMA channel hand the next frame's address to the LED channel (its trigger)
static rgb_s _rgb_frames[RGB_DITHER_FRAMES][RGB_DRIVER_LED_COUNT];
// Read as a ring, so it must be aligned to its size
static rgb_s *_rgb_frame_addrs[RGB_DITHER_FRAMES] __attribute__((aligned(RGB_DITHER_FRAMES * sizeof(rgb_s *))));
static int _rgb_pace_dma_chan = -1;
// About 4 days at 720 Hz; rgb_hal_frames restarts it
#define RGB_PACE_TRANSFERS 0x0FFFFFFFu

// The highest PWM slice the haptics don't drive
static int _rgb_hal_pace_slice(void)
{
    uint32_t used = 0;
    #if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_LRA_HAL)
    used |= 1u << pwm_gpio_to_slice_num(hoja_config_get()->haptics.channel_a_pin);
    used |= 1u << pwm_gpio_to_slice_num(hoja_config_get()->haptics.channel_b_pin);
    #elif defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_ERM_HAL)
    used |= 1u << pwm_gpio_to_slice_num(hoja_config_get()->haptics.channel_a_pin);
    #endif
    for (int s = NUM_PWM_SLICES - 1; s >= 0; s--)
    {
        if (!(used & (1u << s))) return s;
    }
    return -1;
}

static void _rgb_hal_pace_start(void)
{
    dma_channel_set_read_addr(_rgb_pace_dma_chan, _rgb_frame_addrs, false);
    dma_channel_set_trans_count(_rgb_pace_dma_chan, RGB_PACE_TRANSFERS, true);
}

static void _rgb_hal_frames_init(void)
{
    const int slice = _rgb_hal_pace_slice();
    if (slice < 0) return;

    // Some boards use every DMA channel (dual LRA, radio, ADC): those send undithered instead
    const int chan = dma_claim_unused_channel(false);
    if (chan < 0) return;

    for (int f = 0; f < RGB_DITHER_FRAMES; f++)
        _rgb_frame_addrs[f] = _rgb_frames[f];

    // Wraps at RGB_DRIVER_REFRESHRATE * RGB_DRIVER_OUTPUT_FRAMES, with the smallest divider that fits
    const uint32_t rate = RGB_DRIVER_REFRESHRATE * RGB_DRIVER_OUTPUT_FRAMES;
    const uint32_t sys_hz = clock_get_hz(clk_sys);
    uint32_t div = (sys_hz / rate + 65535u) / 65536u;
    div = (div < 1u) ? 1u : ((div > 255u) ? 255u : div);
    pwm_config pc = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&pc, div);
    pwm_config_set_wrap(&pc, (uint16_t)(sys_hz / (div * rate) - 1u));
    pwm_init((uint)slice, &pc, true);

    _rgb_pace_dma_chan = chan;
    dma_channel_config c = dma_channel_get_default_config(_rgb_pace_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_ring(&c, false, __builtin_ctz(sizeof(_rgb_frame_addrs)));
    channel_config_set_dreq(&c, DREQ_PWM_WRAP0 + slice);
    dma_channel_configure(
        _rgb_pace_dma_chan,
        &c,
        &dma_hw->ch[_rgb_dma_chan].al3_read_addr_trig,
        _rgb_frame_addrs,
        RGB_PACE_TRANSFERS,
        true
    );
}

rgb_s *rgb_hal_frames(void)
{
    if (_rgb_pace_dma_chan < 0)
    {
        // No spare PWM slice: the first frame goes out once per animation frame, undithered
        if (_rgb_state_machine >= 0 && !dma_channel_is_busy(_rgb_dma_chan))
            dma_channel_transfer_from_buffer_now(_rgb_dma_chan, _rgb_frames[0], RGB_DRIVER_LED_COUNT);
    }
    else if (!dma_channel_is_busy(_rgb_pace_dma_chan))
        _rgb_hal_pace_start();
    return &_rgb_frames[0][0];
}
#endif

void rgb_hal_init()
{
    // Dynamically grab a PIO block + state machine with room for the program.
    uint offset = 0;
    uint sm = 0;
    if(!pio_claim_free_sm_and_add_program(&ws2812_program, &_rgb_pio, &sm, &offset))
    {
        _rgb_state_machine = -1;
        return;
    }
    _rgb_state_machine = (int)sm;

    // Init DMA channel feeding the claimed state machine's TX FIFO
    _rgb_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(_rgb_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(_rgb_pio, sm, true));

    dma_channel_configure(
        _rgb_dma_chan,
        &c,
        &_rgb_pio->txf[sm],
        &_rgb_states,
        RGB_DRIVER_LED_COUNT,
        false
    );

    ws2812_program_init(_rgb_pio, sm, offset, hoja_config_get()->rgb_driver.gpio_pin);
    rgb_hal_update(NULL);

    #if (RGB_DRIVER_OUTPUT_FRAMES > 1)
    _rgb_hal_frames_init();
    #endif
}

void rgb_hal_deinit()
{
    
}


// Update all RGBs
void rgb_hal_update(rgb_s *data)
{
    if(_rgb_state_machine < 0) return;

    // A frame still going out keeps its buffer; this one is skipped
    if (dma_channel_is_busy(_rgb_dma_chan)) return;

    if(data != NULL)
        memcpy(_rgb_states, data, sizeof(uint32_t) * RGB_DRIVER_LED_COUNT);

    dma_channel_transfer_from_buffer_now(_rgb_dma_chan, _rgb_states, RGB_DRIVER_LED_COUNT);
}

#endif
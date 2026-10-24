#include "hal/lra_hal.h"
#include "hal/gpio_hal.h"
#include "board_config.h"

#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_LRA_HAL)

#include <string.h>
#include <math.h>
#include <float.h>

// We are in Pico land, use native APIs here :)
#include "pico/stdlib.h"

#include "utilities/settings.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"

#include "devices/haptics.h"

#include "utilities/pcm.h"

#include "hoja.h"

#if defined(HOJA_LRA_HAL_ENABLE_DRV2605L)
    #include "drivers/haptic/drv2605l.h"
#endif

#define SAMPLE_RATE         PCM_SAMPLE_RATE
#define REPETITION_RATE     PCM_SAMPLE_REPETITIONS
#define PWM_WRAP            PCM_WRAP_VAL

// The output ring: two halves of PCM_BUFFER_SIZE samples, each written REPETITION_RATE times so one
// DMA channel per PWM slice can stream it to the compare register, one word per PWM wrap, with no
// CPU between refills. Each word holds both motors (low half = the slice's A pin, high half = B).
#define LRA_HALF_WORDS      (PCM_BUFFER_SIZE * REPETITION_RATE)
#define LRA_RING_WORDS      (LRA_HALF_WORDS * 2)
#define LRA_RING_BYTES      (LRA_RING_WORDS * sizeof(uint32_t))
_Static_assert((LRA_RING_BYTES & (LRA_RING_BYTES - 1)) == 0, "the DMA read ring needs a power-of-two size");

__attribute__((aligned(LRA_RING_BYTES))) static uint32_t _lra_ring[LRA_RING_WORDS] = {0};

// Up to two slices: the motors share one when their pins are a slice's A and B
#define LRA_MAX_STREAMS 2
static int  _lra_stream_chan[LRA_MAX_STREAMS] = {-1, -1};
static uint _lra_stream_slice[LRA_MAX_STREAMS];
static uint8_t  _lra_stream_count = 0;
static uint32_t _lra_stream_mask = 0;

// Endless on RP2350; on RP2040 about 37 hours at 32 kHz, restarted by _lra_hal_keep_running
#define LRA_STREAM_TRANSFERS 0xFFFFFFFFu

static uint8_t _chan_a_pin    = 0;
static uint8_t _chan_b_pin    = 0;
static bool    _chan_b_enable = false;

static bool     _lra_running = false;
// The half written last; the other one is free once the DMA plays this one
static uint8_t  _lra_filled_half = 1;

// The half the DMA is reading now
static uint8_t _lra_playing_half(void)
{
    const uintptr_t at = (uintptr_t)dma_hw->ch[_lra_stream_chan[0]].read_addr - (uintptr_t)_lra_ring;
    return (uint8_t)((at / sizeof(uint32_t)) / LRA_HALF_WORDS);
}

// Render the next PCM buffer straight into the free half
static void _lra_hal_fill_free_half(void)
{
    uint32_t samples[PCM_BUFFER_SIZE];
    const uint8_t half = (uint8_t)(1u - _lra_filled_half);

    pcm_generate_buffer(samples);

    uint32_t *dst = &_lra_ring[half * LRA_HALF_WORDS];
    for (int i = 0; i < PCM_BUFFER_SIZE; i++)
    {
        for (int r = 0; r < REPETITION_RATE; r++)
        {
            *dst++ = samples[i];
        }
    }

    _lra_filled_half = half;
}

static bool _lra_hal_half_free(void)
{
    return _lra_playing_half() == _lra_filled_half;
}

// RP2040 transfer counts run out: restart them together so the slices stay in step
static void _lra_hal_keep_running(void)
{
#if PICO_RP2040
    if (dma_channel_is_busy(_lra_stream_chan[0]))
        return;

    uint32_t chan_mask = 0;
    for (int s = 0; s < _lra_stream_count; s++)
    {
        dma_channel_set_trans_count(_lra_stream_chan[s], LRA_STREAM_TRANSFERS, false);
        chan_mask |= 1u << _lra_stream_chan[s];
    }
    dma_start_channel_mask(chan_mask);
#endif
}

static void _lra_hal_stream_start(uint slice)
{
    const int chan = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_ring(&c, false, __builtin_ctz(LRA_RING_BYTES));
    channel_config_set_dreq(&c, DREQ_PWM_WRAP0 + slice); // one word per PWM cycle

    dma_channel_configure(chan, &c, &pwm_hw->slice[slice].cc, _lra_ring, LRA_STREAM_TRANSFERS, false);

    _lra_stream_chan[_lra_stream_count]  = chan;
    _lra_stream_slice[_lra_stream_count] = slice;
    _lra_stream_count++;
    _lra_stream_mask |= 1u << slice;
}

void lra_hal_stop()
{
    if (!_lra_running)
        return;
    _lra_running = false;

    for (int s = 0; s < _lra_stream_count; s++)
    {
        dma_channel_abort(_lra_stream_chan[s]);
    }
    pwm_set_mask_enabled(pwm_hw->en & ~_lra_stream_mask);
}

bool lra_hal_init(uint8_t intensity)
{
    static bool hal_init = false;

    const lra_hal_cfg_s *cfg = &hoja_config_get()->haptics;
    _chan_a_pin    = cfg->channel_a_pin;
    _chan_b_pin    = cfg->channel_b_pin;
    _chan_b_enable = cfg->channel_b_enable;

    // Initialize the PCM
    pcm_init(intensity);

    if (hal_init) return true;
    hal_init = true;

#if defined(HOJA_LRA_HAL_ENABLE_DRV2605L)
    if (!drv2605l_init(cfg->drv2605l.i2c_instance, cfg->drv2605l.od_clamp))
        return false;
#endif

    // One PWM cycle per repeated sample
    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    float clock_div = ((float)f_clk_sys * 1000.0f) / PWM_WRAP / (float)SAMPLE_RATE / (float)REPETITION_RATE;

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_div);
    pwm_config_set_wrap(&config, PWM_WRAP);

    const uint slice_a = pwm_gpio_to_slice_num(_chan_a_pin);
    gpio_set_function(_chan_a_pin, GPIO_FUNC_PWM);
    pwm_init(slice_a, &config, false);
    _lra_hal_stream_start(slice_a);

    if (_chan_b_enable)
    {
        const uint slice_b = pwm_gpio_to_slice_num(_chan_b_pin);
        gpio_set_function(_chan_b_pin, GPIO_FUNC_PWM);

        // On the A slice's other output, the A stream's word already carries it
        if (slice_b != slice_a)
        {
            pwm_init(slice_b, &config, false);
            _lra_hal_stream_start(slice_b);
        }
    }

    // Both halves start silent; the first refill comes once the DMA reaches the second half
    memset(_lra_ring, 0, sizeof(_lra_ring));
    _lra_filled_half = 1;

    // The channels wait on their slices, which start on the same cycle
    uint32_t chan_mask = 0;
    for (int s = 0; s < _lra_stream_count; s++)
    {
        chan_mask |= 1u << _lra_stream_chan[s];
    }
    dma_start_channel_mask(chan_mask);
    pwm_set_mask_enabled(pwm_hw->en | _lra_stream_mask);

    _lra_running = true;
    return true;
}

// Every pass: a register read, and every 8 ms the next buffer. It renders the moment a half frees
// up, as late as it can be and still have the whole half ahead of the DMA, so rumble plays 8 ms
// after it's rendered. A budget-aware task can't do this: the recurring rotation waits on tasks that
// aren't due yet (the LEDs' 60 Hz), so it only comes round every ~17 ms.
void lra_hal_task(uint64_t timestamp)
{
    (void)timestamp;
    if (!_lra_running)
        return;

    if (!_lra_hal_half_free())
        return;

    _lra_hal_fill_free_half();
    _lra_hal_keep_running();
}

void lra_hal_push_amfm(haptic_packet_s *packet)
{
    pcm_amfm_push(packet);
}

void lra_hal_set_standard(uint8_t intensity, bool brake)
{
    pcm_erm_set(intensity, brake);
}

#endif // HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_LRA_HAL

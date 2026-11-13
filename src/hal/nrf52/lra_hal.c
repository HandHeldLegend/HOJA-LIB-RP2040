#include "hal/lra_hal.h"

#if defined(HOJA_HAPTICS_DRIVER) && (HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_LRA_HAL)

#include <string.h>

#include "devices/haptics.h"
#include "hal/gpio_hal.h"
#include "hoja.h"
#include "nrf52_hal.h"
#include "utilities/pcm.h"

// The haptics as audio: the PCM engine's buffers go to a stereo I2S amplifier at the engine's own
// 8 kHz, 16 bits a channel. The I2S unit reads a buffer by itself and takes the address of the next
// one when it starts each, so the task only has to render one buffer ahead (8 ms).
//
// The engine's samples are for a PWM pin: unipolar, lifted by their envelope. An amplifier wants
// them around zero, so each channel goes through a DC blocker first.

_Static_assert(PCM_SAMPLE_RATE == 8000, "the I2S clock below is set for 8 kHz");

#define LRA_I2S             NRF_I2S
// 32 MHz / 125 = 256 kHz, which is both the bit clock (32 bits a frame) and 32 x the 8 kHz frame rate
#define LRA_I2S_MCKFREQ     I2S_CONFIG_MCKFREQ_MCKFREQ_32MDIV125
#define LRA_I2S_RATIO       I2S_CONFIG_RATIO_RATIO_32X

#define LRA_BUFFER_US       ((PCM_BUFFER_SIZE * 1000000u) / PCM_SAMPLE_RATE)
// The amplifier and the clocks rest once the engine has been silent this long
#define LRA_IDLE_BUFFERS    (2000000u / LRA_BUFFER_US)

// A PWM count of PCM_WRAP_HALF_VAL is the engine's full amplitude: full scale here
#define LRA_SAMPLE_SHIFT    4
_Static_assert((PCM_WRAP_HALF_VAL << LRA_SAMPLE_SHIFT) == 32768, "PCM full amplitude is 16-bit full scale");

// DC blocker: y = x - x' + k * y', k = 1 - 2^-LRA_DC_SHIFT (a corner near 10 Hz at 8 kHz)
#define LRA_DC_SHIFT        7

typedef struct
{
    int32_t last_in;
    int32_t last_out_q8; // 8 fraction bits, so the decay doesn't stall near zero
} lra_dc_block_s;

// Two buffers, played in turn. A word is one stereo frame.
static uint32_t _lra_buffers[2][PCM_BUFFER_SIZE];
static uint8_t _lra_next = 0; // The buffer to render into

static lra_dc_block_s _lra_dc[2];
static bool _lra_ready = false;
static bool _lra_streaming = false;
static uint8_t _lra_enable_pin = NRF52_PIN_NONE;
static uint32_t _lra_silent_buffers = 0;
static uint64_t _lra_last_render_us = 0;

static int16_t _lra_dc_block(lra_dc_block_s *dc, int32_t in)
{
    int32_t out_q8 = ((in - dc->last_in) << 8) + dc->last_out_q8 - (dc->last_out_q8 >> LRA_DC_SHIFT);
    dc->last_in = in;
    dc->last_out_q8 = out_q8;

    int32_t out = (out_q8 >> 8) << LRA_SAMPLE_SHIFT;
    if (out > 32767)
        out = 32767;
    else if (out < -32768)
        out = -32768;
    return (int16_t)out;
}

// Renders the next 8 ms into the buffer not being played. False when all of it is silence.
static bool _lra_hal_render(uint32_t *out)
{
    uint32_t samples[PCM_BUFFER_SIZE];
    bool active = false;

    pcm_generate_buffer(samples);

    for (int i = 0; i < PCM_BUFFER_SIZE; i++)
    {
        // The engine's word: left in the high half, right in the low half
        const int16_t left = _lra_dc_block(&_lra_dc[0], (int32_t)(samples[i] >> 16));
        const int16_t right = _lra_dc_block(&_lra_dc[1], (int32_t)(samples[i] & 0xFFFFu));

        if (left || right)
            active = true;

        // The I2S word is the other way round: left in the low half
        out[i] = ((uint32_t)(uint16_t)right << 16) | (uint16_t)left;
    }
    return active;
}

static void _lra_hal_stream_stop(void)
{
    if (!_lra_streaming)
        return;
    _lra_streaming = false;

    if (_lra_enable_pin != NRF52_PIN_NONE)
        gpio_hal_write(_lra_enable_pin, false);

    LRA_I2S->EVENTS_STOPPED = 0;
    LRA_I2S->TASKS_STOP = 1;
}

// Starts playing `first`; the unit takes the address again for the buffer after it
static void _lra_hal_stream_start(uint32_t *first)
{
    // The amplifier is enabled ahead of its clocks
    if (_lra_enable_pin != NRF52_PIN_NONE)
        gpio_hal_write(_lra_enable_pin, true);

    LRA_I2S->RXTXD.MAXCNT = PCM_BUFFER_SIZE;
    LRA_I2S->TXD.PTR = (uint32_t)first;
    LRA_I2S->EVENTS_TXPTRUPD = 0;
    LRA_I2S->TASKS_START = 1;

    _lra_streaming = true;
    _lra_silent_buffers = 0;
}

void lra_hal_stop()
{
    _lra_hal_stream_stop();
    _lra_ready = false;
}

bool lra_hal_init(uint8_t intensity)
{
    const lra_hal_cfg_s *cfg = &hoja_config_get()->haptics;

    pcm_init(intensity);

    if (_lra_ready)
        return true;

    _lra_enable_pin = cfg->enable_pin;
    if (_lra_enable_pin != NRF52_PIN_NONE)
    {
        gpio_hal_init(_lra_enable_pin, false, false);
        gpio_hal_write(_lra_enable_pin, false);
    }

    gpio_hal_init(cfg->sck_pin, false, false);
    gpio_hal_init(cfg->lrck_pin, false, false);
    gpio_hal_init(cfg->sdout_pin, false, false);
    gpio_hal_write(cfg->sck_pin, false);
    gpio_hal_write(cfg->lrck_pin, false);
    gpio_hal_write(cfg->sdout_pin, false);

    LRA_I2S->ENABLE = I2S_ENABLE_ENABLE_Disabled;
    LRA_I2S->PSEL.SCK = nrf52_psel(cfg->sck_pin);
    LRA_I2S->PSEL.LRCK = nrf52_psel(cfg->lrck_pin);
    LRA_I2S->PSEL.SDOUT = nrf52_psel(cfg->sdout_pin);
    LRA_I2S->PSEL.MCK = NRF52_PSEL_DISCONNECTED;
    LRA_I2S->PSEL.SDIN = NRF52_PSEL_DISCONNECTED;

    LRA_I2S->CONFIG.MODE = I2S_CONFIG_MODE_MODE_Master;
    LRA_I2S->CONFIG.TXEN = I2S_CONFIG_TXEN_TXEN_Enabled;
    LRA_I2S->CONFIG.RXEN = I2S_CONFIG_RXEN_RXEN_Disabled;
    // The master clock runs the unit, whether or not it reaches a pin
    LRA_I2S->CONFIG.MCKEN = I2S_CONFIG_MCKEN_MCKEN_Enabled;
    LRA_I2S->CONFIG.MCKFREQ = LRA_I2S_MCKFREQ;
    LRA_I2S->CONFIG.RATIO = LRA_I2S_RATIO;
    LRA_I2S->CONFIG.SWIDTH = I2S_CONFIG_SWIDTH_SWIDTH_16Bit;
    LRA_I2S->CONFIG.ALIGN = I2S_CONFIG_ALIGN_ALIGN_Left;
    LRA_I2S->CONFIG.FORMAT = I2S_CONFIG_FORMAT_FORMAT_I2S;
    LRA_I2S->CONFIG.CHANNELS = I2S_CONFIG_CHANNELS_CHANNELS_Stereo;
    LRA_I2S->INTENCLR = 0xFFFFFFFFu;
    LRA_I2S->ENABLE = I2S_ENABLE_ENABLE_Enabled;

    memset(_lra_buffers, 0, sizeof(_lra_buffers));
    memset(_lra_dc, 0, sizeof(_lra_dc));
    _lra_next = 0;
    _lra_streaming = false;
    _lra_ready = true;
    return true;
}

// Every pass. While playing: a register read, and every 8 ms (when the unit starts a buffer) the next
// one is rendered and its address handed over. While resting: a buffer every 8 ms by the clock, to
// hear the engine start again.
void lra_hal_task(uint64_t timestamp)
{
    if (!_lra_ready)
        return;

    if (_lra_streaming)
    {
        if (!LRA_I2S->EVENTS_TXPTRUPD)
            return;
        LRA_I2S->EVENTS_TXPTRUPD = 0;
    }
    else
    {
        if ((timestamp - _lra_last_render_us) < LRA_BUFFER_US)
            return;
    }
    _lra_last_render_us = timestamp;

    uint32_t *buffer = _lra_buffers[_lra_next];
    const bool active = _lra_hal_render(buffer);

    if (_lra_streaming)
    {
        LRA_I2S->TXD.PTR = (uint32_t)buffer;
        _lra_next ^= 1u;

        _lra_silent_buffers = active ? 0 : (_lra_silent_buffers + 1u);
        if (_lra_silent_buffers > LRA_IDLE_BUFFERS)
            _lra_hal_stream_stop();
    }
    else if (active)
    {
        _lra_hal_stream_start(buffer);
        // Until the next one is rendered, the unit would repeat this buffer: silence instead
        _lra_next ^= 1u;
        memset(_lra_buffers[_lra_next], 0, sizeof(_lra_buffers[0]));
        LRA_I2S->TXD.PTR = (uint32_t)_lra_buffers[_lra_next];
    }
}

void lra_hal_push_amfm(haptic_packet_s *packet)
{
    pcm_amfm_push(packet);
}

void lra_hal_set_standard(uint8_t intensity, bool brake)
{
    pcm_erm_set(intensity, brake);
}

#endif

#include "hal/hdrumble_hal.h"
#include "hal/gpio_hal.h"
#include "board_config.h"

#include <string.h>
#include <math.h>
#include <float.h>

#if defined(HOJA_CONFIG_HDRUMBLE) && (HOJA_CONFIG_HDRUMBLE == 1)

// We are in Pico land, use native APIs here :)
#include "pico/stdlib.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"

#include "extensions/haptics.h"


#define SIN_TABLE_SIZE 4096
int16_t sin_table[SIN_TABLE_SIZE] = {0};
#define SIN_RANGE_MAX 128
#define M_PI 3.14159265358979323846
#define TWO_PI (M_PI * 2)

#define SAMPLE_RATE 12000
#define REPETITION_RATE 4
#define BUFFER_SIZE 255
#define SAMPLE_TRANSITION 30
#define PWM_WRAP BUFFER_SIZE

typedef struct
{
    // How much we increase the phase each time
    float phase_step;
    // How much to increase our phase step each time
    float phase_accumulator;
    // Our current phase
    float phase;

    float f; // Current frequency
    float f_step;
    float f_target;

    float a; // Current amplitude
    float a_step;
    float a_target;

} haptic_s;

typedef struct
{
    haptic_s lo_state;
    haptic_s hi_state;
} haptic_both_s;

#if defined(HOJA_HDRUMBLE_CHAN_A_PIN)
    uint dma_cc_l, dma_trigger_l, dma_sample;
    uint pwm_slice_l;

    haptic_both_s haptics_left = {0};
#endif

#if defined(HOJA_HDRUMBLE_CHAN_B_PIN)
    #ifndef HOJA_HDRUMBLE_CHAN_A_PIN
        #error "To use channel B, HOJA_HDRUMBLE_CHAN_A_PIN must be defined too." 
    #endif 

    uint dma_cc_r, dma_trigger_r;
    uint pwm_slice_r;

    haptic_both_s haptics_right = {0};
#endif

uint32_t dma_trigger_start_mask = 0;

uint32_t single_sample;
uint32_t *single_sample_ptr = &single_sample;

// The current audio buffer in use
static volatile uint audio_buffer_idx = 0;
static uint32_t audio_buffers[2][BUFFER_SIZE] = {0};

float _rumble_scaler = 1;

// Bool to indicate we need to fill our next sine wave
static volatile bool ready_next_sine = false;

void _sine_table_init()
{
    float inc = TWO_PI / SIN_TABLE_SIZE;
    float fi = 0;

    for (int i = 0; i < SIN_TABLE_SIZE; i++)
    {
        float sample = sinf(fi);

        sin_table[i] = (int16_t)(sample * SIN_RANGE_MAX);

        fi += inc;
        fi = fmodf(fi, TWO_PI);
    }
}

static void __isr __time_critical_func(_dma_handler)()
{
    audio_buffer_idx = 1-audio_buffer_idx;
    dma_hw->ch[dma_sample].al1_read_addr    = (intptr_t) &audio_buffers[audio_buffer_idx][0];
    dma_hw->ch[dma_trigger_l].al1_read_addr = (intptr_t) &single_sample_ptr;

    // Trigger right channel if we have one
    #ifdef HOJA_HDRUMBLE_CHAN_B_PIN
        dma_hw->ch[dma_trigger_r].al1_read_addr = (intptr_t) &single_sample_ptr;
    #endif
    
    dma_start_channel_mask(dma_trigger_start_mask);
    ready_next_sine = true;
    dma_hw->ints1 = 1u << dma_trigger_l;
}

float _clamp_rumble_lo(float amplitude)
{
    const float min = 0.12f;
    const float max = 1.0f;

    const float upper_expander = 0.5f; // We want to treat 0 to 0.75 as 0 to 1

    if (!amplitude)
        return 0;
    if (!_rumble_scaler)
        return 0;

    float expanded_amp = amplitude / upper_expander;
    expanded_amp = (expanded_amp > 1) ? 1 : expanded_amp;

    float range = max - min;
    range *= _rumble_scaler;

    float retval = 0;

    retval = range * expanded_amp;
    retval += min;
    if (retval > max)
        retval = max;
    return retval;
}

float _clamp_rumble_hi(float amplitude)
{
    const float min = 0.2f;
    const float max = 1.0f;

    const float upper_expander = 0.5f; // We want to treat 0 to 0.75 as 0 to 1

    if (!amplitude)
        return 0;
    if (!_rumble_scaler)
        return 0;

    float expanded_amp = amplitude / upper_expander;
    expanded_amp = (expanded_amp > 1) ? 1 : expanded_amp;

    float range = max - min;
    range *= _rumble_scaler;

    float retval = 0;

    retval = range * expanded_amp;
    retval += min;
    if (retval > max)
        retval = max;
    return retval;
}

void _generate_sine_wave(uint32_t *buffer, haptic_both_s *state)
{
    uint16_t    transition_idx = 0;
    uint8_t     sample_idx = 0;
    amfm_s      cur[3] = {0};
    int8_t      samples = haptics_get(true, cur, false, NULL);

    for (uint16_t i = 0; i < BUFFER_SIZE; i++)
    {
        // Check if we are at the start of a new block of waveform
        if (!transition_idx)
        {
            if ((samples > 0) && (sample_idx < samples))
            {
                // Set high frequency
                float hif = cur[sample_idx].f_hi;
                float hia = cur[sample_idx].a_hi;

                state->hi_state.f_target = hif;
                state->hi_state.f_step = (hif - state->hi_state.f) / SAMPLE_TRANSITION;

                state->hi_state.phase_step = (SIN_TABLE_SIZE * state->hi_state.f) / SAMPLE_RATE;
                float hphase_step_end = (SIN_TABLE_SIZE * state->hi_state.f_target) / SAMPLE_RATE;
                state->hi_state.phase_accumulator = (hphase_step_end - state->hi_state.phase_step) / SAMPLE_TRANSITION;

                state->hi_state.a_target = hia;
                state->hi_state.a_step = (hia - state->hi_state.a) / SAMPLE_TRANSITION;

                float lof = cur[sample_idx].f_lo;
                float loa = cur[sample_idx].a_lo;

                // Set low frequency
                state->lo_state.f_target = lof;
                state->lo_state.f_step = (lof - state->lo_state.f) / SAMPLE_TRANSITION;

                state->lo_state.phase_step = (SIN_TABLE_SIZE * state->lo_state.f) / SAMPLE_RATE;
                float lphase_step_end = (SIN_TABLE_SIZE * state->lo_state.f_target) / SAMPLE_RATE;
                state->lo_state.phase_accumulator = (lphase_step_end - state->lo_state.phase_step) / SAMPLE_TRANSITION;

                state->lo_state.a_target = loa;
                state->lo_state.a_step = (loa - state->lo_state.a) / SAMPLE_TRANSITION;

                sample_idx++;
            }
            else
            {
                // reset all steps so we just
                // continue generating
                state->hi_state.f_step = 0;
                state->hi_state.a_step = 0;
                state->lo_state.f_step = 0;
                state->lo_state.a_step = 0;
                state->hi_state.phase_accumulator = 0;
                state->lo_state.phase_accumulator = 0;
            }
        }

        if (state->hi_state.phase_accumulator > 0)
            state->hi_state.phase_step += state->hi_state.phase_accumulator;

        if (state->lo_state.phase_accumulator > 0)
            state->lo_state.phase_step += state->lo_state.phase_accumulator;

        float sample_high   = sin_table[(uint16_t)state->hi_state.phase];
        float sample_low    = sin_table[(uint16_t)state->lo_state.phase];

        float hi_a = _clamp_rumble_hi(state->hi_state.a);
        float lo_a = _clamp_rumble_lo(state->lo_state.a);
        float comb_a = hi_a + lo_a;

        if ((comb_a) > 1.0f)
        {
            float new_ratio = 1.0f / (comb_a);
            hi_a *= new_ratio;
            lo_a *= new_ratio;
        }

        sample_high *= hi_a;
        sample_low *= lo_a;

        // Combine samples
        float sample = sample_low + sample_high;
        //sample*=0.45f;

        sample = (sample > 255) ? 255 : (sample < 0) ? 0
                                                     : sample;

        uint32_t outsample = ((uint32_t) sample << 16) | (uint8_t) sample;
        buffer[i] = outsample;

        // Update phases and steps
        state->hi_state.phase += state->hi_state.phase_step;
        state->lo_state.phase += state->lo_state.phase_step;

        // Keep phases within range
        state->hi_state.phase = fmodf(state->hi_state.phase, SIN_TABLE_SIZE);
        state->lo_state.phase = fmodf(state->lo_state.phase, SIN_TABLE_SIZE);

        if (transition_idx < SAMPLE_TRANSITION)
        {
            state->hi_state.f += state->hi_state.f_step;
            state->hi_state.a += state->hi_state.a_step;

            state->lo_state.f += state->lo_state.f_step;
            state->lo_state.a += state->lo_state.a_step;

            transition_idx++;
        }
        else
        {
            // Set values to target
            state->hi_state.f = state->hi_state.f_target;
            state->hi_state.a = state->hi_state.a_target;
            state->lo_state.f = state->lo_state.f_target;
            state->lo_state.a = state->lo_state.a_target;
            state->hi_state.phase_accumulator = 0;
            state->lo_state.phase_accumulator = 0;

            // Reset sample index
            transition_idx = 0;
        }
    }
}

bool hdrumble_hal_init()
{
    _sine_table_init();

    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    float clock_div = ( (float) f_clk_sys * 1000.0f) / 254.0f / (float) SAMPLE_RATE / (float) REPETITION_RATE;

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_div);
    pwm_config_set_wrap(&config, 254);

    uint32_t pwm_enable_mask = 0;

    dma_sample = dma_claim_unused_channel(true);

    #ifdef HOJA_HDRUMBLE_CHAN_A_PIN
        gpio_set_function(HOJA_HDRUMBLE_CHAN_A_PIN, GPIO_FUNC_PWM);

        pwm_slice_l = pwm_gpio_to_slice_num(HOJA_HDRUMBLE_CHAN_A_PIN);
        pwm_init(pwm_slice_l, &config, false);

        pwm_enable_mask         |= (1U<<pwm_slice_l);
        dma_cc_l                 = dma_claim_unused_channel(true);
        dma_trigger_l            = dma_claim_unused_channel(true);
        dma_trigger_start_mask  |= (1U<<dma_trigger_l);

        // setup PWM DMA channel LEFT
        
        dma_channel_config dma_cc_l_cfg = dma_channel_get_default_config(dma_cc_l);
        channel_config_set_transfer_data_size(&dma_cc_l_cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&dma_cc_l_cfg, false);
        channel_config_set_write_increment(&dma_cc_l_cfg, false);
        channel_config_set_chain_to(&dma_cc_l_cfg, dma_sample);               // Chain to increment DMA sample
        channel_config_set_dreq(&dma_cc_l_cfg, DREQ_PWM_WRAP0 + pwm_slice_l); // transfer on PWM cycle end

        dma_channel_configure(dma_cc_l,
                            &dma_cc_l_cfg,
                            &pwm_hw->slice[pwm_slice_l].cc, // write to PWM slice CC register (Compare)
                            &single_sample,
                            REPETITION_RATE, // Write the sample to the CC 4 times
                            false);

        // setup trigger DMA channel LEFT
        dma_channel_config dma_trigger_l_cfg = dma_channel_get_default_config(dma_trigger_l);
        channel_config_set_transfer_data_size(&dma_trigger_l_cfg, DMA_SIZE_32);    // transfer 32-bits at a time
        channel_config_set_read_increment(&dma_trigger_l_cfg, false);              // always read from the same address
        channel_config_set_write_increment(&dma_trigger_l_cfg, false);             // always write to the same address
        channel_config_set_dreq(&dma_trigger_l_cfg, DREQ_PWM_WRAP0 + pwm_slice_l); // transfer on PWM cycle end

        dma_channel_configure(dma_trigger_l,
                            &dma_trigger_l_cfg,
                            &dma_hw->ch[dma_cc_l].al3_read_addr_trig, // write to PWM DMA channel read address trigger
                            &single_sample_ptr,                       // read from location containing the address of single_sample
                            REPETITION_RATE * BUFFER_SIZE,            // trigger once per audio sample per repetition rate
                            false);

        dma_channel_set_irq1_enabled(dma_trigger_l, true); // fire interrupt when trigger DMA channel is done
        // We do not set up our ISR for dma_trigger_l since they are in sync
        irq_set_exclusive_handler(DMA_IRQ_1, _dma_handler);
        irq_set_enabled(DMA_IRQ_1, true);

        // setup sample DMA channel
        dma_channel_config dma_sample_cfg = dma_channel_get_default_config(dma_sample);
        channel_config_set_transfer_data_size(&dma_sample_cfg, DMA_SIZE_32); // transfer 8-bits at a time
        channel_config_set_read_increment(&dma_sample_cfg, true);            // increment read address to go through audio buffer
        channel_config_set_write_increment(&dma_sample_cfg, false);          // always write to the same address
        dma_channel_configure(dma_sample,
                            &dma_sample_cfg,
                            &single_sample,       // write to single_sample. This contains the data for both CH A and CH B
                            &audio_buffers[0][0], // read from audio buffer
                            1,                    // only do one transfer (once per PWM DMA completion due to chaining)
                            false                 // don't start yet
        );

        // clear audio buffer
        memset(audio_buffers[0], 0, BUFFER_SIZE);
        
    #endif

    #ifdef HOJA_HDRUMBLE_CHAN_B_PIN
        gpio_set_function(HOJA_HDRUMBLE_CHAN_B_PIN, GPIO_FUNC_PWM);

        pwm_slice_r = pwm_gpio_to_slice_num(HOJA_HDRUMBLE_CHAN_B_PIN);
        pwm_init(pwm_slice_r, &config, false);

        pwm_enable_mask         |= (1U<<pwm_slice_r);
        dma_cc_r                 = dma_claim_unused_channel(true);
        dma_trigger_r            = dma_claim_unused_channel(true);
        dma_trigger_start_mask  |= (1U<<dma_trigger_r);

        // setup PWM DMA channel RIGHT
        dma_channel_config dma_cc_r_cfg = dma_channel_get_default_config(dma_cc_r);

        channel_config_set_transfer_data_size(&dma_cc_r_cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&dma_cc_r_cfg, false);
        channel_config_set_write_increment(&dma_cc_r_cfg, false);
        // We do not chain the right channel to anything
        channel_config_set_dreq(&dma_cc_r_cfg, DREQ_PWM_WRAP0 + pwm_slice_r); // transfer on PWM cycle end

        dma_channel_configure(dma_cc_r,
                            &dma_cc_r_cfg,
                            &pwm_hw->slice[pwm_slice_r].cc, // write to PWM slice CC register (Compare)
                            &single_sample,
                            REPETITION_RATE, // Write the sample to the CC 4 times
                            false);
        
        // setup trigger DMA channel RIGHT
        dma_channel_config dma_trigger_r_cfg = dma_channel_get_default_config(dma_trigger_r);
        channel_config_set_transfer_data_size(&dma_trigger_r_cfg, DMA_SIZE_32);    // transfer 32-bits at a time
        channel_config_set_read_increment(&dma_trigger_r_cfg, false);              // always read from the same address
        channel_config_set_write_increment(&dma_trigger_r_cfg, false);             // always write to the same address
        channel_config_set_dreq(&dma_trigger_r_cfg, DREQ_PWM_WRAP0 + pwm_slice_r); // transfer on PWM cycle end

        dma_channel_configure(dma_trigger_r,
                            &dma_trigger_r_cfg,
                            &dma_hw->ch[dma_cc_r].al3_read_addr_trig, // write to PWM DMA channel read address trigger
                            &single_sample_ptr,                       // read from location containing the address of single_sample
                            REPETITION_RATE * BUFFER_SIZE,            // trigger once per audio sample per repetition rate
                            false);

        // clear audio buffer
        memset(audio_buffers[1], 0, BUFFER_SIZE);
    #endif

    pwm_set_mask_enabled(pwm_enable_mask);
    dma_start_channel_mask(dma_trigger_start_mask);

    return true;
}

void hdrumble_hal_task(uint32_t timestamp)
{
    if (ready_next_sine)
    {
        ready_next_sine = false;
        uint8_t available_buffer = 1 - audio_buffer_idx;
        _generate_sine_wave(audio_buffers[available_buffer], &haptics_left);
    }
}

void hdrumble_hal_test()
{

}

#endif
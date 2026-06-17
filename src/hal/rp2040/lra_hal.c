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
#define BUFFER_SIZE         PCM_BUFFER_SIZE*2
#define PWM_WRAP            PCM_WRAP_VAL

uint dma_reset;

// Channel A is mandatory for the LRA HAL; channel B is optional and gated at
// runtime by _chan_b_enable. PWM slices + DMA channels are claimed dynamically
// at init. The GPIO pins + dual-channel flag come from hoja_config_s.haptics.
uint dma_cc_l, dma_trigger_l, dma_sample;
uint pwm_slice_l;

uint dma_cc_r, dma_trigger_r, dma_reset_r;
uint pwm_slice_r;

static uint8_t _chan_a_pin    = 0;
static uint8_t _chan_b_pin    = 0;
static bool    _chan_b_enable = false;

uint32_t dma_trigger_start_mask = 0;
volatile uint32_t *dma_trigger_start_ptr = &dma_trigger_start_mask;

uint32_t single_sample;
uint32_t *single_sample_ptr = &single_sample;

// The current audio buffer in use
static volatile uint audio_buffer_idx = 0;
//static uint32_t audio_buffers[2][BUFFER_SIZE] = {0};

__attribute__((aligned(512))) uint32_t audio_buffer[BUFFER_SIZE] = {0};
uint32_t *audio_buffer_ptr = &audio_buffer[0];

// Bool to indicate we need to fill our next sine wave
static volatile bool ready_next_sine = false;

static void __isr __time_critical_func(_dma_handler)()
{
    audio_buffer_idx = 1-audio_buffer_idx;
    //dma_hw->ch[dma_sample].al1_read_addr    = (intptr_t) &audio_buffers[audio_buffer_idx][0];
    dma_hw->ch[dma_trigger_l].al1_read_addr = (intptr_t) &single_sample_ptr;

    // Trigger right channel if we have one
    if(_chan_b_enable)
        dma_hw->ch[dma_trigger_r].al1_read_addr = (intptr_t) &single_sample_ptr;

    dma_start_channel_mask(dma_trigger_start_mask);
    ready_next_sine = true;
    dma_hw->ints1 = 1u << dma_trigger_l;
    uint8_t available_buffer = 1 - audio_buffer_idx;
}

void lra_hal_stop()
{
    dma_channel_abort(dma_cc_l);
    dma_channel_abort(dma_trigger_l);
    dma_channel_abort(dma_sample);
    dma_channel_abort(dma_reset);
    pwm_set_enabled(pwm_slice_l, false);

    if(_chan_b_enable)
    {
        dma_channel_abort(dma_cc_r);
        dma_channel_abort(dma_trigger_r);
        pwm_set_enabled(pwm_slice_r, false);
    }
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

    if(hal_init) return true;
    hal_init = true;

#if defined(HOJA_LRA_HAL_ENABLE_DRV2605L)
    if(!drv2605l_init(cfg->drv2605l.i2c_instance, cfg->drv2605l.od_clamp))
        return false;
#endif

    // Initialize the PWM and DMA channels
    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    float clock_div = ( (float) f_clk_sys * 1000.0f) / PWM_WRAP / (float) SAMPLE_RATE / (float) REPETITION_RATE;

    // Clock div equation should basically equal 1
    //clock_div = 1.0f;

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_div);
    pwm_config_set_wrap(&config, PWM_WRAP);

    uint32_t pwm_enable_mask = 0;

    // ---- Channel A (mandatory) ----
    {
        dma_sample = dma_claim_unused_channel(true);
        gpio_set_function(_chan_a_pin, GPIO_FUNC_PWM);

        pwm_slice_l = pwm_gpio_to_slice_num(_chan_a_pin);
        pwm_init(pwm_slice_l, &config, false);

        pwm_enable_mask         |= (1U<<pwm_slice_l);
        dma_cc_l                 = dma_claim_unused_channel(true);
        dma_trigger_l            = dma_claim_unused_channel(true);
        dma_reset                = dma_claim_unused_channel(true);
        dma_trigger_start_mask  |= (1U<<dma_trigger_l);

        // setup PWM DMA channel LEFT
        dma_channel_config dma_cc_l_cfg = dma_channel_get_default_config(dma_cc_l);
        channel_config_set_transfer_data_size(&dma_cc_l_cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&dma_cc_l_cfg, false);
        channel_config_set_write_increment(&dma_cc_l_cfg, false);
        channel_config_set_chain_to(&dma_cc_l_cfg, dma_sample);               // Chain to increment DMA sample. This loads our next sample to &single_sample after we perform REPETITION_RATE num of transfers
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
        channel_config_set_chain_to(&dma_trigger_l_cfg, dma_reset);

        dma_channel_configure(dma_trigger_l,
                            &dma_trigger_l_cfg,
                            &dma_hw->ch[dma_cc_l].al3_read_addr_trig, // write to PWM DMA channel read address trigger
                            &single_sample_ptr,                       // read from location containing the address of single_sample
                            REPETITION_RATE * BUFFER_SIZE,            // trigger once per audio sample per repetition rate
                            false);

        // Don't worry about interrupt
        //dma_channel_set_irq1_enabled(dma_trigger_l, true); // fire interrupt when trigger DMA channel is done
        //// We do not set up our ISR for dma_trigger_l since they are in sync
        //irq_set_exclusive_handler(DMA_IRQ_1, _dma_handler);
        //irq_set_enabled(DMA_IRQ_1, true);

        // setup sample DMA channel
        dma_channel_config dma_sample_cfg = dma_channel_get_default_config(dma_sample);
        channel_config_set_transfer_data_size(&dma_sample_cfg, DMA_SIZE_32); // transfer 8-bits at a time
        channel_config_set_read_increment(&dma_sample_cfg, true);            // increment read address to go through audio buffer
        channel_config_set_write_increment(&dma_sample_cfg, false);          // always write to the same address
        channel_config_set_ring(&dma_sample_cfg, false, 9); // 1<<9 = 512. 512/4 = 128 buffer bytes

        dma_channel_configure(dma_sample,
                            &dma_sample_cfg,
                            &single_sample,       // write to single_sample. This contains the data for both CH A and CH B
                            &audio_buffer[0],     // read from audio buffer
                            1,                    // only do one transfer (once per PWM DMA completion due to chaining)
                            false                 // don't start yet
        );  

        // Set up a dummy transfer to just get looping
        dma_channel_config dma_reset_cfg = dma_channel_get_default_config(dma_reset);
        channel_config_set_transfer_data_size(&dma_reset_cfg, DMA_SIZE_32);
        channel_config_set_read_increment(&dma_reset_cfg, false); 
        channel_config_set_write_increment(&dma_reset_cfg, false); 

        dma_channel_configure(dma_reset,
                            &dma_reset_cfg, 
                            &dma_hw->multi_channel_trigger,
                            &dma_trigger_start_mask,
                            1, // only do one transfer
                            false
        );

        // clear audio buffer
        memset(audio_buffer, 0, BUFFER_SIZE);
        //memset(audio_buffers[1], 0, BUFFER_SIZE);
    }

    // ---- Channel B (optional, dual-channel LRA) ----
    if(_chan_b_enable)
    {
        gpio_set_function(_chan_b_pin, GPIO_FUNC_PWM);

        pwm_slice_r = pwm_gpio_to_slice_num(_chan_b_pin);
        pwm_init(pwm_slice_r, &config, false);

        pwm_enable_mask         |= (1U<<pwm_slice_r);
        dma_cc_r                 = dma_claim_unused_channel(true);
        dma_trigger_r            = dma_claim_unused_channel(true);
        dma_reset_r              = dma_claim_unused_channel(true);
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
    }

    pwm_set_mask_enabled(pwm_enable_mask);
    dma_start_channel_mask(dma_trigger_start_mask);

    return true;
}

// Get which half of the buffer is safe to write to
bool get_inactive_buffer_half() 
{
    // Get current DMA read address
    uint32_t current_trans_count = (dma_hw->ch[dma_trigger_l].transfer_count>>1);

    // If DMA is reading from first half, return pointer to second half
    if (current_trans_count >= PCM_BUFFER_SIZE) {
        return true;
    } else {
        return false;
    }
}

volatile bool _erm_simulation_enabled = false;
void lra_hal_task(uint64_t timestamp)
{
    static bool inactive_half = true;
    static uint32_t buffered[PCM_BUFFER_SIZE] = {0};

    bool inactive_this = get_inactive_buffer_half();

    if(inactive_this != inactive_half)
    {
        inactive_half = inactive_this;

        if(inactive_half)
            memcpy(&audio_buffer[PCM_BUFFER_SIZE], buffered, PCM_BUFFER_SIZE*sizeof(uint32_t));
            
        else 
            memcpy(&audio_buffer[0], buffered, PCM_BUFFER_SIZE*sizeof(uint32_t));

        pcm_generate_buffer(buffered);
    }
}

void lra_hal_push_amfm(haptic_packet_s *packet)
{
    _erm_simulation_enabled = false; // Unset our ERM simulation mode
    pcm_amfm_push(packet);
}

void lra_hal_set_standard(uint8_t intensity, bool brake)
{
    pcm_erm_set(intensity, brake); // Set ERM state with brake off
}

#endif // HOJA_HAPTICS_DRIVER == HAPTICS_DRIVER_LRA_HAL
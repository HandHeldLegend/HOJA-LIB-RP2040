#include "hal/hdrumble_hal.h"
#include "hal/gpio_hal.h"
#include "board_config.h"

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
#include "switch/switch_haptics.h"

#include "utilities/pcm.h"
#include "utilities/erm_simulator.h"

// Helper driver is specific to this HAL
#if defined(HOJA_HAPTIC_HELPER_DRIVER) && (HOJA_HAPTIC_HELPER_DRIVER==HAPTIC_HELPER_DRIVER_DRV2605L)
    #include "drivers/haptic/drv2605l.h"
#endif

#define SAMPLE_RATE         PCM_SAMPLE_RATE
#define REPETITION_RATE     4
#define BUFFER_SIZE         PCM_BUFFER_SIZE*2
#define PWM_WRAP            PCM_BUFFER_SIZE

uint dma_reset;

#if defined(HOJA_HDRUMBLE_CHAN_A_PIN)
    uint dma_cc_l, dma_trigger_l, dma_sample;
    uint pwm_slice_l;
#endif

#if defined(HOJA_HDRUMBLE_CHAN_B_PIN)
    #ifndef HOJA_HDRUMBLE_CHAN_A_PIN
        #error "To use channel B, HOJA_HDRUMBLE_CHAN_A_PIN must be defined too." 
    #endif 
    uint dma_cc_r, dma_trigger_r, dma_reset_r;
    uint pwm_slice_r;
#endif

uint32_t dma_trigger_start_mask = 0;
volatile uint32_t *dma_trigger_start_ptr = &dma_trigger_start_mask;

uint32_t single_sample;
uint32_t *single_sample_ptr = &single_sample;

// The current audio buffer in use
static volatile uint audio_buffer_idx = 0;
//static uint32_t audio_buffers[2][BUFFER_SIZE] = {0};

__attribute__((aligned(2048))) uint32_t audio_buffer[BUFFER_SIZE] = {0};
uint32_t *audio_buffer_ptr = &audio_buffer[0];

// Bool to indicate we need to fill our next sine wave
static volatile bool ready_next_sine = false;

static void __isr __time_critical_func(_dma_handler)()
{
    audio_buffer_idx = 1-audio_buffer_idx;
    #if defined(HOJA_HDRUMBLE_CHAN_A_PIN)
        //dma_hw->ch[dma_sample].al1_read_addr    = (intptr_t) &audio_buffers[audio_buffer_idx][0];
        dma_hw->ch[dma_trigger_l].al1_read_addr = (intptr_t) &single_sample_ptr;
    

        // Trigger right channel if we have one
        #ifdef HOJA_HDRUMBLE_CHAN_B_PIN
            dma_hw->ch[dma_trigger_r].al1_read_addr = (intptr_t) &single_sample_ptr;
        #endif
    
        dma_start_channel_mask(dma_trigger_start_mask);
        ready_next_sine = true;
        dma_hw->ints1 = 1u << dma_trigger_l;
    #endif
    uint8_t available_buffer = 1 - audio_buffer_idx;
}

bool hdrumble_hal_init()
{
    static bool hal_init = false;

    // Initialize the haptics
    switch_haptics_init(haptic_config->haptic_strength);

    // Initialize the PCM
    pcm_init();

    if(hal_init) return true;
    hal_init = true;

    // Optional init driver
    #if defined(HOJA_HAPTIC_HELPER_DRIVER_INIT)
    HOJA_HAPTIC_HELPER_DRIVER_INIT();
    #else 
        #warning "No HD haptics helper driver defined. Using HAL only."
    #endif

    // Initialize the PWM and DMA channels
    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    float clock_div = ( (float) f_clk_sys * 1000.0f) / PWM_WRAP / (float) SAMPLE_RATE / (float) REPETITION_RATE;

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_div);
    pwm_config_set_wrap(&config, PWM_WRAP);

    uint32_t pwm_enable_mask = 0;

    #ifdef HOJA_HDRUMBLE_CHAN_A_PIN
        dma_sample = dma_claim_unused_channel(true);
        gpio_set_function(HOJA_HDRUMBLE_CHAN_A_PIN, GPIO_FUNC_PWM);

        pwm_slice_l = pwm_gpio_to_slice_num(HOJA_HDRUMBLE_CHAN_A_PIN);
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
        channel_config_set_ring(&dma_sample_cfg, false, 11); // 1<<11 = 2048. 2048/4 = 512 buffer bytes

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
        
    #endif

    #ifdef HOJA_HDRUMBLE_CHAN_B_PIN
        #warning "HDRUMBLE CHANNEL B IS ENABLED"
        gpio_set_function(HOJA_HDRUMBLE_CHAN_B_PIN, GPIO_FUNC_PWM);

        pwm_slice_r = pwm_gpio_to_slice_num(HOJA_HDRUMBLE_CHAN_B_PIN);
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

        
    #endif

    pwm_set_mask_enabled(pwm_enable_mask);
    dma_start_channel_mask(dma_trigger_start_mask);

    return true;
}

// Get which half of the buffer is safe to write to
bool get_inactive_buffer_half() 
{
    #ifdef HOJA_HDRUMBLE_CHAN_A_PIN
    // Get current DMA read address
    uint32_t current_trans_count = (dma_hw->ch[dma_trigger_l].transfer_count>>2);
    
    // If DMA is reading from first half, return pointer to second half
    if (current_trans_count >= PCM_BUFFER_SIZE) {
        return true;
    } else {
        return false;
    }
    #else 
    return false;
    #endif
}

bool _erm_simulation_enabled = false;
void hdrumble_hal_task(uint32_t timestamp)
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

    /*
    if (ready_next_sine)
    {
        ready_next_sine = false;
        uint8_t available_buffer = 1 - audio_buffer_idx;
        pcm_generate_buffer(audio_buffers[available_buffer]);
    }*/

    if(_erm_simulation_enabled)
        erm_simulator_task(timestamp);
}

void hdrumble_hal_push_amfm(haptic_processed_s *input)
{
    _erm_simulation_enabled = false; // Unset our ERM simulation mode
    pcm_amfm_push(input);
}

void hdrumble_hal_set_standard(uint8_t intensity)
{
    _erm_simulation_enabled = true; // Enable ERM simulation mode
    erm_simulator_set_intensity(intensity);
}
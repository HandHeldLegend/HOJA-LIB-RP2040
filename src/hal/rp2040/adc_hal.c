#include "hal/adc_hal.h"

#include <math.h>
#include <stdint.h>

#include "hardware/adc.h"
#include "hardware/dma.h"

#include <stdlib.h>
#include <string.h>

#include "utilities/hooks.h"

#define SAMPLES_BIT_SHIFT   5
#define SAMPLES_PER_CHANNEL (1<<SAMPLES_BIT_SHIFT)

volatile uint16_t _dma_adc_buffer[SAMPLES_PER_CHANNEL];
uint16_t _current_adc_values[4];

static uint32_t _adc_dma_chan = 0;

adc_hal_cfg_t       _adchal_driver = {0};
bool                _adchal_initialized = false;

#define ADC_HAL_CHECK_RANGE(x, target) ((x > (target-8)) && (x < (target+8)))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))
// Fast median of 3 that avoids temp variables and branches
#define MEDIAN3(a,b,c) max(min(max(a,b), c), min(a,b))

uint16_t _process_samples(volatile uint16_t *source) 
{
    uint32_t sum = source[0] & 0xFFF;
    
    // Main processing loop (handles samples 1 to length-2)
    for (int i = 1; i < SAMPLES_PER_CHANNEL-1; i++) {
        uint16_t sample = source[i] & 0xFFF;
        sum += sample;
        continue;
        
        if (ADC_HAL_CHECK_RANGE(sample, 512) ||
            ADC_HAL_CHECK_RANGE(sample, 1536) ||
            ADC_HAL_CHECK_RANGE(sample, 2560) ||
            ADC_HAL_CHECK_RANGE(sample, 3584)) {
            // At transition point - take median
            sum += MEDIAN3(source[i-1] & 0xFFF, sample, source[i+1] & 0xFFF);
        } else {
            sum += sample;
        }
    }

    sum += source[SAMPLES_PER_CHANNEL-1] & 0xFFF;  // Handle last sample
    
    // Calculate 12-bit result
    uint16_t result = (uint16_t)(sum >> SAMPLES_BIT_SHIFT);
    return (result > 4095) ? 4095 : result;
}

static bool adc_init_done = false;

uint16_t adc_hal_read(uint8_t ch_local, uint8_t driver_instance)
{
    if(!adc_init_done || ch_local>3) return 0;

    adc_select_input(ch_local);
    dma_channel_set_write_addr(_adc_dma_chan, _dma_adc_buffer, true);
    adc_run(true);

    dma_channel_wait_for_finish_blocking(_adc_dma_chan);

    adc_run(false);
    adc_fifo_drain();

    _current_adc_values[ch_local] = _process_samples(_dma_adc_buffer);

    return _current_adc_values[ch_local];
}

bool _adc_hal_init(uint32_t gpio)
{
    if(gpio<26 || gpio>29) return false;

    uint8_t adc_gpio = gpio-26;

    if(!adc_init_done)
    {
        adc_init_done = true;

        adc_init();
        adc_fifo_setup(true, true, 1, false, false);

        _adc_dma_chan = dma_claim_unused_channel(true);
        dma_channel_config c = dma_channel_get_default_config(_adc_dma_chan);

        channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
        channel_config_set_read_increment(&c, false);
        channel_config_set_write_increment(&c, true);
        channel_config_set_dreq(&c, DREQ_ADC);
        
        // Configure but don't start DMA yet
        dma_channel_configure(
            _adc_dma_chan,
            &c,
            _dma_adc_buffer,        // Initial destination
            &adc_hw->fifo,          // Source
            SAMPLES_PER_CHANNEL,    // Count
            true // Start immediately
        );
    }

    adc_gpio_init(adc_gpio);
    return true;
}

bool adc_hal_register_driver(uint8_t ch_local, adc_driver_cfg_s *cfg)
{
    if(cfg->driver_instance<0) return false;
    if(cfg->driver_instance>0) return false; // Only 1 instance here

    int8_t instance = cfg->driver_instance;

    // Get associated driver
    adc_hal_cfg_t *driver   = &_adchal_driver;
    adc_hal_cfg_t *read     = &cfg->hal_cfg;

    // Initialize GPIO for HAL ADC
    uint32_t gpio = ch_local + 26;
    return _adc_hal_init(gpio);
}
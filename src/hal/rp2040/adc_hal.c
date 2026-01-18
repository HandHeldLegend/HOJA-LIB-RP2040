#include "hal/adc_hal.h"

#include <math.h>
#include <stdint.h>

#include "hardware/adc.h"
#include "hardware/dma.h"
#include "pico/multicore.h"

#include <stdlib.h>
#include <string.h>

#include "utilities/hooks.h"

#define SAMPLES_BIT_SHIFT   5
#define SAMPLES_PER_CHANNEL (1<<SAMPLES_BIT_SHIFT)

// DMA buffer only needed if not RP2350
#if !PICO_RP2350
volatile uint16_t _dma_adc_buffer[SAMPLES_PER_CHANNEL];
static uint32_t _adc_dma_chan = 0;
#endif

#define ADC_HAL_CHECK_RANGE(x, target) ((x > (target-8)) && (x < (target+8)))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))
#define MEDIAN3(a,b,c) max(min(max(a,b), c), min(a,b))

auto_init_mutex(_adc_safe_mutex); 

#if !PICO_RP2350
uint16_t _process_samples(volatile uint16_t *source) 
{
    uint32_t sum = 0;
    for (int i = 0; i < SAMPLES_PER_CHANNEL; i++) {
        uint16_t sample = source[i] & 0xFFF;
        sum += sample;
    }
    uint16_t result = (uint16_t)(sum >> SAMPLES_BIT_SHIFT);
    return (result > 4095) ? 4095 : result;
}
#endif

static bool adc_init_done = false;

// Read channel according to cfg data
bool adc_hal_read(adc_hal_driver_s *driver)
{
    if(!adc_init_done) return false;
    if(!driver->initialized) return false;

    mutex_enter_blocking(&_adc_safe_mutex);
    adc_select_input(driver->ch);

#if PICO_RP2350
    // Direct read for RP2350
    driver->output = adc_read();
#else
    // DMA workaround for RP2040
    dma_channel_set_write_addr(_adc_dma_chan, _dma_adc_buffer, true);
    adc_run(true);
    dma_channel_wait_for_finish_blocking(_adc_dma_chan);
    adc_run(false);
    adc_fifo_drain();
    driver->output = _process_samples(_dma_adc_buffer);
#endif

    mutex_exit(&_adc_safe_mutex);
    return true;
}

bool _adc_hal_init(uint32_t gpio)
{
    #if PICO_RP2350
    if(gpio>39) { if(gpio>47) return false; }
    else if(gpio<26 || gpio>29) return false;
    #else
    if(gpio<26 || gpio>29) return false;
    #endif

    mutex_enter_blocking(&_adc_safe_mutex);

    if(!adc_init_done)
    {
        adc_init_done = true;
        adc_init();

#if !PICO_RP2350
        // Only configure FIFO and DMA if we are NOT on RP2350
        adc_fifo_setup(true, true, 1, false, false);

        _adc_dma_chan = dma_claim_unused_channel(true);
        dma_channel_config c = dma_channel_get_default_config(_adc_dma_chan);

        channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
        channel_config_set_read_increment(&c, false);
        channel_config_set_write_increment(&c, true);
        channel_config_set_dreq(&c, DREQ_ADC);
        
        dma_channel_configure(
            _adc_dma_chan,
            &c,
            _dma_adc_buffer,
            &adc_hw->fifo,
            SAMPLES_PER_CHANNEL,
            false // Don't start yet
        );
#endif
    }

    adc_gpio_init(gpio);
    mutex_exit(&_adc_safe_mutex);
    return true;
}

bool adc_hal_init(adc_hal_driver_s *driver)
{
    uint8_t gpio = driver->gpio;
    uint8_t gpio_base = 26;
    #if PICO_RP2350
    if(gpio>39)
    {
        gpio_base = 40;
        if(gpio>47) return false;
    }
    else if(gpio<26 || gpio>29) return false;
    #else
    if(gpio<26 || gpio>29) return false;
    #endif

    if(!_adc_hal_init(gpio)) return false;
    
    driver->ch = gpio-gpio_base;
    driver->initialized = true;
    return true;
}
#include "hal/adc_hal.h"

#include "hardware/adc.h"
#include "hardware/dma.h"

#include <stdlib.h>
#include <string.h>

#include "utilities/hooks.h"

#define SAMPLES_BIT_SHIFT   6
#define SAMPLES_PER_CHANNEL (1<<SAMPLES_BIT_SHIFT)

#if HOJA_RP2040_ADC_COUNT > 0
uint16_t _dma_adc_buffer[SAMPLES_PER_CHANNEL];
uint16_t _safe_adc_buffer[SAMPLES_PER_CHANNEL];
volatile uint16_t _current_adc_values[HOJA_RP2040_ADC_COUNT];

// Store our ADC channels
uint8_t _current_adc_reading_channel = 0;
int8_t  _current_adc_channels[HOJA_RP2040_ADC_COUNT];
uint8_t _current_init_channel_idx = 0;

static uint32_t _adc_dma_chan;
static uint8_t  _adc_reading_chan;

uint16_t _process_samples(uint16_t *source) 
{
    memcpy(_safe_adc_buffer, source, SAMPLES_PER_CHANNEL);
    uint32_t sum = 0;
    
    for (int i = 0; i < SAMPLES_PER_CHANNEL; i++) {
        uint16_t sample = _safe_adc_buffer[i];
        
        // Check for problematic transition points
        if (abs(sample - 512) < 8 || abs(sample - 1536) < 8 ||
            abs(sample - 2560) < 8 || abs(sample - 3584) < 8) {
            // Take median of nearby samples
            uint16_t nearby[3];
            nearby[0] = (i > 0) ? _safe_adc_buffer[i-1] : sample;
            nearby[1] = sample;
            nearby[2] = (i < SAMPLES_PER_CHANNEL-1) ? _safe_adc_buffer[i+1] : sample;
            
            // Simple median of 3
            if (nearby[0] > nearby[1]) {
                uint16_t temp = nearby[0];
                nearby[0] = nearby[1];
                nearby[1] = temp;
            }
            if (nearby[1] > nearby[2]) {
                uint16_t temp = nearby[1];
                nearby[1] = nearby[2];
                nearby[2] = temp;
            }
            if (nearby[0] > nearby[1]) {
                uint16_t temp = nearby[0];
                nearby[0] = nearby[1];
                nearby[1] = temp;
            }
            sum += nearby[1];
        } else {
            sum += sample;
        }
    }
    
    // Calculate 12-bit result
    uint16_t result = (uint16_t)((sum >> SAMPLES_BIT_SHIFT ));
    return (result > 4095) ? 4095 : result;
}

void _adc_start_read(uint8_t channel)
{
    adc_select_input(channel);
    dma_channel_set_write_addr(_adc_dma_chan, _dma_adc_buffer, false);
    dma_channel_start(_adc_dma_chan);
}

void _adc_hal_task()
{
    if(!dma_channel_is_busy(_adc_dma_chan))
    {
        _current_adc_values[_current_adc_reading_channel] = _process_samples(_dma_adc_buffer);
        _current_adc_reading_channel = (_current_adc_reading_channel + 1) % HOJA_RP2040_ADC_COUNT;
        _adc_start_read(_current_adc_reading_channel);
    }
}

uint16_t adc_hal_read(uint8_t gpio, bool invert)
{
    if((gpio < 26) || (gpio > 29)) return 0; // Invalid ADC gpio on rp2040
    uint8_t adc_channel = gpio-26;
    return _current_adc_values[adc_channel];
}

bool adc_hal_init(uint8_t channel, uint32_t gpio)
{
    if((gpio < 26) || (gpio > 29)) return false; // Invalid ADC gpio on rp2040

    static bool adc_init_done = false;
    if(!adc_init_done)
    {
        adc_init();

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
            false                   // Don't start yet
        );

        // Register ADC hook stask on core 1
        HOOK_REGISTER(_adc_hal_task, 1);
    }

    uint8_t adc_channel = gpio-26;
    adc_gpio_init(gpio);
    _current_adc_channels[_current_init_channel_idx] = adc_channel;
    _current_init_channel_idx++;

    if(!adc_init_done)
    {
        _adc_start_read(_current_adc_reading_channel);
        adc_init_done = true;
    }

    return true;
}
#endif
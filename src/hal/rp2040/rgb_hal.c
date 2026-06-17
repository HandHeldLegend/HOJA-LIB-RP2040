#include "hal/rgb_hal.h"

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "generated/ws2812.pio.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER == RGB_DRIVER_HAL)

// PIO block + state machine are allocated dynamically at init.
static PIO _rgb_pio = NULL;
int _rgb_state_machine = -1;
int _rgb_dma_chan;
uint32_t _rgb_states[RGB_DRIVER_LED_COUNT] = {0};

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

    ws2812_program_init(_rgb_pio, sm, offset, RGB_DRIVER_OUTPUT_PIN);
    rgb_hal_update(NULL);
}

void rgb_hal_deinit()
{
    
}


// Update all RGBs
void rgb_hal_update(rgb_s *data)
{
    if(_rgb_state_machine < 0) return;

    if(data != NULL)
        memcpy(_rgb_states, data, sizeof(uint32_t) * RGB_DRIVER_LED_COUNT);

    dma_channel_transfer_from_buffer_now(_rgb_dma_chan, _rgb_states, RGB_DRIVER_LED_COUNT);
}

#endif
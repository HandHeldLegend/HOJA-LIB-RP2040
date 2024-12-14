#include "hal/rgb_hal.h"

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "generated/ws2812.pio.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER == RGB_DRIVER_HAL)

#if !defined(RGB_DRIVER_PIO_INSTANCE)
    #error "RGB_DRIVER_PIO_INSTANCE undefined in board_config.h" 
#endif 

int _rgb_state_machine = -1;

void rgb_hal_init()
{
    _rgb_state_machine = pio_claim_unused_sm(RGB_DRIVER_PIO_INSTANCE, true);
    uint offset = pio_add_program(RGB_DRIVER_PIO_INSTANCE, &ws2812_program);
    ws2812_program_init(RGB_DRIVER_PIO_INSTANCE, _rgb_state_machine, offset, RGB_DRIVER_OUTPUT_PIN);
    rgb_hal_update(NULL);
}

void rgb_hal_deinit()
{
    
}

uint32_t _rgb_states[RGB_DRIVER_COUNT] = {0};

// Update all RGBs
void rgb_hal_update(uint32_t *data)
{
    if(_rgb_state_machine < 0) return;

    if(data != NULL)
        memcpy(_rgb_states, data, sizeof(uint32_t) * RGB_DRIVER_COUNT);

    for (uint8_t i = 0; i < RGB_DRIVER_COUNT; i++)
    {
        pio_sm_put_blocking(RGB_DRIVER_PIO_INSTANCE, (uint) _rgb_state_machine, _rgb_states[i]);
    }
}

#endif
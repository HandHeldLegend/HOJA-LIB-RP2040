#include "hal/nesbus_hal.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "generated/nserial.pio.h"
#include "input_shared_types.h"
#include "hoja.h"

#include "input/analog.h"
#include "input/button.h"

#include "board_config.h"

#if defined(HOJA_NESBUS_DRIVER) && (HOJA_NESBUS_DRIVER == NESBUS_DRIVER_HAL)
#if !defined(NESBUS_DRIVER_PIO_INSTANCE)
    #error "NESBUS_DRIVER_PIO_INSTANCE must be defined in board_config.h" 
#endif

#if !defined(NESBUS_DRIVER_CLOCK_PIN)
    #error "NESBUS_DRIVER_CLOCK_PIN must be defined in board_config.h" 
#endif

#if !defined(NESBUS_DRIVER_DATA_PIN)
    #error "NESBUS_DRIVER_DATA_PIN must be defined in board_config.h" 
#endif

#if !defined(NESBUS_DRIVER_LATCH_PIN)
    #error "NESBUS_DRIVER_LATCH_PIN must be defined in board_config.h" 
#endif

#if(NESBUS_DRIVER_PIO_INSTANCE==0)
    #define PIO_IN_USE pio0
    #define PIO_IRQ_USE_0 PIO0_IRQ_0
#else 
    #define PIO_IN_USE pio1 
    #define PIO_IRQ_USE_0 PIO1_IRQ_0
#endif

typedef struct
{
    union
    {
        struct
        {
            uint8_t padding : 4;
            uint8_t r   : 1;
            uint8_t l   : 1;
            uint8_t x   : 1;
            uint8_t a   : 1;
            uint8_t dright  : 1;
            uint8_t dleft   : 1;
            uint8_t ddown   : 1;
            uint8_t dup     : 1;
            uint8_t start   : 1;
            uint8_t select  : 1;
            uint8_t y : 1;
            uint8_t b : 1;
        };
        uint16_t value;
    };
} nesbus_hal_input_s;

#define INPUT_POLL_RATE 1000 // 1ms

static bool _nspi_running;
static uint _nspi_irq;
static PIO  _nspi_pio;
static uint _nspi_sm;
static uint _nspi_offset;
static volatile uint32_t _nspi_buffer = 0xFFFFFFFF;

volatile bool _nspi_clear = true;

static void _nspi_isr_handler(void)
{
  if (pio_interrupt_get(PIO_IN_USE, 0))
  {
    pio_interrupt_clear(PIO_IN_USE, 0);

    pio_sm_drain_tx_fifo(_nspi_pio, _nspi_sm);
    pio_sm_exec_wait_blocking(_nspi_pio, _nspi_sm, pio_encode_jmp(_nspi_offset + nserial_offset_startserial));
    pio_sm_put_blocking(_nspi_pio, _nspi_sm, _nspi_buffer);
    _nspi_clear = true;
  }
}

void nesbus_hal_stop()
{

}

bool nesbus_hal_init()
{
    // Set up PIO for NSPI
    _nspi_pio = PIO_IN_USE;
    _nspi_irq = PIO_IRQ_USE_0;
    _nspi_sm = 0;
    _nspi_offset = pio_add_program(_nspi_pio, &nserial_program);

    pio_set_irq0_source_enabled(PIO_IN_USE, pis_interrupt0, true);
    irq_set_exclusive_handler(_nspi_irq, _nspi_isr_handler);

    nserial_program_init(_nspi_pio, _nspi_sm, _nspi_offset, NESBUS_DRIVER_DATA_PIN, NESBUS_DRIVER_CLOCK_PIN);
    nserial_program_latch_init(_nspi_pio, _nspi_sm+1, _nspi_offset, NESBUS_DRIVER_LATCH_PIN);

    pio_sm_put_blocking(_nspi_pio, _nspi_sm, 0xFFFFFFFF);

    irq_set_enabled(_nspi_irq, true);
    _nspi_running = true;
}

void nesbus_hal_task(uint64_t timestamp)
{   
    static nesbus_hal_input_s   buffer  = {.value = 0xFFFF};
    static interval_s interval = {0};

    if(!_nspi_running)
    {
        return;
    }
    else
    {
        // Update NESBus data at our input poll rate
        if(interval_run(timestamp, INPUT_POLL_RATE, &interval))
        {
            hoja_set_connected_status(CONN_STATUS_PLAYER_1);

            if(_nspi_clear)
            {
                _nspi_buffer = buffer.value<<16;
                _nspi_clear = false;
            }
        }
    }
}

#endif
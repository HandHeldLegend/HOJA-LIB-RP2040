#include "hal/nesbus_hal.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "generated/nserial.pio.h"
#include "input_shared_types.h"
#include "hoja.h"

#include "input/mapper.h"
#include "transport/transport_nesbus.h"

#include "board_config.h"

#if defined(HOJA_TRANSPORT_NESBUS_DRIVER) && (HOJA_TRANSPORT_NESBUS_DRIVER == NESBUS_DRIVER_HAL)

// PIO block + GPIO pins come from hoja_config_s.nesbus, resolved at init.
#define PIO_IN_USE _nspi_pio

typedef struct
{
    union
    {
        struct
        {
            uint32_t padding : 20;
            uint32_t r   : 1;
            uint32_t l   : 1;
            uint32_t x   : 1;
            uint32_t a   : 1;
            uint32_t dright  : 1;
            uint32_t dleft   : 1;
            uint32_t ddown   : 1;
            uint32_t dup     : 1;
            uint32_t start   : 1;
            uint32_t select  : 1;
            uint32_t y : 1;
            uint32_t b : 1;
        };
        uint32_t value;
    };
} snes_hal_input_s;

typedef struct
{
    union
    {
        struct
        {
            uint32_t padding : 24;
            uint32_t dright  : 1;
            uint32_t dleft   : 1;
            uint32_t ddown   : 1;
            uint32_t dup     : 1;
            uint32_t start   : 1;
            uint32_t select  : 1;
            uint32_t b : 1;
            uint32_t a : 1;
        };
        uint32_t value;
    };
} nes_hal_input_s;

volatile nes_hal_input_s tmpnes  = {.value = 0};
volatile snes_hal_input_s tmpsnes  = {.value = 0xFFFF0000};

#define INPUT_POLL_RATE 1000 // 1ms

static bool _nspi_running;
static uint _nspi_irq;
static PIO  _nspi_pio;
static uint _nspi_sm;       // data shift SM
static uint _nspi_latch_sm; // latch-detect SM (same PIO + program)
static uint _nspi_offset;
static volatile uint32_t _nspi_buffer = 0xFFFFFFFF;
static bool _nspi_snesdetected = false;

volatile bool _nspi_clear = true;


static void _nspi_isr_handler(void)
{
    
    if (pio_interrupt_get(PIO_IN_USE, 0))
    {
        pio_interrupt_clear(PIO_IN_USE, 0);
        pio_sm_exec_wait_blocking(_nspi_pio, _nspi_sm, pio_encode_jmp(_nspi_offset + nserial_offset_dumpy));

        uint32_t count = 31 - pio_sm_get_blocking(PIO_IN_USE, _nspi_sm);
        if(count == 31) 
        {
            // Do nothing
        }
        else if(count >= 12) 
        {
            _nspi_snesdetected = true;
        }
        _nspi_clear = true;
    }
}

void transport_nesbus_stop()
{

}

bool transport_nesbus_init(core_params_s *params)
{
    if(params->core_report_format != CORE_REPORTFORMAT_SNES) return false;

    const hoja_nesbus_cfg_s *cfg = &hoja_config_get()->nesbus;

    // Dynamically grab a PIO block + state machine (data shifter) with room for
    // the program, then claim a second SM on the same block for latch detection.
    if(!pio_claim_free_sm_and_add_program(&nserial_program, &_nspi_pio, &_nspi_sm, &_nspi_offset))
        return false;

    int latch_sm = pio_claim_unused_sm(_nspi_pio, false);
    if(latch_sm < 0)
    {
        pio_remove_program_and_unclaim_sm(&nserial_program, _nspi_pio, _nspi_sm, _nspi_offset);
        return false;
    }
    _nspi_latch_sm = (uint)latch_sm;

    // IRQ line depends on which PIO block we were given.
    _nspi_irq = (uint)pio_get_irq_num(_nspi_pio, 0);

    pio_set_irq0_source_enabled(PIO_IN_USE, pis_interrupt0, true);
    irq_set_exclusive_handler(_nspi_irq, _nspi_isr_handler);

    nserial_program_init(_nspi_pio, _nspi_sm, _nspi_offset, cfg->data_pin, cfg->clock_pin);
    nserial_program_latch_init(_nspi_pio, _nspi_latch_sm, _nspi_offset, cfg->latch_pin);

    pio_sm_put_blocking(_nspi_pio, _nspi_sm, 0x00);

    irq_set_enabled(_nspi_irq, true);
    _nspi_running = true;
    return true;
}

void transport_nesbus_task(uint64_t timestamp)
{   
    
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
            mapper_input_s input = mapper_get_input();

            static uint32_t pack = 0;

            bool dpad[4] = {input.presses[SNES_CODE_DOWN], input.presses[SNES_CODE_RIGHT],
                    input.presses[SNES_CODE_LEFT], input.presses[SNES_CODE_UP]};

            dpad_translate_input(dpad);

            if(_nspi_snesdetected)
            {
                tmpsnes.a = !input.presses[SNES_CODE_A];
                tmpsnes.b = !input.presses[SNES_CODE_B];
                tmpsnes.x = !input.presses[SNES_CODE_X];
                tmpsnes.y = !input.presses[SNES_CODE_Y];

                tmpsnes.l = !input.presses[SNES_CODE_L];
                tmpsnes.r = !input.presses[SNES_CODE_R];

                tmpsnes.start = !input.presses[SNES_CODE_START];
                tmpsnes.select = !input.presses[SNES_CODE_SELECT];

                tmpsnes.ddown  = !dpad[0];
                tmpsnes.dright = !dpad[1];
                tmpsnes.dleft  = !dpad[2];
                tmpsnes.dup    = !dpad[3];

                pack = tmpsnes.value;
            }
            else
            {
                tmpnes.a = !input.presses[SNES_CODE_A];
                tmpnes.b = !input.presses[SNES_CODE_B];

                tmpnes.start = !input.presses[SNES_CODE_START];
                tmpnes.select = !input.presses[SNES_CODE_SELECT];

                tmpnes.ddown  = !dpad[0];
                tmpnes.dright = !dpad[1];
                tmpnes.dleft  = !dpad[2];
                tmpnes.dup    = !dpad[3];

                pack = tmpnes.value;
            }

            if(_nspi_clear)
            {
                _nspi_buffer = pack;
                pio_sm_drain_tx_fifo(_nspi_pio, _nspi_sm);
                pio_sm_put_blocking(_nspi_pio, _nspi_sm, _nspi_buffer);
                _nspi_clear = false;
            }
        }
    }
}

#endif
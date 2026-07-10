#include "hal/nesbus_hal.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "generated/nesbus.pio.h"
#include "input_shared_types.h"
#include "hoja.h"

#include "input/mapper.h"
#include "transport/transport_nesbus.h"

#include "utilities/crosscore_utils.h"
#include "utilities/interval.h"

#include "transport/transport.h"
#include "devices/battery.h"

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
            uint32_t padding : 16;
            uint32_t blank : 4;
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

nesbus_pio_sm_s k_nesbus_sm = {0};

#define INPUT_POLL_RATE 1000 // 1ms

HOJA_CROSSCORE_SNAPSHOT_TYPE(nesbus, uint32_t);
hoja_snapshot_nesbus_t _ss_nesbus;

volatile uint32_t _nesbus_write_data = 0xFFFFFFFF;

bool _nesbus_snesmode = true;

// --- Connection lifecycle -------------------------------------------------
// When the console reads the controller (latch ISR) we report player 1 +
// connected. After 4s with no read we drop to player 0 + disconnected, and
// after 8s we power the device off (on boards with a ship-mode capable PMIC).
#define NESBUS_DISCONNECT_US (4u * 1000u * 1000u)
#define NESBUS_SHUTDOWN_US   (8u * 1000u * 1000u)

// Set in the latch ISR whenever the console reads the controller. Consumed by
// the task as a connection heartbeat.
volatile bool _nesbus_polled = false;

static uint64_t _nesbus_last_poll_us = 0;
static bool     _nesbus_timer_started = false;
static bool     _nesbus_connected = false;

static void _nesbus_set_connected(bool connected)
{
    if (connected == _nesbus_connected)
        return;

    _nesbus_connected = connected;

    tp_evt_s conn = {
        .evt = TP_EVT_CONNECTIONCHANGE,
        .evt_connectionchange = {
            .connection = connected ? TP_CONNECTION_CONNECTED : TP_CONNECTION_DISCONNECTED}};
    transport_evt_cb(conn);

    tp_evt_s player = {
        .evt = TP_EVT_PLAYERLED,
        .evt_playernumber = {.player_number = connected ? 1 : 0}};
    transport_evt_cb(player);
}

void _nesbus_set_inputbits(uint32_t bitcount)
{
    if(bitcount<20 && bitcount > 0) _nesbus_snesmode = true;
    else _nesbus_snesmode = false;
}

static void _nesbus_isr_handler(void)
{
  if (pio_interrupt_get(k_nesbus_sm.pio, 0))
  {
    pio_sm_set_enabled(k_nesbus_sm.pio, k_nesbus_sm.sm_data, false);
    pio_sm_exec(k_nesbus_sm.pio, k_nesbus_sm.sm_data, k_nesbus_sm.movy_instr_data);
    pio_sm_exec(k_nesbus_sm.pio, k_nesbus_sm.sm_data, k_nesbus_sm.push_instr_data);
    pio_sm_exec(k_nesbus_sm.pio, k_nesbus_sm.sm_data, k_nesbus_sm.sety_instr_data);
    pio_sm_exec(k_nesbus_sm.pio, k_nesbus_sm.sm_data, k_nesbus_sm.jump_instr_data);
    pio_sm_set_enabled(k_nesbus_sm.pio, k_nesbus_sm.sm_data, true);
    pio_interrupt_clear(k_nesbus_sm.pio, 0);
    _nesbus_set_inputbits(pio_sm_get_blocking(k_nesbus_sm.pio, k_nesbus_sm.sm_data));
    _nesbus_polled = true; // Console polled us -> connection heartbeat
  }
}

void transport_nesbus_stop()
{
}

static snes_hal_input_s tmpsnes  = {.value = 0xFFFFFFFF};
static nes_hal_input_s tmpnes  = {.value = 0xFFFFFFFF};

bool transport_nesbus_init(core_params_s *params)
{
    if(params->core_report_format != CORE_REPORTFORMAT_SNES) return false;

    const hoja_nesbus_cfg_s *cfg = &hoja_config_get()->nesbus;
    tmpsnes.padding = 0;
    tmpnes.padding = 0;

    _nesbus_polled = false;
    _nesbus_last_poll_us = 0;
    _nesbus_timer_started = false;
    _nesbus_connected = false;

    nesbus_pio_init(&k_nesbus_sm, _nesbus_isr_handler, cfg->latch_pin, cfg->data_pin, cfg->clock_pin);
    return true;
}

void transport_nesbus_task(uint64_t timestamp)
{   
    
    static interval_s interval = {0};

    if(k_nesbus_sm.running)
    {
        // Update NESBus data at our input poll rate
        if(interval_run(timestamp, INPUT_POLL_RATE, &interval))
        {
            static mapper_input_s input = {0};
            input = mapper_get_input();

            static uint32_t pack = 0;

            bool dpad[4] = {input.presses[SNES_CODE_DOWN], input.presses[SNES_CODE_RIGHT],
                    input.presses[SNES_CODE_LEFT], input.presses[SNES_CODE_UP]};

            dpad_translate_input(dpad);

            if(_nesbus_snesmode)
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
                _nesbus_write_data = tmpsnes.value;
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
                _nesbus_write_data = tmpnes.value;
            }
        }
        
        //pio_sm_drain_tx_fifo(k_nesbus_sm.pio, k_nesbus_sm.sm_data);
        if(pio_sm_is_tx_fifo_empty(k_nesbus_sm.pio, k_nesbus_sm.sm_data))
        {
            pio_sm_put(k_nesbus_sm.pio, k_nesbus_sm.sm_data, _nesbus_write_data);
        }
    }

    // Console read = connection heartbeat.
    if(_nesbus_polled)
    {
        _nesbus_polled = false;
        _nesbus_last_poll_us = timestamp;
        _nesbus_timer_started = true;
        _nesbus_set_connected(true);
    }

    // Seed the timeout window from the first tick so a controller that is never
    // read (e.g. plugged into a powered-off console) still shuts down on time.
    if(!_nesbus_timer_started)
    {
        _nesbus_timer_started = true;
        _nesbus_last_poll_us = timestamp;
        return;
    }

    uint64_t elapsed = timestamp - _nesbus_last_poll_us;

    if(_nesbus_connected && (elapsed >= NESBUS_DISCONNECT_US))
    {
        _nesbus_set_connected(false);
    }

    // Only power off where ship mode can actually cut power; otherwise
    // hoja_shutdown() would reboot-loop (e.g. dev boards powered over USB).
    if((elapsed >= NESBUS_SHUTDOWN_US) && battery_has_driver())
    {
        hoja_deinit(hoja_shutdown);
    }
}

#endif
#include "board_config.h"

#if defined(HOJA_TRANSPORT_JOYBUS64_DRIVER) && (HOJA_TRANSPORT_JOYBUS64_DRIVER == JOYBUS_N64_DRIVER_HAL)

#include "cores/core_n64.h"
#include "transport/transport.h"
#include "transport/transport_joybus64.h"

#include "hal/joybus_n64_hal.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "generated/joybus.pio.h"

#include "utilities/n64_crc.h"
#include "utilities/interval.h"
#include "utilities/crosscore_utils.h"

#include "devices/battery.h"

#include "hoja.h"
#include "hoja_bsp.h"

typedef enum
{
    N64_CMD_PROBE   = 0x00,
    N64_CMD_POLL    = 0x01,
    N64_CMD_READMEM = 0x02,
    N64_CMD_WRITEMEM = 0x03,
    N64_CMD_RESET = 0xFF
} n64_cmd_t;

SNAPSHOT_TYPE(n64input, core_n64_report_s);
snapshot_n64input_t _n64_hal_snap;

// The PIO block + state machine are allocated dynamically at init (see
// pio_claim_free_sm_and_add_program); the data pin comes from
// hoja_config_s.joybus, shared with the GameCube joybus HAL. Cached into these
// statics so the time-critical handlers stay branch-free.
static PIO     _n64_pio       = NULL;
static uint    _n64_sm        = 0;
static uint8_t _n64_data_pin  = 0;
#define PIO_SM _n64_sm

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))
#define ALIGNED_JOYBUS_8(val) ((val) << 24)
#define N64_RANGE 90
#define N64_RANGE_MULTIPLIER (N64_RANGE)/4096

uint _n64_irq;
uint _n64_offset;
pio_sm_config _n64_c;

#define PIO_IN_USE_N64 _n64_pio
bool _n64_rumble = false;

volatile static uint8_t _workingCmd = 0x00;
volatile static uint8_t _byteCount = 0;
volatile uint8_t _crc_reply = 0;

volatile bool   _n64_sent_poll = false;
bool            _n64_running = false;

uint8_t _n64_calculate_crc(const uint8_t *data, size_t len, uint8_t init_value, bool pak_inserted) {
    uint8_t crc = init_value;

    for (size_t i = 0; i < len; ++i) {
        crc = n64_crc[(crc ^ data[i])];
    }

    return crc ^ ((pak_inserted) ? 0xFF : 0x00);
}

uint8_t _n64_iterate_crc(uint8_t crc, uint8_t input) {
    uint8_t out = n64_crc[(crc ^ input)];
    return out;
}

void _n64_send_rumble_identify()
{
  for(uint i = 0; i<32; i++)
  {
    pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(0x80));
  }
  pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(0xB8)); // Precomputed CRC (0x47 or also try 0xB8)
}

void _n64_send_null_identify()
{
  for(uint i = 0; i<33; i++)
  {
    pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, 0);
  }
}

void _n64_send_probe()
{
    pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(0x05));
    pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, 0);
    pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(0x01)); // Indicate PAK is installed
}

void _n64_send_pak_write()
{
    pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(_crc_reply));
}

void _n64_send_poll()
{
  static core_n64_report_s out;
  snapshot_n64input_read(&_n64_hal_snap, &out);
  pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(out.buttons_1));
  pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(out.buttons_2));
  pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(out.stick_x));
  pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(out.stick_y));
  _n64_sent_poll = true; // Set flag for auto-init
}

#define PAK_MSG_BYTES 33

// Constants for default cycles and clock speeds
// 1 cycle is about 0.05us delay time

#define DEFAULT_CYCLES         100
#define DEFAULT_CLOCK_KHZ      125000 // 125 MHz
#define NEW_CLOCK_KHZ          (HOJA_BSP_CLOCK_SPEED_HZ/1000)
const uint32_t _delay_cycles_memread = 120;
const uint32_t _delay_cycles_memwrite = 100;
const uint32_t _delay_cycles_probe = 100;
const uint32_t _delay_cycles_poll = 120;
uint8_t _n64_hal_in_buffer[64] = {0};

void __time_critical_func(_n64_command_handler)()
{
    uint32_t c = DEFAULT_CYCLES;
    
    // Resume working on commands that are longer than 1 byte
    if (_workingCmd == N64_CMD_WRITEMEM)
    {
      uint16_t writeaddr = 0;
      _n64_hal_in_buffer[_byteCount] = pio_sm_get(PIO_IN_USE_N64, PIO_SM);

      if(_byteCount>1) // Only 32 bytes after the address
      _crc_reply = _n64_iterate_crc(_crc_reply, _n64_hal_in_buffer[_byteCount]);
      
      if (_byteCount >= PAK_MSG_BYTES)
      {
        writeaddr = (_n64_hal_in_buffer[0] << 8) | (_n64_hal_in_buffer[1]&0xE0);
        if (writeaddr==0xC000)
        {
          _n64_rumble = (_n64_hal_in_buffer[2] > 0) ? true : false;
        }
        _workingCmd = 0;
        _byteCount = 0;

        // WRITE 
        // 200 delay cycles = ~12us
        // 100 delay cycles = ~7.5us
        c = _delay_cycles_memwrite;
        while (c--)
            asm("nop");

        joybus_jump_output(PIO_IN_USE_N64, PIO_SM, _n64_offset);
        _n64_send_pak_write();
      } 
      else _byteCount++;
      
    }
    else if (_workingCmd == N64_CMD_READMEM)
    {
      
      _n64_hal_in_buffer[_byteCount] = pio_sm_get(PIO_IN_USE_N64, PIO_SM);

      if (_byteCount >= 1)
      {
        _workingCmd = 0;
        _byteCount = 0;

        joybus_jump_output(PIO_IN_USE_N64, PIO_SM, _n64_offset);

        // End receive so we respond
        c = _delay_cycles_memread;
        while(c--)
          asm("nop");
        
        uint16_t readaddr = (_n64_hal_in_buffer[0]<<8) | (_n64_hal_in_buffer[1]&0xE0);
        if(readaddr==0x8000)
        {
          _n64_send_rumble_identify();
        }
        else _n64_send_null_identify();
      }
      else _byteCount++;

    }
    // Single byte commands and setup
    // for future handling
    else
    {
        _workingCmd = pio_sm_get(PIO_IN_USE_N64, PIO_SM);

        switch (_workingCmd)
        {
        default:
            break;

        // Read from mem pak
        case N64_CMD_READMEM:
            _crc_reply = 0;
            break;

        // Write to mem pak
        case N64_CMD_WRITEMEM:
            _crc_reply = 0;
            break;

        // Probe/Reset target response time 3us
        case N64_CMD_RESET:
        case N64_CMD_PROBE:
            c = _delay_cycles_probe;
            while (c--)
                asm("nop");
            joybus_jump_output(PIO_IN_USE_N64, PIO_SM, _n64_offset);
            _n64_send_probe();
            _workingCmd = 0;
            break;

        // Poll
        case N64_CMD_POLL:
            c = _delay_cycles_poll;
            while (c--)
                asm("nop");
            joybus_jump_output(PIO_IN_USE_N64, PIO_SM, _n64_offset);
            _n64_send_poll();
            _workingCmd = 0;
            break;
        }
    }
}

static void __time_critical_func(_n64_isr_handler)(void)
{
  irq_set_enabled(_n64_irq, false);
  if (pio_interrupt_get(PIO_IN_USE_N64, 0))
  {
    pio_interrupt_clear(PIO_IN_USE_N64, 0);
    _n64_command_handler();
  }
  irq_set_enabled(_n64_irq, true);
}

void _n64_reset_state()
{
  joybus_program_init(PIO_IN_USE_N64, PIO_SM, _n64_offset, _n64_data_pin, &_n64_c);
}

bool _joybus_n64_hal_init()
{
    _n64_data_pin = hoja_config_get()->joybus.data_pin;

    // Dynamically grab a PIO block + state machine with room for the program.
    if(!pio_claim_free_sm_and_add_program(&joybus_program, &_n64_pio, &_n64_sm, &_n64_offset))
        return false;

    // IRQ line depends on which PIO block we were given.
    _n64_irq = (uint)pio_get_irq_num(_n64_pio, 0);

    pio_set_irq0_source_enabled(PIO_IN_USE_N64, pis_interrupt0, true);

    irq_set_exclusive_handler(_n64_irq, _n64_isr_handler);

    irq_set_priority(_n64_irq, 0);

    joybus_program_init(PIO_IN_USE_N64, PIO_SM, _n64_offset, _n64_data_pin, &_n64_c);
    irq_set_enabled(_n64_irq, true);
    return true;
}

core_params_s *_n64_hal_params = NULL;
transport_autoinit_sm_s *_n64_hal_tpsm = NULL;

// --- Connection lifecycle -------------------------------------------------
// A console poll reports player 1 + connected. After 4s with no poll we drop to
// player 0 + disconnected, and after 8s we power the device off (on boards with
// a ship-mode capable PMIC).
#define N64_DISCONNECT_US (4u * 1000u * 1000u)
#define N64_SHUTDOWN_US   (8u * 1000u * 1000u)

static uint64_t _n64_last_poll_us  = 0;
static bool     _n64_timer_started = false;
static bool     _n64_connected     = false;

static void _n64_set_connected(bool connected)
{
  if(connected == _n64_connected) return;

  _n64_connected = connected;

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

/***********************************************/
/********* Transport Defines *******************/
void transport_jb64_stop()
{
  if (!_n64_hal_params) return;

  // Disable and clear the IRQ
  irq_set_enabled(_n64_irq, false);
  irq_remove_handler(_n64_irq, _n64_isr_handler);
  pio_set_irq0_source_enabled(PIO_IN_USE_N64, pis_interrupt0, false);

  // Stop and release the dynamically-claimed state machine + program
  pio_sm_set_enabled(PIO_IN_USE_N64, PIO_SM, false);
  pio_remove_program_and_unclaim_sm(&joybus_program, PIO_IN_USE_N64, PIO_SM, _n64_offset);

  // Notify transport about disconnection before clearing params
  _n64_set_connected(false);
  _n64_timer_started = false;
  _n64_last_poll_us  = 0;

  // Reset all internal state
  _workingCmd     = 0x00;
  _byteCount      = 0;
  _crc_reply      = 0;
  _n64_sent_poll  = false;
  _n64_rumble     = false;
  _n64_hal_params = NULL;
  _n64_hal_tpsm   = NULL;
}

bool transport_jb64_init(core_params_s *params)
{
  if(params->core_report_format != CORE_REPORTFORMAT_N64) return false;
  _n64_hal_params = params;

  _n64_last_poll_us  = 0;
  _n64_timer_started = false;
  _n64_connected     = false;

  _joybus_n64_hal_init();
  return true;
}

void _jbgc_autoinit_finalize(bool success)
{
  if(_n64_hal_tpsm)
  {
    _n64_hal_tpsm->state = success ? TP_AUTOINIT_SUCCESS : TP_AUTOINIT_FAILURE;
    _n64_hal_tpsm->result_ready_cb();
    _n64_hal_tpsm = NULL;
  }
}

//bool transport_jbgc_autoinit(transport_autoinit_state_t *sm, core_params_s *params)
//{
//  _n64_hal_tpsm = sm;
//  transport_jb64_init(params);
//}

void transport_jb64_task(uint64_t timestamp)
{
  if(!_n64_hal_params) return;

  static interval_s interval = {0};

  if(interval_run(timestamp, _n64_hal_params->core_pollrate_us, &interval))
  {
    // Get input report here
    core_report_s report;
    if(core_get_generated_report(&report))
    {
      snapshot_n64input_write(&_n64_hal_snap, (core_n64_report_s*)report.data);
    }

    // Rumble
    // Handle rumble state if it changes
    static bool rumblestate = false;
    if(_n64_rumble != rumblestate)
    {
        rumblestate = _n64_rumble;

        uint8_t rumble = rumblestate ? 255 : 0;

        tp_evt_s evt = { 
          .evt = TP_EVT_ERMRUMBLE,
          .evt_ermrumble = {
          .left = rumble, .right = rumble, .leftbrake = 0, .rightbrake = 0
        }};
        
        transport_evt_cb(evt);
    }
  }

  if(_n64_sent_poll)
  {
    _n64_last_poll_us  = timestamp;
    _n64_timer_started = true;
    _n64_set_connected(true);
    _jbgc_autoinit_finalize(true);
    _n64_sent_poll = false;
  }

  // Seed the timeout window from the first tick so a controller that is never
  // polled (e.g. plugged into a powered-off console) still shuts down on time.
  if(!_n64_timer_started)
  {
    _n64_timer_started = true;
    _n64_last_poll_us  = timestamp;
    return;
  }

  uint64_t elapsed = timestamp - _n64_last_poll_us;

  // 4s with no poll -> player 0 + disconnected (and reset the bus state machine).
  if(_n64_connected && (elapsed >= N64_DISCONNECT_US))
  {
    _n64_set_connected(false);
    _n64_reset_state();
    _jbgc_autoinit_finalize(false);
  }

  // 8s with no poll -> power off, but only where ship mode can actually cut
  // power; otherwise hoja_shutdown() would reboot-loop (e.g. USB dev boards).
  if((elapsed >= N64_SHUTDOWN_US) && battery_has_driver())
  {
    hoja_deinit(hoja_shutdown);
  }
}

#endif
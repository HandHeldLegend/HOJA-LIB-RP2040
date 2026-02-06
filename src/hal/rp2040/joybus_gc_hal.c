#include "board_config.h"

#if defined(HOJA_TRANSPORT_JOYBUS64_DRIVER) && (HOJA_TRANSPORT_JOYBUS64_DRIVER == JOYBUS_GC_DRIVER_HAL)

#include <stdlib.h>

#include "cores/core_gamecube.h"
#include "transport/transport.h"
#include "transport/transport_joybusgc.h"

#include "hal/joybus_gc_hal.h"
#include "pico/stdlib.h"
#include "hoja.h"
#include "hardware/pio.h"
#include "generated/joybus.pio.h"

#include "utilities/interval.h"
#include "utilities/crosscore_snapshot.h"

typedef enum
{
  GCUBE_CMD_PROBE = 0x00,
  GCUBE_CMD_POLL = 0x40,
  GCUBE_CMD_ORIGIN = 0x41,
  GCUBE_CMD_ORIGINEXT = 0x42,
  GCUBE_CMD_SWISS = 0x1D,
} gc_cmd_t;

SNAPSHOT_TYPE(gcinput, core_gamecube_report_s);
snapshot_gcinput_t _gc_hal_snap;

core_params_s *_gc_core_params = NULL;

#if !defined(JOYBUS_GC_DRIVER_PIO_INSTANCE)
#error "JOYBUS_GC_DRIVER_PIO_INSTANCE must be defined in board_config.h"
#endif

#if !defined(JOYBUS_GC_DRIVER_DATA_PIN)
#error "JOYBUS_GC_DRIVER_DATA_PIN must be defined in board_config.h"
#endif

#if (JOYBUS_GC_DRIVER_PIO_INSTANCE == 0)
#define GC_PIO_IN_USE pio0
#define PIO_IRQ_USE_0 PIO0_IRQ_0
#define PIO_IRQ_USE_1 PIO0_IRQ_1
#else
#define GC_PIO_IN_USE pio1
#define PIO_IRQ_USE_0 PIO1_IRQ_0
#define PIO_IRQ_USE_1 PIO1_IRQ_1
#endif

#define PIO_SM 0

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))
#define ALIGNED_JOYBUS_8(val) ((val) << 24)

uint _gamecube_irq;
uint _gamecube_offset;
pio_sm_config _gamecube_c;

volatile bool _gc_got_data = false;
bool _gc_running = false;
bool _gc_rumble = false;
bool _gc_brake = false;

volatile uint8_t _gamecube_in_buffer[8] = {0};

void _gamecube_send_probe()
{
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(0x09));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, 0);
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(0x03));
}

void _gamecube_send_origin()
{
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, 0);
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(0x80));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(0x80));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(0x80));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(0x80));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(0x80));

  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(0x0)); // LT
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(0x0)); // RT
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, 0);
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, 0);
}

void _gamecube_send_poll()
{
  static core_gamecube_report_s out;
  snapshot_gcinput_read(&_gc_hal_snap, &out);
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(out.buttons_1));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(out.buttons_2));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(out.stick_left_x));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(out.stick_left_y));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(out.stick_right_x));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(out.stick_right_y));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(out.analog_trigger_l));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(out.analog_trigger_r));
}

void _gamecube_reset_state()
{
  joybus_program_init(GC_PIO_IN_USE, PIO_SM, _gamecube_offset, JOYBUS_GC_DRIVER_DATA_PIN, &_gamecube_c);
}

#define BYTECOUNT_DEFAULT 2
#define BYTECOUNT_UNKNOWN -1
#define BYTECOUNT_SWISS 10
volatile int _byteCounter = BYTECOUNT_UNKNOWN;
volatile uint8_t _workingCmd = 0x00;
volatile uint8_t _workingMode = 0x03;

// Constants for default cycles and clock speeds
#define DEFAULT_CYCLES 80        // 80 was the tested working from old FW
#define DEFAULT_CLOCK_KHZ 125000 // 125 MHz
#define NEW_CLOCK_KHZ (HOJA_BSP_CLOCK_SPEED_HZ / 1000)
const uint32_t _gc_delay_cycles_origin = 50;
const uint32_t _gc_delay_cycles_probe = 25; // 100 was around 9us
const uint32_t _gc_delay_cycles_poll = 75;

void __time_critical_func(_gamecube_command_handler)()
{
  bool ret = false;
  uint8_t dat = 0;
  uint16_t c;

  // Single byte commands handle here
  if (_byteCounter == BYTECOUNT_UNKNOWN)
  {
    _workingCmd = pio_sm_get(GC_PIO_IN_USE, PIO_SM);

    if (_workingCmd == 0)
    {
      joybus_jump_output(GC_PIO_IN_USE, PIO_SM, _gamecube_offset);
      c = _gc_delay_cycles_probe;
      while (c--)
        asm("nop");
      _gamecube_send_probe();
      _byteCounter = BYTECOUNT_UNKNOWN;
      ret = true;
    }
    else if (_workingCmd == GCUBE_CMD_ORIGIN)
    {
      c = _gc_delay_cycles_origin;
      while (c--)
        asm("nop");
      joybus_jump_output(GC_PIO_IN_USE, PIO_SM, _gamecube_offset);
      _gamecube_send_origin();
      _byteCounter = BYTECOUNT_UNKNOWN;
      ret = true;
    }
    else if (_workingCmd == GCUBE_CMD_SWISS)
    {
      _byteCounter = BYTECOUNT_SWISS;
    }
    else
    {
      _byteCounter = BYTECOUNT_DEFAULT;
    }
  }
  else
  {

    dat = pio_sm_get(GC_PIO_IN_USE, PIO_SM);

    switch (_workingCmd)
    {
    default:
      break;

    case GCUBE_CMD_SWISS:
    {
      if (!_byteCounter)
      {
        _byteCounter = BYTECOUNT_UNKNOWN;
        _gamecube_reset_state();
        ret = true;
      }
    }
    break;

    case GCUBE_CMD_POLL:
    {
      if (_byteCounter == 1)
      {
        // Get our working mode
        _workingMode = dat;
      }
      else if (!_byteCounter)
      {
        _gc_rumble = (dat & 1) ? true : false;
        _gc_brake = (dat & 2) ? true : false;
        _byteCounter = BYTECOUNT_UNKNOWN;
        c = _gc_delay_cycles_poll;
        while (c--)
          asm("nop");
        joybus_jump_output(GC_PIO_IN_USE, PIO_SM, _gamecube_offset);
        _gamecube_send_poll();
        ret = true;
      }
    }
    break;

    case GCUBE_CMD_ORIGINEXT:
    {
      if (!_byteCounter)
      {
        _byteCounter = BYTECOUNT_UNKNOWN;
        c = _gc_delay_cycles_poll;
        while (c--)
          asm("nop");
        joybus_jump_output(GC_PIO_IN_USE, PIO_SM, _gamecube_offset);
        _gamecube_send_origin();
        ret = true;
      }
    }
    break;
    }
  }

  _gc_got_data = true;

  if (!ret)
  {
    _byteCounter -= 1;
  }
}

static void __time_critical_func(_gamecube_isr_handler)(void)
{
  irq_set_enabled(_gamecube_irq, false);
  if (pio_interrupt_get(GC_PIO_IN_USE, 0))
  {
    pio_interrupt_clear(GC_PIO_IN_USE, 0);
    _gamecube_command_handler();
  }
  irq_set_enabled(_gamecube_irq, true);
}

bool _joybus_gc_hal_init()
{
  _gamecube_offset = pio_add_program(GC_PIO_IN_USE, &joybus_program);

  _gamecube_irq = PIO_IRQ_USE_0;

  pio_set_irq0_source_enabled(GC_PIO_IN_USE, pis_interrupt0, true);

  irq_set_exclusive_handler(_gamecube_irq, _gamecube_isr_handler);

  irq_set_priority(PIO_IRQ_USE_0, 0);
  // irq_set_priority(PIO_IRQ_USE_1, 0);

  joybus_program_init(GC_PIO_IN_USE, PIO_SM, _gamecube_offset, JOYBUS_GC_DRIVER_DATA_PIN, &_gamecube_c);
  irq_set_enabled(_gamecube_irq, true);
  _gc_running = true;

  return true;
}

core_params_s *_gc_hal_params = NULL;

void _jbgc_translate_data(uint8_t mode, core_gamecube_report_s *in, core_gamecube_report_s *out)
{
  out->blank_2 = 1;
  *out = *in;

  switch (_workingMode)
  {
  // Default is mode 3
  default:
    // Leave as-is
    break;

  case 0:
    out->mode0.stick_left_x   = in->stick_left_x;
    out->mode0.stick_left_y   = in->stick_left_y;
    out->mode0.stick_right_x  = in->stick_right_x;
    out->mode0.stick_right_y  = in->stick_right_y;
    out->mode0.analog_trigger_l = in->analog_trigger_l >> 4;
    out->mode0.analog_trigger_r = in->analog_trigger_r >> 4;
    out->mode0.analog_a = 0; // 4bits
    out->mode0.analog_b = 0; // 4bits
    break;

  case 1:
    out->mode1.stick_left_x     = in->stick_left_x;
    out->mode1.stick_left_y     = in->stick_left_y;
    out->mode1.stick_right_x    = in->stick_right_x >> 4;
    out->mode1.stick_right_y    = in->stick_right_y >> 4;
    out->mode1.analog_trigger_l = in->analog_trigger_l;
    out->mode1.analog_trigger_r = in->analog_trigger_r;
    out->mode1.analog_a = 0; // 4bits
    out->mode1.analog_b = 0; // 4bits
    break;

  case 2:
    out->mode2.stick_left_x = in->stick_left_x;
    out->mode2.stick_left_y = in->stick_left_y;
    out->mode2.stick_right_x = in->stick_right_x >> 4;
    out->mode2.stick_right_y = in->stick_right_y >> 4;
    out->mode2.analog_trigger_l = in->analog_trigger_l >> 4;
    out->mode2.analog_trigger_r = in->analog_trigger_r >> 4;
    out->mode2.analog_a = 0;
    out->mode2.analog_b = 0;
    break;

  case 4:
    out->mode4.stick_left_x = in->stick_left_x;
    out->mode4.stick_left_y = in->stick_left_y;
    out->mode4.stick_right_x = in->stick_right_x;
    out->mode4.stick_right_y = in->stick_right_y;
    out->mode4.analog_a = 0;
    out->mode4.analog_b = 0;
    break;
  }
}

static inline void _jbgc_handle_rumble()
{
  // Handle rumble state if it changes
  static bool rumblestate = false;
  if (_gc_rumble != rumblestate)
  {
    rumblestate = _gc_rumble;

    uint8_t rumble = rumblestate ? 255 : 0;

    tp_evt_s evt = {.evt_ermrumble = {
                        .left = rumble, .right = rumble, .left = 0, .right = 0}};

    transport_evt_cb(evt);
  }
}

void _jbgc_handle_connection(bool connected)
{
  // Handle connection state if it changes
  static uint8_t connectstate = 0;
  bool emit = false;
  tp_connectionchange_t c = TP_CONNECTION_NONE;

  if (!connectstate && connected)
  {
    c = TP_CONNECTION_CONNECTED;
    connectstate = 1;
    emit = true;
  }
  else if (connectstate && !connected)
  {
    c = TP_CONNECTION_DISCONNECTED;
    connectstate = 0;
    emit = true;
    _gamecube_reset_state();
    sleep_ms(8);
  }

  tp_evt_s evt = {.evt_connectionchange = {
                      .connection = c}};

  if (emit)
  {
    transport_evt_cb(evt);
  }
}

/***********************************************/
/********* Transport Defines *******************/
void transport_jbgc_stop()
{
}

bool transport_jbgc_init(core_params_s *params)
{
  if (params->core_report_format != CORE_REPORTFORMAT_GAMECUBE)
    return false;
  _gc_hal_params = params;

  _joybus_gc_hal_init();
  return true;
}

void transport_jbgc_task(uint64_t timestamp)
{
  if (!_gc_hal_params)
    return;

  static interval_s interval = {0};
  static interval_s interval_reset = {0};

  if (interval_run(timestamp, _gc_hal_params->core_pollrate_us, &interval))
  {
    // Get input report here
    core_report_s report;
    if (core_get_generated_report(&report))
    {

      snapshot_gcinput_write(&_gc_hal_snap, (core_gamecube_report_s*)report.data);
    }

    // Rumblecore_gamecube_report_s
    _jbgc_handle_rumble();
  }

  if (interval_resettable_run(timestamp, 1000000, _gc_got_data, &interval_reset))
  {
    _jbgc_handle_connection(false);
  }

  if (_gc_got_data)
  {
    _jbgc_handle_connection(true);
    _gc_got_data = false;
  }
}

#endif
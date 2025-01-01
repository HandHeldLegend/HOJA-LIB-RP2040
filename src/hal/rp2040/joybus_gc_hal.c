#include "hal/joybus_gc_hal.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "generated/joybus.pio.h"

#include "wired/gamecube.h"
#include "utilities/interval.h"
#include "devices/haptics.h"

#include "input_shared_types.h"
#include "input/button.h"
#include "input/analog.h"

#include "board_config.h"

#if defined(HOJA_JOYBUS_GC_DRIVER) && (HOJA_JOYBUS_GC_DRIVER == JOYBUS_GC_DRIVER_HAL)

#if !defined(JOYBUS_GC_DRIVER_PIO_INSTANCE)
    #error "JOYBUS_GC_DRIVER_PIO_INSTANCE must be defined in board_config.h" 
#endif

#if !defined(JOYBUS_GC_DRIVER_DATA_PIN)
    #error "JOYBUS_GC_DRIVER_DATA_PIN must be defined in board_config.h" 
#endif

#if(JOYBUS_GC_DRIVER_PIO_INSTANCE==0)
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
uint _gamecube_irq_tx;
uint _gamecube_offset;
pio_sm_config _gamecube_c;

volatile bool _gc_got_data = false;
bool _gc_running = false;
bool _gc_rumble = false;

uint8_t _gamecube_out_buffer[8] = {0};
volatile uint8_t _gamecube_in_buffer[8] = {0};
static gamecube_input_s _out_buffer = {
    .stick_left_x   = 128, .stick_left_y    = 128, 
    .stick_right_x  = 128, .stick_right_y   = 128
    };

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
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(_out_buffer.buttons_1));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(_out_buffer.buttons_2));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(_out_buffer.stick_left_x));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(_out_buffer.stick_left_y));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(_out_buffer.stick_right_x));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(_out_buffer.stick_right_y));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(_out_buffer.analog_trigger_l));
  pio_sm_put_blocking(GC_PIO_IN_USE, PIO_SM, ALIGNED_JOYBUS_8(_out_buffer.analog_trigger_r));
}

void _gamecube_reset_state()
{
  joybus_set_in(true, GC_PIO_IN_USE, PIO_SM, _gamecube_offset, &_gamecube_c, JOYBUS_GC_DRIVER_DATA_PIN);
}

#define BYTECOUNT_DEFAULT 2
#define BYTECOUNT_UNKNOWN -1
#define BYTECOUNT_SWISS 10
volatile int _byteCounter = BYTECOUNT_UNKNOWN;
volatile uint8_t _workingCmd = 0x00;
volatile uint8_t _workingMode = 0x03;

// Constants for default cycles and clock speeds
#define DEFAULT_CYCLES         80 // 80 was the tested working from old FW
#define DEFAULT_CLOCK_KHZ      133000 // 125 MHz
#define NEW_CLOCK_KHZ          (HOJA_BSP_CLOCK_SPEED_HZ/1000)
const uint32_t _delay_cycles_gc = (DEFAULT_CYCLES * DEFAULT_CLOCK_KHZ) / NEW_CLOCK_KHZ;

void __time_critical_func(_gamecube_command_handler)()
{
  bool ret = false;
  uint8_t dat = 0;
  uint16_t c = _delay_cycles_gc;

  if(_byteCounter==BYTECOUNT_UNKNOWN)
  {
    _workingCmd = pio_sm_get(GC_PIO_IN_USE, PIO_SM);
    switch(_workingCmd)
    {
      default:
        _byteCounter = BYTECOUNT_DEFAULT;
        break;
      case GCUBE_CMD_SWISS:
        _byteCounter = BYTECOUNT_SWISS;
        break;
    }
  }
  else
  {
    dat = pio_sm_get(GC_PIO_IN_USE, PIO_SM);
  }

  switch(_workingCmd)
  {
    default:
    break;

    case GCUBE_CMD_SWISS:
    {
      if(!_byteCounter)
      {
        _byteCounter = BYTECOUNT_UNKNOWN;
        _gamecube_reset_state();
        ret = true;
      }
    }
    break;

    case GCUBE_CMD_PROBE:
    {
      _byteCounter = BYTECOUNT_UNKNOWN;
      joybus_set_in(false, GC_PIO_IN_USE, PIO_SM, _gamecube_offset, &_gamecube_c, JOYBUS_GC_DRIVER_DATA_PIN);
      while(c--)
        asm("nop");
      _gamecube_send_probe();
      ret = true;
    }
    break;

    case GCUBE_CMD_POLL:
    {
      if(_byteCounter==1)
      {
        // Get our working mode
        _workingMode = dat;
      }
      else if(!_byteCounter)
      {
        _gc_rumble = (dat&1) ? true : false;
        _byteCounter = BYTECOUNT_UNKNOWN;
        joybus_set_in(false, GC_PIO_IN_USE, PIO_SM, _gamecube_offset, &_gamecube_c, JOYBUS_GC_DRIVER_DATA_PIN);
        while(c--)
          asm("nop");
        _gamecube_send_poll();
        ret = true;
      }
    }
    break;

    case GCUBE_CMD_ORIGIN:
    {
      _byteCounter = BYTECOUNT_UNKNOWN;
      joybus_set_in(false, GC_PIO_IN_USE, PIO_SM, _gamecube_offset, &_gamecube_c, JOYBUS_GC_DRIVER_DATA_PIN);
      while(c--)
        asm("nop");
      _gamecube_send_origin();
      ret = true;
    }
    break;

    case GCUBE_CMD_ORIGINEXT:
    {
      if(!_byteCounter)
      {
        _byteCounter = BYTECOUNT_UNKNOWN;
        joybus_set_in(false, GC_PIO_IN_USE, PIO_SM, _gamecube_offset, &_gamecube_c, JOYBUS_GC_DRIVER_DATA_PIN);
        while(c--)
          asm("nop");
        _gamecube_send_origin();
        ret = true;
      }
    }
    break;
  }

  if(!ret)
  {
    _byteCounter -= 1;
  }

}

static void _gamecube_isr_handler(void)
{
  irq_set_enabled(_gamecube_irq, false);
  if (pio_interrupt_get(GC_PIO_IN_USE, 0))
  {
    _gc_got_data = true;
    pio_interrupt_clear(GC_PIO_IN_USE, 0);
    _gamecube_command_handler();
  }
  irq_set_enabled(_gamecube_irq, true);
}

static void _gamecube_isr_txdone(void)
{
  if (pio_interrupt_get(GC_PIO_IN_USE, 1))
  {
    pio_interrupt_clear(GC_PIO_IN_USE, 1);
    joybus_set_in(true, GC_PIO_IN_USE, PIO_SM, _gamecube_offset, &_gamecube_c, JOYBUS_GC_DRIVER_DATA_PIN);
  }
}

void joybus_gc_hal_stop()
{

}

bool joybus_gc_hal_init()
{
    _gamecube_offset = pio_add_program(GC_PIO_IN_USE, &joybus_program);

    _gamecube_irq    = PIO_IRQ_USE_0;
    _gamecube_irq_tx = PIO_IRQ_USE_1;

    pio_set_irq0_source_enabled(GC_PIO_IN_USE, pis_interrupt0, true);
    pio_set_irq1_source_enabled(GC_PIO_IN_USE, pis_interrupt1, true);

    irq_set_exclusive_handler(_gamecube_irq,    _gamecube_isr_handler);
    irq_set_exclusive_handler(_gamecube_irq_tx, _gamecube_isr_txdone);

    joybus_program_init(GC_PIO_IN_USE, PIO_SM, _gamecube_offset, JOYBUS_GC_DRIVER_DATA_PIN, &_gamecube_c);
    irq_set_enabled(_gamecube_irq, true);
    irq_set_enabled(_gamecube_irq_tx, true);
    _gc_running = true;

    return true;
}

#define INPUT_POLL_RATE 1000 // 1ms

void joybus_gc_hal_task(uint32_t timestamp)
{
    static interval_s     interval_reset    = {0};
    static interval_s     interval    = {0};
    static button_data_s  buttons     = {0};
    static analog_data_s  analog      = {0};
    
    // Wait for init to complete
    if (!_gc_running) return;

    // Reset if no new data for 150ms
    if (interval_resettable_run(timestamp, 150000, _gc_got_data, &interval_reset))
    {
        _gamecube_reset_state();
    }
    
    if(interval_run(timestamp, INPUT_POLL_RATE, &interval))
    {
        if(_gc_got_data) _gc_got_data = false;

        // Update input data
        button_access_try(&buttons, BUTTON_ACCESS_REMAPPED_DATA);
        analog_access_try(&analog,  ANALOG_ACCESS_DEADZONE_DATA);
        
        static bool _rumblestate = false;
        if (_gc_rumble != _rumblestate)
        {
          _rumblestate = _gc_rumble;
          haptics_set_std(_rumblestate ? 235 : 0);
        }

        // Our buttons are always the same formatting
        _out_buffer.blank_2 = 1;
        _out_buffer.a       = buttons.button_a;
        _out_buffer.b       = buttons.button_b;
        _out_buffer.x       = buttons.button_x;
        _out_buffer.y       = buttons.button_y;
        _out_buffer.start   = buttons.button_plus;
        _out_buffer.l       = buttons.trigger_zl;
        _out_buffer.r       = buttons.trigger_zr;

        _out_buffer.dpad_down   = buttons.dpad_down;
        _out_buffer.dpad_left   = buttons.dpad_left;
        _out_buffer.dpad_right  = buttons.dpad_right;
        _out_buffer.dpad_up     = buttons.dpad_up;

        _out_buffer.z           = buttons.trigger_r;

        const int32_t center_value = 128;
        const float   target_max = 110.0f / 2048.0f;
        const int32_t fixed_multiplier = (int32_t) (target_max * (1<<10));

        bool lx_sign = analog.lx < 0;
        bool ly_sign = analog.ly < 0;
        bool rx_sign = analog.rx < 0;
        bool ry_sign = analog.ry < 0;

        uint32_t lx_abs = lx_sign ? -analog.lx : analog.lx;
        uint32_t ly_abs = ly_sign ? -analog.ly : analog.ly;
        uint32_t rx_abs = rx_sign ? -analog.rx : analog.rx;
        uint32_t ry_abs = ry_sign ? -analog.ry : analog.ry;

        // Analog stick data conversion
        int32_t lx = ((lx_abs * fixed_multiplier) >> 10) * (lx_sign ? -1 : 1);
        int32_t ly = ((ly_abs * fixed_multiplier) >> 10) * (ly_sign ? -1 : 1);
        int32_t rx = ((rx_abs * fixed_multiplier) >> 10) * (rx_sign ? -1 : 1);
        int32_t ry = ((ry_abs * fixed_multiplier) >> 10) * (ry_sign ? -1 : 1);

        uint8_t lx8 = CLAMP_0_255(lx + center_value);
        uint8_t ly8 = CLAMP_0_255(ly + center_value);
        uint8_t rx8 = CLAMP_0_255(rx + center_value);
        uint8_t ry8 = CLAMP_0_255(ry + center_value);
        // End analog stick conversion section

        // Trigger with SP function conversion
        uint8_t lt8 = 0;
        uint8_t rt8 = 0;
        uint8_t outl = 0;
        uint8_t outr = 0;

        // Handle reporting for differing modes
        switch(_workingMode)
        {
        // Default is mode 3
        default:
            _out_buffer.stick_left_x  = lx8;
            _out_buffer.stick_left_y  = ly8;
            _out_buffer.stick_right_x = rx8;
            _out_buffer.stick_right_y = ry8;
            _out_buffer.analog_trigger_l  = lt8;
            _out_buffer.analog_trigger_r  = rt8;
        break;

        case 0:
            _out_buffer.mode0.stick_left_x  = lx8;
            _out_buffer.mode0.stick_left_y  = ly8;
            _out_buffer.mode0.stick_right_x = rx8;
            _out_buffer.mode0.stick_right_y = ry8;
            _out_buffer.mode0.analog_trigger_l  = lt8>>4;
            _out_buffer.mode0.analog_trigger_r  = rt8>>4;
            _out_buffer.mode0.analog_a = 0; // 4bits
            _out_buffer.mode0.analog_b = 0; // 4bits
        break;

        case 1:
            _out_buffer.mode1.stick_left_x  = lx8;
            _out_buffer.mode1.stick_left_y  = ly8;
            _out_buffer.mode1.stick_right_x = rx8>>4;
            _out_buffer.mode1.stick_right_y = ry8>>4;
            _out_buffer.mode1.analog_trigger_l  = lt8;
            _out_buffer.mode1.analog_trigger_r  = rt8;
            _out_buffer.mode1.analog_a = 0; // 4bits
            _out_buffer.mode1.analog_b = 0; // 4bits
        break;

        case 2:
            _out_buffer.mode2.stick_left_x  = lx8;
            _out_buffer.mode2.stick_left_y  = ly8;
            _out_buffer.mode2.stick_right_x = rx8>>4;
            _out_buffer.mode2.stick_right_y = ry8>>4;
            _out_buffer.mode2.analog_trigger_l  = lt8>>4;
            _out_buffer.mode2.analog_trigger_r  = rt8>>4;
            _out_buffer.mode2.analog_a = 0;
            _out_buffer.mode2.analog_b = 0;
        break;

        case 4:
            _out_buffer.mode4.stick_left_x  = lx8;
            _out_buffer.mode4.stick_left_y  = ly8;
            _out_buffer.mode4.stick_right_x = rx8;
            _out_buffer.mode4.stick_right_y = ry8;
            _out_buffer.mode4.analog_a = 0;
            _out_buffer.mode4.analog_b = 0;
        break;
        }
    }
}


#endif
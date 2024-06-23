#include "gamecube.h"

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))

uint _gamecube_irq;
uint _gamecube_irq_tx;
uint _gamecube_offset;
pio_sm_config _gamecube_c;

volatile bool _gc_got_data = false;
bool _gc_running = false;
bool _gc_rumble = false;

uint8_t _gamecube_out_buffer[8] = {0};
volatile uint8_t _gamecube_in_buffer[8] = {0};
static gamecube_input_s _out_buffer = {.stick_left_x = 127, .stick_left_y = 127, .stick_right_x = 127, .stick_right_y = 127};

#define ALIGNED_JOYBUS_8(val) ((val) << 24)

#if(HOJA_CAPABILITY_NINTENDO_JOYBUS==1)
void _gamecube_send_probe()
{
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x09));
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, 0);
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x03));
}

void _gamecube_send_origin()
{
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, 0);
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x80));
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x80));
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x80));
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x80));
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x80));

  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x0)); // LT
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x0)); // RT
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, 0);
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, 0);
}

void _gamecube_send_poll()
{
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_out_buffer.buttons_1));
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_out_buffer.buttons_2));
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_out_buffer.stick_left_x));
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_out_buffer.stick_left_y));
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_out_buffer.stick_right_x));
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_out_buffer.stick_right_y));
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_out_buffer.analog_trigger_l));
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_out_buffer.analog_trigger_r));
  analog_send_reset();
}

void _gamecube_reset_state()
{
  joybus_set_in(true, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c, HOJA_SERIAL_PIN);
}
#endif

#define BYTECOUNT_DEFAULT 2
#define BYTECOUNT_UNKNOWN -1
#define BYTECOUNT_SWISS 10
volatile int _byteCounter = BYTECOUNT_UNKNOWN;
volatile uint8_t _workingCmd = 0x00;
volatile uint8_t _workingMode = 0x03;

#if(HOJA_CAPABILITY_NINTENDO_JOYBUS==1)

void __time_critical_func(_gamecube_command_handler)()
{
  bool ret = false;
  uint8_t dat = 0;
  uint16_t c = 80;

  if(_byteCounter==BYTECOUNT_UNKNOWN)
  {
    _workingCmd = pio_sm_get(GAMEPAD_PIO, GAMEPAD_SM);
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
    dat = pio_sm_get(GAMEPAD_PIO, GAMEPAD_SM);
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
      joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c, HOJA_SERIAL_PIN);
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
        _byteCounter = BYTECOUNT_UNKNOWN;
        joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c, HOJA_SERIAL_PIN);
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
      joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c, HOJA_SERIAL_PIN);
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
        joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c, HOJA_SERIAL_PIN);
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
  if (pio_interrupt_get(GAMEPAD_PIO, 0))
  {
    _gc_got_data = true;
    pio_interrupt_clear(GAMEPAD_PIO, 0);
    _gamecube_command_handler();
  }
  irq_set_enabled(_gamecube_irq, true);
}

static void _gamecube_isr_txdone(void)
{
  if (pio_interrupt_get(GAMEPAD_PIO, 1))
  {
    pio_interrupt_clear(GAMEPAD_PIO, 1);
    joybus_set_in(true, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c, HOJA_SERIAL_PIN);
  }
}
#endif

void gamecube_comms_task(uint32_t timestamp, button_data_s *buttons, a_data_s *analog)
{
  #if(HOJA_CAPABILITY_NINTENDO_JOYBUS==1)
  static interval_s interval = {0};
  

  if (!_gc_running)
  {
    sleep_ms(150);
    _gamecube_offset = pio_add_program(GAMEPAD_PIO, &joybus_program);

    _gamecube_irq = PIO1_IRQ_0;
    _gamecube_irq_tx = PIO1_IRQ_1;

    pio_set_irq0_source_enabled(GAMEPAD_PIO, pis_interrupt0, true);
    pio_set_irq1_source_enabled(GAMEPAD_PIO, pis_interrupt1, true);

    irq_set_exclusive_handler(_gamecube_irq, _gamecube_isr_handler);
    irq_set_exclusive_handler(_gamecube_irq_tx, _gamecube_isr_txdone);

    joybus_program_init(GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, HOJA_SERIAL_PIN, &_gamecube_c);
    irq_set_enabled(_gamecube_irq, true);
    irq_set_enabled(_gamecube_irq_tx, true);
    _gc_running = true;
  }
  else
  {
    if (interval_resettable_run(timestamp, 150000, _gc_got_data, &interval))
    {
      _gamecube_reset_state();
    }
    else
    {
      if(_gc_got_data) _gc_got_data = false;
      
      static bool _rumblestate = false;
      if (_gc_rumble != _rumblestate)
      {
        _rumblestate = _gc_rumble;
        float amp = _rumblestate ? 0.85f : 0;
        hoja_rumble_set(HOJA_HAPTIC_BASE_HFREQ, amp, HOJA_HAPTIC_BASE_LFREQ, amp);
      }

      // Our buttons are always the same formatting
      _out_buffer.blank_2 = 1;
      _out_buffer.a = buttons->button_a;
      _out_buffer.b = buttons->button_b;
      _out_buffer.x = buttons->button_x;
      _out_buffer.y = buttons->button_y;
      _out_buffer.start = buttons->button_plus;
      _out_buffer.l = buttons->trigger_zl;
      _out_buffer.r = buttons->trigger_zr;
      _out_buffer.dpad_down = buttons->dpad_down;
      _out_buffer.dpad_left = buttons->dpad_left;
      _out_buffer.dpad_right = buttons->dpad_right;
      _out_buffer.dpad_up = buttons->dpad_up;

      // Analog stick data conversion
      float lx = (analog->lx * 0.0488f) + 28;
      float ly = (analog->ly * 0.0488f) + 28;
      float rx = (analog->rx * 0.0488f) + 28;
      float ry = (analog->ry * 0.0488f) + 28;
      uint8_t lx8 = CLAMP_0_255(lx);
      uint8_t ly8 = CLAMP_0_255(ly);
      uint8_t rx8 = CLAMP_0_255(rx);
      uint8_t ry8 = CLAMP_0_255(ry);
      // End analog stick conversion section


      // Trigger with SP function conversion
      uint8_t lt8 = 0;
      uint8_t rt8 = 0;
      uint8_t outl = 0;
      uint8_t outr = 0;

      // Handle trigger SP stuff
      switch (global_loaded_settings.gc_sp_mode)
      {

      default:
        _out_buffer.z = buttons->trigger_r;
        lt8 = buttons->trigger_zl ? 255 : (buttons->zl_analog >> 4);
        rt8 = buttons->trigger_zr ? 255 : (buttons->zr_analog >> 4);

        break;

      case GC_SP_MODE_LT:

        _out_buffer.z = buttons->trigger_r;

        outl = buttons->trigger_l ? (global_loaded_settings.gc_sp_light_trigger) : 0;
        lt8 = buttons->trigger_zl ? 255 : outl;

        rt8 = buttons->zr_analog >> 4;
        rt8 = buttons->trigger_zr ? 255 : rt8;
        
        break;

      case GC_SP_MODE_RT:
        _out_buffer.z = buttons->trigger_r;

        outr = buttons->trigger_l ? (global_loaded_settings.gc_sp_light_trigger) : 0;
        rt8 = buttons->trigger_zr ? 255 : outr;

        lt8 = buttons->zl_analog >> 4;
        lt8 = buttons->trigger_zl ? 255 : lt8;

        break;

      case GC_SP_MODE_TRAINING:

        _out_buffer.z = buttons->trigger_r;

        lt8 = buttons->zl_analog >> 4;
        lt8 = buttons->trigger_zl ? 255 : lt8;

        rt8 = buttons->zr_analog >> 4;
        rt8 = buttons->trigger_zr ? 255 : rt8;

        if (buttons->trigger_l)
        {
          _out_buffer.a = 1;
          lt8 = 255;
          rt8 = 255;
          _out_buffer.l = 1;
          _out_buffer.r = 1;
        }

        break;

      case GC_SP_MODE_DUALZ:
        _out_buffer.z = buttons->trigger_l | buttons->trigger_r;

        _out_buffer.l = buttons->trigger_zl;
        _out_buffer.r = buttons->trigger_zr;

        lt8 = buttons->zl_analog >> 4;
        lt8 = buttons->trigger_zl ? 255 : lt8;

        rt8 = buttons->zr_analog >> 4;
        rt8 = buttons->trigger_zr ? 255 : rt8;

        break;
      }
    
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
}

void gamecube_init()
{

  // joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c);
}
#include "gamecube.h"

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))

uint _gamecube_irq;
uint _gamecube_irq_tx;
uint _gamecube_offset;
pio_sm_config _gamecube_c;

volatile bool _gc_got_data = false;
bool _gc_running = false;

uint8_t _gamecube_out_buffer[8] = {0};
volatile uint8_t _gamecube_in_buffer[8] = {0};
static gamecube_input_s _out_buffer = {.stick_left_x = 127, .stick_left_y = 127, .stick_right_x=127, .stick_right_y=127};

#define ALIGNED_JOYBUS_8(val) ((val) << 24)

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
}

void _gamecube_reset_state()
{
  joybus_set_in(true, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c, HOJA_SERIAL_PIN);
}

static volatile uint8_t _byteCounter = 3;
static volatile uint8_t _workingCmd = 0x00;
void __time_critical_func(_gamecube_command_handler)()
{
  uint16_t c = 40;

  if(_workingCmd == 0x40)
  {
    _byteCounter-=1;
    uint8_t dat = pio_sm_get(GAMEPAD_PIO, GAMEPAD_SM);
    if(_byteCounter==0)
    {
      _workingCmd = 0x00;
      while(c--) asm("nop");
      joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c, HOJA_SERIAL_PIN);
      _gamecube_send_poll();
    }
  }
  else
  {
    _workingCmd = pio_sm_get(GAMEPAD_PIO, GAMEPAD_SM);
    switch (_workingCmd)
    {
      default:
        break;
      case 0x00:
        while(c--) asm("nop");
        joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c, HOJA_SERIAL_PIN);
        _gamecube_send_probe();
      break;

      case 0x40:
        _byteCounter=2;
      break;

      case 0x41:
        while(c--) asm("nop");
        joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c, HOJA_SERIAL_PIN);
        _gamecube_send_origin();
      break;
    }
  }

  
}

static void _gamecube_isr_handler(void)
{
  if (pio_interrupt_get(GAMEPAD_PIO, 0))
  {
    _gc_got_data=true;
    pio_interrupt_clear(GAMEPAD_PIO, 0);
    uint16_t c = 40;
    while(c--) asm("nop");
    _gamecube_command_handler();
  }
}

static void _gamecube_isr_txdone(void)
{
  if (pio_interrupt_get(GAMEPAD_PIO, 1))
  {
    pio_interrupt_clear(GAMEPAD_PIO, 1);
    joybus_set_in(true, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c, HOJA_SERIAL_PIN);
  }
}

void gamecube_comms_task(uint32_t timestamp, button_data_s *buttons, a_data_s *analog)
{
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
    if(interval_resettable_run(timestamp, 40000, _gc_got_data))
    {
      printf("RESET.");
      _gamecube_reset_state();
      sleep_ms(100);
    }
    else
    {
      _gc_got_data = false;
      _out_buffer.blank_2 = 1;
      _out_buffer.button_a = buttons->button_a;
      _out_buffer.button_b = buttons->button_b;
      _out_buffer.button_x = buttons->button_x;
      _out_buffer.button_y = buttons->button_y;
      _out_buffer.button_start = buttons->button_plus;
      _out_buffer.button_l = buttons->trigger_zl;
      _out_buffer.button_r = buttons->trigger_zr;
      _out_buffer.button_z = buttons->trigger_r;

      float lx = (analog->lx*0.0488f) + 28;
      float ly = (analog->ly*0.0488f) + 28;
      float rx = (analog->rx*0.0488f) + 28;
      float ry = (analog->ry*0.0488f) + 28;

      _out_buffer.stick_left_x = CLAMP_0_255(lx);
      _out_buffer.stick_left_y = CLAMP_0_255(ly);
      _out_buffer.stick_right_x = CLAMP_0_255(rx);
      _out_buffer.stick_right_y = CLAMP_0_255(ry);

      _out_buffer.dpad_down = buttons->dpad_down;
      _out_buffer.dpad_left = buttons->dpad_left;
      _out_buffer.dpad_right = buttons->dpad_right;
      _out_buffer.dpad_up = buttons->dpad_up;

      int outl = 0;
      int outr = 0;

      // Handle trigger SP stuff
      switch(global_loaded_settings.gc_sp_mode)
      {
        default:
          _out_buffer.analog_trigger_l = buttons->trigger_zl ? 255 : 0;
          _out_buffer.analog_trigger_r = buttons->trigger_zr ? 255 : 0;
          break;

        case GC_SP_MODE_LT:
          outl = buttons->trigger_l ? (HOJA_ANALOG_LIGHT) : 0;
          _out_buffer.analog_trigger_l = buttons->trigger_zl ? 255 : outl;
          _out_buffer.analog_trigger_r = buttons->trigger_zr ? 255 : 0;
          break;

        case GC_SP_MODE_RT:
          outr = buttons->trigger_l ? (HOJA_ANALOG_LIGHT) : 0;
          _out_buffer.analog_trigger_r = buttons->trigger_zr ? 255 : outr;
          _out_buffer.analog_trigger_l = buttons->trigger_zl ? 255 : 0;
          break;

        case GC_SP_MODE_ADC:
          _out_buffer.analog_trigger_l = analog->lt >> 4;
          _out_buffer.analog_trigger_r = analog->rt >> 4;
          break;
      }
    }
  }
}

void gamecube_init()
{

  // joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _gamecube_offset, &_gamecube_c);
}
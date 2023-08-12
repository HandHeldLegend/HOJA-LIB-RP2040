#include "n64.h"
#include "n64_crc.h"

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))
#define ALIGNED_JOYBUS_8(val) ((val) << 24)
#define N64_RANGE 90
#define N64_RANGE_MULTIPLIER (N64_RANGE*2)/4095

uint _n64_irq;
uint _n64_irq_tx;
uint _n64_offset;
pio_sm_config _n64_c;

static n64_input_s _out_buffer = {.stick_x = 0, .stick_y = 0};
static uint8_t _in_buffer[64] = {0};

volatile static uint8_t _workingCmd = 0x00;
volatile static uint8_t _byteCount = 0;
volatile uint8_t _crc_reply = 0;

volatile bool _n64_got_data = false;
bool _n64_running = false;

uint8_t _n64_get_crc(uint8_t val)
{
    return crc_repeating_table[val] ^ 0xFF;
}

void _n64_send_probe()
{
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x05));
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, 0);
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, 0);
}

void _n64_send_pak_write()
{
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_crc_reply));
}

void _n64_send_poll()
{
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_out_buffer.buttons_1));
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_out_buffer.buttons_2));
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_out_buffer.stick_x));
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(_out_buffer.stick_y));
}

#define PAK_MSG_BYTES 36

void __time_critical_func(_n64_command_handler)()
{
    uint16_t c = 40;
    if (_workingCmd == 0x03)
    {
        _in_buffer[_byteCount] = pio_sm_get(GAMEPAD_PIO, GAMEPAD_SM);

        if (_byteCount == 3)
        {
            // Calculate response CRC
            _crc_reply = _n64_get_crc(_in_buffer[3]);
        }

        _byteCount++;
        if (_byteCount == PAK_MSG_BYTES)
        {
            _workingCmd = 0;
            _byteCount = 0;
            while (c--)
                asm("nop");
            joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _n64_offset, &_n64_c, HOJA_SERIAL_PIN);
            _n64_send_pak_write();
        }
    }
    else
    {
        _workingCmd = pio_sm_get(GAMEPAD_PIO, GAMEPAD_SM);

        switch (_workingCmd)
        {
        default:
            break;

        // Write to mem pak
        case 0x03:
            _byteCount = 1;
            break;

        case 0xFF:
        case 0x00:
            while (c--)
                asm("nop");
            joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _n64_offset, &_n64_c, HOJA_SERIAL_PIN);
            _n64_send_probe();
            break;

        // Poll
        case 0x01:
            while (c--)
                asm("nop");
            joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _n64_offset, &_n64_c, HOJA_SERIAL_PIN);
            _n64_send_poll();
            break;
        }
    }
}

static void _n64_isr_handler(void)
{
  if (pio_interrupt_get(GAMEPAD_PIO, 0))
  {
    _n64_got_data=true;
    pio_interrupt_clear(GAMEPAD_PIO, 0);
    uint16_t c = 40;
    while(c--) asm("nop");
    _n64_command_handler();
  }
}

static void _n64_isr_txdone(void)
{
  if (pio_interrupt_get(GAMEPAD_PIO, 1))
  {
    pio_interrupt_clear(GAMEPAD_PIO, 1);
    joybus_set_in(true, GAMEPAD_PIO, GAMEPAD_SM, _n64_offset, &_n64_c, HOJA_SERIAL_PIN);
  }
}

void _n64_reset_state()
{
  joybus_set_in(true, GAMEPAD_PIO, GAMEPAD_SM, _n64_offset, &_n64_c, HOJA_SERIAL_PIN);
}

void n64_comms_task(uint32_t timestamp, button_data_s *buttons, a_data_s *analog)
{
  if (!_n64_running)
  {
    sleep_ms(150);
    _n64_offset = pio_add_program(GAMEPAD_PIO, &joybus_program);
    _n64_irq = PIO1_IRQ_0;
    _n64_irq_tx = PIO1_IRQ_1;

    pio_set_irq0_source_enabled(GAMEPAD_PIO, pis_interrupt0, true);
    pio_set_irq1_source_enabled(GAMEPAD_PIO, pis_interrupt1, true);

    irq_set_exclusive_handler(_n64_irq, _n64_isr_handler);
    irq_set_exclusive_handler(_n64_irq_tx, _n64_isr_txdone);
    joybus_program_init(GAMEPAD_PIO, GAMEPAD_SM, _n64_offset, HOJA_SERIAL_PIN, &_n64_c);
    irq_set_enabled(_n64_irq, true);
    irq_set_enabled(_n64_irq_tx, true);
    _n64_running = true;
  }
  else
  {
    if(interval_resettable_run(timestamp, 40000, _n64_got_data))
    {
      printf("RESET.");
      _n64_reset_state();
      sleep_ms(100);
    }
    else
    {
      _n64_got_data = false;
      _out_buffer.button_a = buttons->button_a;
      _out_buffer.button_b = buttons->button_b;

      _out_buffer.cpad_up   = buttons->button_x;
      _out_buffer.cpad_down = buttons->button_y;

      _out_buffer.cpad_left     = buttons->trigger_l;
      _out_buffer.cpad_right    = buttons->trigger_r;

      _out_buffer.button_start = buttons->button_plus;

      _out_buffer.button_l = buttons->button_minus;

      _out_buffer.button_r = buttons->trigger_zr;
      _out_buffer.button_z = buttons->trigger_zl;

      float lx = (analog->lx*N64_RANGE_MULTIPLIER) - N64_RANGE;
      float ly = (analog->ly*N64_RANGE_MULTIPLIER) - N64_RANGE;

      _out_buffer.stick_x = (int8_t) lx;
      _out_buffer.stick_y = (int8_t) ly;

      _out_buffer.dpad_down = buttons->dpad_down;
      _out_buffer.dpad_left = buttons->dpad_left;
      _out_buffer.dpad_right    = buttons->dpad_right;
      _out_buffer.dpad_up       = buttons->dpad_up;
    }
  }
}


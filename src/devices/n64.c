#include "n64.h"
#include "n64_crc.h"

#if(HOJA_CAPABILITY_NINTENDO_JOYBUS==1)

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))
#define ALIGNED_JOYBUS_8(val) ((val) << 24)
#define N64_RANGE 90
#define N64_RANGE_MULTIPLIER (N64_RANGE*2)/4095

uint _n64_irq;
uint _n64_irq_tx;
uint _n64_offset;
pio_sm_config _n64_c;
bool _n64_rumble = false;

static n64_input_s _out_buffer = {.stick_x = 0, .stick_y = 0};
static uint8_t _in_buffer[64] = {0};

volatile static uint8_t _workingCmd = 0x00;
volatile static uint8_t _byteCount = 0;
volatile uint8_t _crc_reply = 0;

volatile bool _n64_got_data = false;
bool _n64_running = false;

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
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x80));
  }
  pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0xB8)); // Precomputed CRC (0x47 or also try 0xB8)
}

void _n64_send_null_identify()
{
  for(uint i = 0; i<33; i++)
  {
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, 0);
  }
}

void _n64_send_probe()
{
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x05));
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, 0);
    pio_sm_put_blocking(GAMEPAD_PIO, GAMEPAD_SM, ALIGNED_JOYBUS_8(0x01)); // Indicate PAK is installed
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
    analog_send_reset();
}

#define PAK_MSG_BYTES 33

void __time_critical_func(_n64_command_handler)()
{
    uint16_t c = 20;
    
    // Resume working on commands that are longer than 1 byte
    if (_workingCmd == N64_CMD_WRITEMEM)
    {
      uint16_t writeaddr = 0;
      _in_buffer[_byteCount] = pio_sm_get(GAMEPAD_PIO, GAMEPAD_SM);

      if(_byteCount>1) // Only 32 bytes after the address
      _crc_reply = _n64_iterate_crc(_crc_reply, _in_buffer[_byteCount]);
      
      if (_byteCount >= PAK_MSG_BYTES)
      {
        writeaddr = (_in_buffer[0] << 8) | (_in_buffer[1]&0xE0);
        if (writeaddr==0xC000)
        {
          _n64_rumble = (_in_buffer[2] > 0) ? true : false;
        }
        _workingCmd = 0;
        _byteCount = 0;
        //while (c--)
        //    asm("nop");
        joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _n64_offset, &_n64_c, HOJA_SERIAL_PIN);
        _n64_send_pak_write();
      } 
      else _byteCount++;
      
    }
    else if (_workingCmd == N64_CMD_READMEM)
    {
      
      _in_buffer[_byteCount] = pio_sm_get(GAMEPAD_PIO, GAMEPAD_SM);

      if (_byteCount >= 1)
      {
        _workingCmd = 0;
        _byteCount = 0;

        joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _n64_offset, &_n64_c, HOJA_SERIAL_PIN);
        // End receive so we respond
        while(c--)
          asm("nop");

        uint16_t readaddr = (_in_buffer[0]<<8) | (_in_buffer[1]&0xE0);
        if(readaddr==0x8000)
        {
          _n64_send_rumble_identify();
        }
        else _n64_send_null_identify();
      }
      else _byteCount++;

    }
    else
    {
        _workingCmd = pio_sm_get(GAMEPAD_PIO, GAMEPAD_SM);

        switch (_workingCmd)
        {
        default:
            break;

        case N64_CMD_READMEM:
            _crc_reply = 0;
            break;

        // Write to mem pak
        case N64_CMD_WRITEMEM:
            _crc_reply = 0;
            break;

        case N64_CMD_RESET:
        case N64_CMD_PROBE:
            _workingCmd = 0;
            while (c--)
                asm("nop");
            joybus_set_in(false, GAMEPAD_PIO, GAMEPAD_SM, _n64_offset, &_n64_c, HOJA_SERIAL_PIN);
            _n64_send_probe();
            break;

        // Poll
        case N64_CMD_POLL:
            _workingCmd = 0;
            c=40;
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
  //pio_sm_clear_fifos(GAMEPAD_PIO, GAMEPAD_SM);
  joybus_set_in(true, GAMEPAD_PIO, GAMEPAD_SM, _n64_offset, &_n64_c, HOJA_SERIAL_PIN);
}

#endif

void n64_comms_task(uint32_t timestamp, button_data_s *buttons, a_data_s *analog)
{
  #if(HOJA_CAPABILITY_NINTENDO_JOYBUS==1)

  static interval_s interval = {0};

  if (!_n64_running)
  {
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
    if(interval_resettable_run(timestamp, 100000, _n64_got_data, &interval))
    {
      _n64_reset_state();
      sleep_ms(8);
    }
    else
    {

      static bool _rumblestate = false;
      if(_n64_rumble != _rumblestate)
      {
        _rumblestate = _n64_rumble;
        float amp = _rumblestate ? 0.85f : 0;
        static hoja_rumble_msg_s rumble_msg_left = {0};
        static hoja_rumble_msg_s rumble_msg_right = {0};
        
        rumble_msg_left.count = 1;
        rumble_msg_left.frames[0].low_amplitude = amp;
        rumble_msg_left.frames[0].low_frequency = HOJA_HAPTIC_BASE_LFREQ;

        rumble_msg_right.count = 1;
        rumble_msg_right.frames[0].low_amplitude = amp;
        rumble_msg_right.frames[0].low_frequency = HOJA_HAPTIC_BASE_LFREQ;

        cb_hoja_rumble_set(&rumble_msg_left, &rumble_msg_right);
      }

      _n64_got_data = false;
      _out_buffer.button_a = buttons->button_a;
      _out_buffer.button_b = buttons->button_b;

      _out_buffer.cpad_up   = buttons->button_x;
      _out_buffer.cpad_down = buttons->button_y;

      _out_buffer.cpad_left     = buttons->trigger_l;
      _out_buffer.cpad_right    = buttons->trigger_r;

      _out_buffer.button_start = buttons->button_plus;

      _out_buffer.button_l = buttons->button_minus;

      #if (HOJA_CAPABILITY_ANALOG_TRIGGER_L)
        _out_buffer.button_z = ( (buttons->zl_analog > ANALOG_DIGITAL_THRESH) ? true : false ) | buttons->trigger_zl;
      #else
          _out_buffer.button_z = buttons->trigger_zl;
      #endif

      #if (HOJA_CAPABILITY_ANALOG_TRIGGER_R)
          _out_buffer.button_r = ( (buttons->zr_analog > ANALOG_DIGITAL_THRESH) ? true : false ) | buttons->trigger_zr;
      #else
          _out_buffer.button_r = buttons->trigger_zr;
      #endif

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

  #endif
}


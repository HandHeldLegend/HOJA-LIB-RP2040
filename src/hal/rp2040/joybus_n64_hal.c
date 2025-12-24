#include "hal/joybus_n64_hal.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "generated/joybus.pio.h"

#include "hoja.h"

#include "wired/n64.h"
#include "wired/n64_crc.h"
#include "utilities/interval.h"
#include "devices/haptics.h"

#include "input_shared_types.h"
#include "input/mapper.h"

#include "hoja_bsp.h"
#include "board_config.h"

#if defined(HOJA_JOYBUS_N64_DRIVER) && (HOJA_JOYBUS_N64_DRIVER == JOYBUS_N64_DRIVER_HAL)

#if !defined(JOYBUS_N64_DRIVER_PIO_INSTANCE)
    #error "JOYBUS_N64_DRIVER_PIO_INSTANCE must be defined in board_config.h" 
#endif

#if !defined(JOYBUS_N64_DRIVER_DATA_PIN)
    #error "JOYBUS_N64_DRIVER_DATA_PIN must be defined in board_config.h" 
#endif

#if(JOYBUS_N64_DRIVER_PIO_INSTANCE==0)
    #define PIO_IN_USE_N64 pio0
    #define PIO_IRQ_USE_0 PIO0_IRQ_0
    #define PIO_IRQ_USE_1 PIO0_IRQ_1
#else 
    #define PIO_IN_USE_N64 pio1 
    #define PIO_IRQ_USE_0 PIO1_IRQ_0
    #define PIO_IRQ_USE_1 PIO1_IRQ_1
#endif

#define PIO_SM 0

#define CLAMP_0_255(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))
#define ALIGNED_JOYBUS_8(val) ((val) << 24)
#define N64_RANGE 90
#define N64_RANGE_MULTIPLIER (N64_RANGE)/4096

uint _n64_irq;
uint _n64_offset;
pio_sm_config _n64_c;
bool _n64_rumble = false;

static n64_input_s _out_buffer = {.stick_x = 0, .stick_y = 0};
static uint8_t _in_buffer[64] = {0};

volatile static uint8_t _workingCmd = 0x00;
volatile static uint8_t _byteCount = 0;
volatile uint8_t _crc_reply = 0;

volatile bool   _n64_got_data = false;
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
    pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(_out_buffer.buttons_1));
    pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(_out_buffer.buttons_2));
    pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(_out_buffer.stick_x));
    pio_sm_put_blocking(PIO_IN_USE_N64, PIO_SM, ALIGNED_JOYBUS_8(_out_buffer.stick_y));
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

void __time_critical_func(_n64_command_handler)()
{
    uint32_t c = DEFAULT_CYCLES;
    
    // Resume working on commands that are longer than 1 byte
    if (_workingCmd == N64_CMD_WRITEMEM)
    {
      uint16_t writeaddr = 0;
      _in_buffer[_byteCount] = pio_sm_get(PIO_IN_USE_N64, PIO_SM);

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
      
      _in_buffer[_byteCount] = pio_sm_get(PIO_IN_USE_N64, PIO_SM);

      if (_byteCount >= 1)
      {
        _workingCmd = 0;
        _byteCount = 0;

        joybus_jump_output(PIO_IN_USE_N64, PIO_SM, _n64_offset);

        // End receive so we respond
        c = _delay_cycles_memread;
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
    _n64_got_data=true;
  }
  irq_set_enabled(_n64_irq, true);
}

void _n64_reset_state()
{
  joybus_program_init(PIO_IN_USE_N64, PIO_SM, _n64_offset, JOYBUS_N64_DRIVER_DATA_PIN, &_n64_c);
}

void joybus_n64_hal_stop()
{   

}

bool joybus_n64_hal_init()
{
    _n64_offset = pio_add_program(PIO_IN_USE_N64, &joybus_program);

    _n64_irq    = PIO_IRQ_USE_0;

    pio_set_irq0_source_enabled(PIO_IN_USE_N64, pis_interrupt0, true);

    irq_set_exclusive_handler(_n64_irq, _n64_isr_handler);

    irq_set_priority(PIO_IRQ_USE_0, 0);

    joybus_program_init(PIO_IN_USE_N64, PIO_SM, _n64_offset, JOYBUS_N64_DRIVER_DATA_PIN, &_n64_c);
    irq_set_enabled(_n64_irq, true);
    _n64_running = true;

    return true;
}

#define INPUT_POLL_RATE 1000 // 1ms
#define N64WIRE_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

void joybus_n64_hal_task(uint64_t timestamp)
{   
    static interval_s       interval_reset = {0};
    static interval_s       interval    = {0};

    // Only go when we have init
    if (!_n64_running) return;

    if(interval_resettable_run(timestamp, 100000, _n64_got_data, &interval_reset))
    {
      hoja_set_connected_status(CONN_STATUS_DISCONNECTED);
      _n64_reset_state();
      sleep_ms(8); // Wait for reset
    }
    
    if(interval_run(timestamp, INPUT_POLL_RATE, &interval))
    {
      if(_n64_got_data) 
      {
        hoja_set_connected_status(CONN_STATUS_PLAYER_1);
        _n64_got_data = false;
      }

      static bool _rumblestate = false;
      if(_n64_rumble != _rumblestate)
      {
          _rumblestate = _n64_rumble;
          haptics_set_std(_rumblestate ? 255 : 0, false);
      }

      mapper_input_s input = mapper_get_input();
      
      _out_buffer.button_a = input.presses[N64_CODE_A];
      _out_buffer.button_b = input.presses[N64_CODE_B];

      _out_buffer.cpad_up   = input.presses[N64_CODE_CUP];
      _out_buffer.cpad_down = input.presses[N64_CODE_CDOWN];

      _out_buffer.cpad_left     = input.presses[N64_CODE_CLEFT];
      _out_buffer.cpad_right    = input.presses[N64_CODE_CRIGHT];

      _out_buffer.button_start = input.presses[N64_CODE_START];

      _out_buffer.button_l = input.presses[N64_CODE_L];

      _out_buffer.button_z = input.presses[N64_CODE_Z];
      _out_buffer.button_r = input.presses[N64_CODE_R];

      const float   target_max = 85.0f / 2048.0f;
      float lx = mapper_joystick_concat(0,input.inputs[N64_CODE_LX_LEFT],input.inputs[N64_CODE_LX_RIGHT] ) * target_max;
      float ly = mapper_joystick_concat(0,input.inputs[N64_CODE_LY_DOWN],input.inputs[N64_CODE_LY_UP]    ) * target_max;

      int8_t lx8 = N64WIRE_CLAMP(lx, -128, 127);
      int8_t ly8 = N64WIRE_CLAMP(ly, -128, 127);

      _out_buffer.stick_x = lx8;
      _out_buffer.stick_y = ly8;

      _out_buffer.dpad_down     = input.presses[N64_CODE_DOWN];
      _out_buffer.dpad_left     = input.presses[N64_CODE_LEFT];
      _out_buffer.dpad_right    = input.presses[N64_CODE_RIGHT];
      _out_buffer.dpad_up       = input.presses[N64_CODE_UP];
    }
}

#endif
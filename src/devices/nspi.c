#include "nspi.h"

static bool _nspi_running;
static uint _nspi_irq;
static PIO  _nspi_pio;
static uint _nspi_sm;
static uint _nspi_offset;
static volatile uint32_t _nspi_buffer = 0xFFFFFFFF;

static void _nspi_isr_handler(void)
{
  if (pio_interrupt_get(GAMEPAD_PIO, 0))
  {
    pio_interrupt_clear(GAMEPAD_PIO, 0);

    pio_sm_drain_tx_fifo(_nspi_pio, _nspi_sm);
    pio_sm_exec_wait_blocking(_nspi_pio, _nspi_sm, pio_encode_jmp(_nspi_offset+nserial_offset_startserial));
    pio_sm_put_blocking(_nspi_pio, _nspi_sm, _nspi_buffer);
    analog_send_reset();
  }
}

void nspi_init()
{
  // Set up PIO for NSPI
  _nspi_pio = GAMEPAD_PIO;
  _nspi_irq = PIO1_IRQ_0;
  _nspi_sm = GAMEPAD_SM;
  _nspi_offset = pio_add_program(_nspi_pio, &nserial_program);

  pio_set_irq0_source_enabled(GAMEPAD_PIO, pis_interrupt0, true);
  irq_set_exclusive_handler(_nspi_irq, _nspi_isr_handler);

  nserial_program_init(_nspi_pio, _nspi_sm, _nspi_offset, HOJA_SERIAL_PIN);
  nserial_program_latch_init(_nspi_pio, _nspi_sm+1, _nspi_offset, HOJA_LATCH_PIN);

  pio_sm_put_blocking(_nspi_pio, _nspi_sm, 0xFFFFFFFF);

  irq_set_enabled(_nspi_irq, true);
}

void nspi_comms_task(uint32_t timestamp, button_data_s *buttons, a_data_s *analog)
{
  if(!_nspi_running)
  {
    nspi_init();
    _nspi_running = true;
  }
  else
  {
    static nspi_input_s buffer = {.value = 0xFFFF};

    buffer.a = !buttons->button_a;
    buffer.b = !buttons->button_b;
    buffer.x = !buttons->button_x;
    buffer.y = !buttons->button_y;

    int up = (buttons->dpad_up | (analog->ly > (3000)) );
    int down = -(buttons->dpad_down | (analog->ly < (1000)) );

    int right = (buttons->dpad_right | (analog->lx > (3000)) );
    int left = -(buttons->dpad_left | (analog->lx < (1000)) );

    int updown = up+down;
    int rightleft = right+left;

    buffer.dup    = (updown==1)   ? 0 : 1;
    buffer.ddown  = (updown==-1)  ? 0 : 1;
    buffer.dright = (rightleft==1)  ? 0 : 1;
    buffer.dleft  = (rightleft==-1) ? 0 : 1;

    buffer.select = !buttons->button_minus;
    buffer.start  = !buttons->button_plus;

    buffer.l = ! buttons->trigger_l;
    buffer.r = ! buttons->trigger_r;

    _nspi_buffer = buffer.value<<16;
  }
}

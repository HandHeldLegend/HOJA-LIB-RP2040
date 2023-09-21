#include "nspi.h"

static bool _nspi_running;
static uint _nspi_irq;
static PIO  _nspi_pio;
static uint _nspi_sm;
static uint _nspi_offset;
static uint32_t _nspi_buffer = 0xFFFFFFFF;

void nspi_load_buffer()
{
  pio_sm_put_blocking(_nspi_pio, 0, _nspi_buffer);
}

void nspi_isr_handler(uint gpio, uint32_t events)
{
  gpio_acknowledge_irq(HOJA_LATCH_PIN, IO_IRQ_BANK0);
  nserial_latch_jump(_nspi_pio, _nspi_sm);
  nspi_load_buffer();
}

void nspi_init()
{
  // Set up GPIO for NSPI latch interrupt
  _nspi_irq = IO_IRQ_BANK0;
  gpio_init(HOJA_LATCH_PIN);
  gpio_set_dir(HOJA_LATCH_PIN, GPIO_IN);
  gpio_pull_up(HOJA_LATCH_PIN);

  irq_set_exclusive_handler(_nspi_irq, (void*) nspi_isr_handler);
  // Enable GPIO ISR
  gpio_set_irq_enabled(HOJA_LATCH_PIN, GPIO_IRQ_EDGE_RISE, true);

  // Set up PIO for NSPI
  _nspi_pio = pio0;
  _nspi_sm = 0;
  _nspi_offset = pio_add_program(_nspi_pio, &nserial_program);

  nserial_program_init(_nspi_pio, _nspi_sm, _nspi_offset, 0);
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

    int up = (buttons->dpad_up | (analog->lx > (2048+1024)) );
    int down = -(buttons->dpad_down | (analog->lx < (2048-1024)) );

    int right = (buttons->dpad_right | (analog->ly > (2048+1024)) );
    int left = -(buttons->dpad_left | (analog->ly < (2048-1024)) );

    int updown = up+down;
    int rightleft = right+left;

    buffer.dup    = (updown==1)   ? 0 : 1;
    buffer.ddown  = (updown==-1)  ? 0 : 1;
    buffer.dright = (rightleft==1)  ? 0 : 1;
    buffer.dleft  = (rightleft==-1) ? 0 : 1;

    buffer.select = !buttons->button_minus;
    buffer.start  = !buttons->button_plus;

    buffer.l = ! (buttons->trigger_l | buttons->trigger_zl);
    buffer.r = ! (buttons->trigger_r | buttons->trigger_zr);

    _nspi_buffer = (buffer.value << 16) | 0xFFFF;
  }
}

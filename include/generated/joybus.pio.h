// -------------------------------------------------- //
// This file is autogenerated by pioasm; do not edit! //
// -------------------------------------------------- //

#pragma once

#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

// ------ //
// joybus //
// ------ //

#define joybus_wrap_target 0
#define joybus_wrap 24
#define joybus_pio_version 0

#define joybus_offset_joybusin 0u
#define joybus_offset_joybusout 10u

static const uint16_t joybus_program_instructions[] = {
            //     .wrap_target
    0xe080, //  0: set    pindirs, 0
    0xe027, //  1: set    x, 7
    0x20a0, //  2: wait   1 pin, 0
    0x2720, //  3: wait   0 pin, 0               [7]
    0xa042, //  4: nop
    0x4001, //  5: in     pins, 1
    0x0042, //  6: jmp    x--, 2
    0xc000, //  7: irq    nowait 0
    0xb842, //  8: nop                    side 1
    0x0001, //  9: jmp    1
    0xf880, // 10: set    pindirs, 0      side 1
    0x7a21, // 11: out    x, 1            side 1 [2]
    0xf081, // 12: set    pindirs, 1      side 0
    0x1231, // 13: jmp    !x, 17          side 0 [2]
    0xf880, // 14: set    pindirs, 0      side 1
    0x1eea, // 15: jmp    !osre, 10       side 1 [6]
    0x0014, // 16: jmp    20
    0xf081, // 17: set    pindirs, 1      side 0
    0x16ea, // 18: jmp    !osre, 10       side 0 [6]
    0xf880, // 19: set    pindirs, 0      side 1
    0xfa80, // 20: set    pindirs, 0      side 1 [2]
    0xf381, // 21: set    pindirs, 1      side 0 [3]
    0xfb80, // 22: set    pindirs, 0      side 1 [3]
    0xc001, // 23: irq    nowait 1
    0x000a, // 24: jmp    10
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program joybus_program = {
    .instructions = joybus_program_instructions,
    .length = 25,
    .origin = -1,
    .pio_version = joybus_pio_version,
#if PICO_PIO_VERSION > 0
    .used_gpio_ranges = 0x0
#endif
};

static inline pio_sm_config joybus_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + joybus_wrap_target, offset + joybus_wrap);
    sm_config_set_sideset(&c, 2, true, false);
    return c;
}

#include "hardware/clocks.h"
static inline void joybus_set_in(bool in, PIO pio, uint sm, uint offset, pio_sm_config *c, uint pin)
{
    // Disable SM
    pio_sm_clear_fifos(pio, sm);
    pio_sm_set_enabled(pio, sm, false);
    if (in)
    {
        pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
        pio_sm_init(pio, sm, offset + joybus_offset_joybusin, c);
    }
    else
    {
        pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
        pio_sm_init(pio, sm, offset + joybus_offset_joybusout, c);
    }
    pio_sm_set_enabled(pio, sm, true);
}
static inline void joybus_program_init(PIO pio, uint sm, uint offset, uint pin, pio_sm_config *c) {
    *c = joybus_program_get_default_config(offset);
    gpio_init(pin);
    // Set this pin's GPIO function (connect PIO to the pad)
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
    sm_config_set_in_pins(c, pin);
    sm_config_set_out_pins(c, pin, 1);
    sm_config_set_jmp_pin(c, pin);
    // Must run 12800000hz
    float div = clock_get_hz(clk_sys) / (4000000);
    sm_config_set_clkdiv(c, div);
    // Set sideset pin
    sm_config_set_sideset_pins(c, pin);
    sm_config_set_in_shift(c, false, true, 8);
    sm_config_set_out_shift(c, false, true, 8);
    // Load our configuration, and jump to the start of the program
    pio_sm_init(pio, sm, offset, c);
    // Set the state machine running
    pio_sm_set_enabled(pio, sm, true);
}

#endif


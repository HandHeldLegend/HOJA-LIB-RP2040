#ifndef HOJA_NIMBLE_NRF52_CLOCK_H
#define HOJA_NIMBLE_NRF52_CLOCK_H

// The crystal oscillator as NimBLE's radio driver asks for it. The HAL starts the crystal at boot
// and leaves it running, so there is nothing to switch.

static inline int nrf52_clock_hfxo_request(void)
{
    return 0;
}

static inline int nrf52_clock_hfxo_release(void)
{
    return 0;
}

#endif

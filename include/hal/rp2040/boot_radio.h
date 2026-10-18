#ifndef HAL_RP2040_BOOT_RADIO_H
#define HAL_RP2040_BOOT_RADIO_H

#include <stdbool.h>

// Whether the radio region (boot_layout.h) holds the radio firmware this build was linked against:
// the radio must not start otherwise. Boards without the bootloader or a radio region always do.
bool boot_radio_ready(void);

#endif

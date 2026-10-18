#ifndef BOOTLOADER_BOOT_RGB_H
#define BOOTLOADER_BOOT_RGB_H

// The LEDs while the bootloader works: every LED breathes slowly in one colour, so the controller is
// plainly alive through an update. Boards without RGB (no BOOT_RGB_PIN) get empty calls. A normal
// boot never lights them; the firmware resets the PIO and takes them over.

#include <stdint.h>

typedef enum
{
    BOOT_RGB_UPDATE_MODE, // Waiting in the USB update mode
    BOOT_RGB_WRITING,     // Writing firmware: a staged update, or over USB
} boot_rgb_state_t;

// Call often: it draws a new frame only when one is due
void boot_rgb_task(boot_rgb_state_t state);

#endif

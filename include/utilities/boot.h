#ifndef UTILITIES_REBOOT_H
#define UTILITIES_REBOOT_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_shared_types.h"
#include "cores/cores.h"

#define BOOT_MEM_MAGIC 0b10110011

// Resolved boot selections after boot_init() (inputs, persisted memory, battery, transport policy).
typedef struct
{
    core_reportformat_t reportformat;
    gamepad_transport_t transport;
    uint16_t            flags; // COREBOOT_FLAG_WLAN, COREBOOT_FLAG_ALTFLASH, etc.
    bool                pairing;
    bool                usb_bootloader;       // always false after boot_init returns (USB path reboots)
    bool                baseband_bootloader;    // ESP32 firmware-update mode
} boot_info_s;

// Persisted across a controlled reboot (e.g. runtime pairing macro).
typedef struct
{
    union {
        struct
        {
            uint8_t magic_num : 8;
            uint8_t report_format : 4;
            uint8_t gamepad_method : 4;
            uint8_t gamepad_pair : 1;
            uint8_t baseband_update : 1; // Reboot into ESP32 baseband firmware-update mode
            uint8_t reserved_1 : 6;
            uint8_t reserved_2 : 8;
        };
        uint32_t val;
    };
} boot_memory_s;

#define BOOT_MEMORY_SIZE sizeof(boot_memory_s)

void boot_clear_memory(void);
void boot_get_memory(boot_memory_s *out);
void boot_set_memory(boot_memory_s *in);

// Run once during startup. Resolves all boot inputs and stores the result internally.
// USB bootloader combo reboots into UF2 and does not return.
void boot_init(void);

// Read-only view of the boot selections made by boot_init(). Valid for the lifetime of the firmware.
const boot_info_s *boot_get_info(void);

// Compare four 12-bit (0..4095) analog samples. Returns a single-bit mask (1<<index) for the
// clear winner, or 0 if ambiguous or below min_activation. The winner must beat the
// second-highest channel by at least min_delta.
uint8_t boot_pick_strongest_analog4(const uint16_t raw[4], uint16_t min_delta, uint16_t min_activation);

#endif // REBOOT_H

#ifndef UTILITIES_REBOOT_H
#define UTILITIES_REBOOT_H

#include <stdint.h>
#include <stdbool.h>
#include "hoja_shared_types.h"

#define BOOT_MEM_MAGIC 0b10110011

typedef struct
{
    union {
        struct
        {
            uint8_t magic_num : 8;
            uint8_t gamepad_mode : 4;
            uint8_t gamepad_method: 4;
            uint8_t gamepad_pair : 1;
            uint8_t reserved_1 : 7;
            uint8_t reserved_2 : 8;
        };
        uint32_t val;
    };
} boot_memory_s;

#define BOOT_MEMORY_SIZE sizeof(boot_memory_s)

void boot_clear_memory();
void boot_get_memory(boot_memory_s *out);
void boot_set_memory(boot_memory_s *in);
void boot_get_mode_method(gamepad_mode_t *mode, gamepad_transport_t *transport, bool *pair);

// Compare four 12-bit (0..4095) analog samples. Returns a single-bit mask (1<<index) for the
// clear winner, or 0 if ambiguous or below min_activation. The winner must beat the
// second-highest channel by at least min_delta.
uint8_t boot_pick_strongest_analog4(const uint16_t raw[4], uint16_t min_delta, uint16_t min_activation);

#endif // REBOOT_H
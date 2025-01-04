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
void boot_get_mode_method(gamepad_mode_t *mode, gamepad_method_t *method, bool *pair);

#endif // REBOOT_H
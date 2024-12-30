#ifndef UTILITIES_REBOOT_H
#define UTILITIES_REBOOT_H

#include <stdint.h>
#include "hoja_shared_types.h"

#define BOOT_MEM_MAGIC 0b10110011

typedef struct
{
    union {
        struct
        {
            uint8_t magic_num : 8;
            uint8_t gamepad_mode : 4;
            uint8_t gamepad_protocol: 4;
            uint8_t reserved_1 : 8;
            uint8_t reserved_2 : 8;
        };
        uint32_t val;
    };
} boot_memory_s;

void boot_get_memory(boot_memory_s *out);
void boot_set_memory(boot_memory_s *in);
gamepad_mode_t boot_get_mode_selection();

#endif // REBOOT_H
#ifndef REBOOT_H
#define REBOOT_H

#include <stdint.h>

typedef enum
{
    ADAPTER_REBOOT_REASON_NULL = 0,
    ADAPTER_REBOOT_REASON_BTSTART,
    ADAPTER_REBOOT_REASON_MODECHANGE,
} hoja_reboot_reason_t;

typedef union
{
    struct
    {
        uint8_t reboot_reason : 8;
        uint8_t gamepad_mode : 4;
        uint8_t gamepad_protocol: 4;
        uint8_t padding_1 : 8;
        uint8_t padding_2 : 8;
    };
    uint32_t value;
} hoja_reboot_memory_u;

void reboot_with_memory(uint32_t value);
uint32_t reboot_get_memory();

#endif // REBOOT_H
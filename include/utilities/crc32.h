#ifndef UTILITIES_CRC32_H
#define UTILITIES_CRC32_H

// CRC-32 as zlib and the config app compute it: reflected polynomial 0xEDB88320, starting from and
// finished with 0xFFFFFFFF. A nibble table keeps it to 64 bytes of flash. Header-only, so the
// bootloader (a separate program) shares it with the firmware.

#include <stdint.h>

static inline uint32_t crc32_compute(const uint8_t *data, uint32_t len)
{
    static const uint32_t table[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u,
        0x5005713Cu, 0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u,
        0xA00AE278u, 0xBDBDF21Cu,
    };
    uint32_t crc = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        crc = (crc >> 4) ^ table[crc & 0x0Fu];
        crc = (crc >> 4) ^ table[crc & 0x0Fu];
    }
    return ~crc;
}

#endif

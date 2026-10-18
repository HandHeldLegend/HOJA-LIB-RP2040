#include "boot_flash.h"

#include "hardware/flash.h"
#include "hardware/sync.h"

void boot_flash_erase(uint32_t offset, uint32_t len)
{
    uint32_t status = save_and_disable_interrupts();
    flash_range_erase(offset, len);
    restore_interrupts(status);
}

void boot_flash_program(uint32_t offset, const uint8_t *data, uint32_t len)
{
    uint32_t status = save_and_disable_interrupts();
    flash_range_program(offset, data, len);
    restore_interrupts(status);
}

void boot_flash_write_sector(uint32_t offset, const uint8_t *sector)
{
    uint32_t status = save_and_disable_interrupts();
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
    flash_range_program(offset, sector, FLASH_SECTOR_SIZE);
    restore_interrupts(status);
}

uint32_t boot_flash_crc32(const uint8_t *data, uint32_t len)
{
    // A nibble table: a few ms for a whole image, in 64 bytes of table
    static const uint32_t table[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
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

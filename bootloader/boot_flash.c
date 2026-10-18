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

#include "hal/flash_hal.h"

#include <string.h>

// Pico SDK specific includes
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "hardware/structs/xip_ctrl.h"
#include "board_config.h"

#if defined(HOJA_BLUETOOTH_DRIVER) && (HOJA_BLUETOOTH_DRIVER == BLUETOOTH_DRIVER_HAL)
#include "pico/btstack_flash_bank.h"
#define BT_IN_USE_FLASH 1 
#define FLASH_START_OFFSET (PICO_FLASH_BANK_STORAGE_OFFSET - FLASH_SECTOR_SIZE)
#else 
// Calculate the last available sectors
#define LAST_FLASH_SECTOR (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define FLASH_START_OFFSET (LAST_FLASH_SECTOR)
#endif 

volatile bool _flash_go = false;
uint8_t *_write_from = NULL;
volatile uint32_t _write_size = 0;
volatile uint32_t _write_offset = 0;

uint32_t _get_sector_offset_read(uint32_t page)
{
    // Calculate offset from the end of flash backwards
    uint32_t target_offset = XIP_BASE + FLASH_START_OFFSET - (page * FLASH_SECTOR_SIZE);
    return target_offset;
}

uint32_t _get_sector_offset_write(uint32_t page)
{
    // Calculate offset from the end of flash backwards
    uint32_t target_offset = FLASH_START_OFFSET - (page * FLASH_SECTOR_SIZE);
    return target_offset;
}

// Write data to flash. Different pages can be used to switch between memory items
// Memory being written must match the device sector size
bool flash_hal_write(uint8_t *data, uint32_t size, uint32_t page) 
{
    if(_flash_go) return false;
    if(size > FLASH_SECTOR_SIZE) return false;

    _write_from = data;
    _write_size = size;
    _write_offset = _get_sector_offset_write(page);
    _flash_go = true;

    // Block until it's done
    while(_flash_go)
    {
        sleep_us(1);
    }

    return true;
}

bool flash_hal_read(uint8_t *out, uint32_t size, uint32_t page) 
{
    if(size > FLASH_SECTOR_SIZE) return false;

    uint32_t offset = _get_sector_offset_read(page);
    const uint8_t *flash_target_contents = (const uint8_t *) (offset);
    memcpy(out, flash_target_contents, size);
    return true;
}

// Should be called from both cores really
void flash_hal_init()
{
    uint core = get_core_num();

    if(core==1)
    {
        // Nothing
        return;
    }
    else 
    {
        multicore_lockout_victim_init();
    }
}

// Run this to apply our flash if
// our flash flag is set
void flash_hal_task()
{
    uint core = get_core_num();
    if(core==1)
    {
        if(_flash_go)
        {
            multicore_lockout_start_blocking();
            // Store interrupts status and disable
            uint32_t ints = save_and_disable_interrupts();
            // Create blank page data
            uint8_t thisPage[FLASH_SECTOR_SIZE] = {0x00};
            // Copy settings into our page buffer
            memcpy(thisPage, _write_from, _write_size);
            // Erase the settings flash sector
            flash_range_erase(_write_offset, FLASH_SECTOR_SIZE);
            // Program the flash sector with our page
            flash_range_program(_write_offset, thisPage, FLASH_SECTOR_SIZE);

            // Restore interrups
            restore_interrupts(ints);

            _write_from = NULL;
            _write_size = 0;
            _write_offset = 0;

            _flash_go = false;

            multicore_lockout_end_blocking();
        }
    }
}

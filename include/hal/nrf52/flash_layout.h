#ifndef HAL_NRF52_FLASH_LAYOUT_H
#define HAL_NRF52_FLASH_LAYOUT_H

// Where an nRF52 board keeps what it saves, in the chip's own flash (4 KB pages). The board's
// board_config.h gives the area, which the firmware's linker script must leave free:
//
//   NRF52_FLASH_STORAGE_OFFSET   its first byte, a whole page
//   NRF52_FLASH_STORAGE_SECTORS  its length in pages
//
// From its end, counting down: flash_hal's page 0 (the settings from before profiles), page 1 (kept
// for Mii data, as on the other boards), and below them the record store (profiles). flash_hal
// writes nowhere else, whatever it is asked.

#include "board_config.h"

#if !defined(NRF52_FLASH_STORAGE_OFFSET) || !defined(NRF52_FLASH_STORAGE_SECTORS)
#error "board_config.h: NRF52_FLASH_STORAGE_OFFSET and NRF52_FLASH_STORAGE_SECTORS are not set"
#endif

#define NRF52_FLASH_SECTOR_SIZE     4096u
#define NRF52_FLASH_STORAGE_SIZE    (NRF52_FLASH_STORAGE_SECTORS * NRF52_FLASH_SECTOR_SIZE)
#define NRF52_FLASH_STORAGE_END     (NRF52_FLASH_STORAGE_OFFSET + NRF52_FLASH_STORAGE_SIZE)

#define NRF52_FLASH_PAGES           2u
// flash_hal's page 0; page n is n sectors below it
#define NRF52_FLASH_PAGE0_OFFSET    (NRF52_FLASH_STORAGE_END - NRF52_FLASH_SECTOR_SIZE)

#define NRF52_FLASH_RECORDS_OFFSET  NRF52_FLASH_STORAGE_OFFSET
#define NRF52_FLASH_RECORDS_SECTORS (NRF52_FLASH_STORAGE_SECTORS - NRF52_FLASH_PAGES)

#endif

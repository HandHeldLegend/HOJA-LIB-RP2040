#ifndef BT_FLASH_BANK_H
#define BT_FLASH_BANK_H

#include "hardware/flash.h"

// Bluetooth pairing storage at the top of flash: the same two sectors BTstack's flash bank uses,
// so settings below it stay where they are whichever stack a build uses
#define BT_FLASH_BANK_SIZE   (FLASH_SECTOR_SIZE * 2u)
#define BT_FLASH_BANK_OFFSET (PICO_FLASH_SIZE_BYTES - BT_FLASH_BANK_SIZE)

#endif

#ifndef OR_FLASH_H
#define OR_FLASH_H
#include <stdint.h>
#include <stdbool.h>

void or_flash_load(uint32_t color);
bool or_flash_handler(uint32_t* output);

#endif
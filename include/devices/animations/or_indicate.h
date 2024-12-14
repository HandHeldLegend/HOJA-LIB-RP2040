#ifndef OR_INDICATE_H
#define OR_INDICATE_H

#include <stdint.h>

void or_indicate_load(uint32_t color);
bool or_indicate_handler(uint32_t* output);

#endif
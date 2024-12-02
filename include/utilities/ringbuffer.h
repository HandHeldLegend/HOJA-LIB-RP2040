#ifndef UTILITIES_RINGBUFFER_H
#define UTILITIES_RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint16_t size;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} ringbuffer_s;

bool ringbuffer_set(void* data_in, void* data_store, uint32_t size, ringbuffer_s* rb);
bool ringbuffer_get(void* data_out, void* data_store, uint32_t size, ringbuffer_s* rb);

#endif
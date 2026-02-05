#ifndef CORES_SNES_H
#define CORES_SNES_H

#include <stdint.h>
#include <stdbool.h>

#include "cores/cores.h"

typedef struct
{
    union
    {
        struct
        {
            uint32_t padding : 20;
            uint32_t r   : 1;
            uint32_t l   : 1;
            uint32_t x   : 1;
            uint32_t a   : 1;
            uint32_t dright  : 1;
            uint32_t dleft   : 1;
            uint32_t ddown   : 1;
            uint32_t dup     : 1;
            uint32_t start   : 1;
            uint32_t select  : 1;
            uint32_t y : 1;
            uint32_t b : 1;
        };
        uint32_t value;
    };
} core_snes_report_s;
#define CORE_SNES_REPORT_SIZE sizeof(core_snes_report_s)

bool core_snes_init(core_params_s *params);

#endif
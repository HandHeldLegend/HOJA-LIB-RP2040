#ifndef NSPI_H
#define NSPI_H

#include "hoja_includes.h"

typedef struct
{
    union
    {
        struct
        {
            uint8_t padding : 4;
            uint8_t r   : 1;
            uint8_t l   : 1;
            uint8_t x   : 1;
            uint8_t a   : 1;
            uint8_t dright  : 1;
            uint8_t dleft   : 1;
            uint8_t ddown   : 1;
            uint8_t dup     : 1;
            uint8_t start   : 1;
            uint8_t select  : 1;
            uint8_t y : 1;
            uint8_t b : 1;
            
            
            
            
            
            
            
            
            
            
            
            
        };
        uint16_t value;
    };
} nspi_input_s;

void nspi_comms_task(uint32_t timestamp, button_data_s *buttons, a_data_s *analog);

#endif

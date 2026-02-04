#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "input/mapper.h"

// This function is called when a report should be generated and sent back. 
// This is usually called at the time-of-flight for the outgoing packet, 
// but sometimes its called on a polled basis 
bool core_get_generated_report(uint8_t *data, uint16_t len)
{

}

void core_set_input(mapper_input_s *input)
{
    
}

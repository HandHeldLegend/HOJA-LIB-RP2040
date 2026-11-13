#ifndef INPUT_TOUCHPAD_H
#define INPUT_TOUCHPAD_H

// Touchpads (a board's trackpads): where a finger is on each, and how hard it presses. A board that
// has them answers cb_hoja_read_touchpads (hoja.h). A pad's click is an ordinary input, read with
// the board's buttons (INPUT_CODE_TP1 the left pad's, INPUT_CODE_TP2 the right's), so it can be
// mapped like any other.

#include <stdbool.h>
#include <stdint.h>

#define TOUCHPAD_EDGE         32767
#define TOUCHPAD_PRESSURE_MAX 32767u

typedef struct
{
    bool touched;
    int16_t x;         // The left edge is -TOUCHPAD_EDGE, the right edge TOUCHPAD_EDGE
    int16_t y;         // The top edge is -TOUCHPAD_EDGE, the bottom edge TOUCHPAD_EDGE
    uint16_t pressure; // Up to TOUCHPAD_PRESSURE_MAX, and never 0 while touched
} touchpad_data_s;

#endif

#ifndef INPUT_CONFIG_H
#define INPUT_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "input_shared_types.h"
#include "driver_define_helper.h"

#define HOJA_INPUT_NAME_LEN        8
#define HOJA_INPUT_MAX_DEFAULT_MAPS  MAPPER_INPUT_COUNT

// One physical input on the board: explicit mapper code, scan type, and the
// label surfaced to the config tool. An all-zero .name disables the entry.
typedef struct
{
    mapper_input_code_t code;
    input_type_t        type;
    char                name[HOJA_INPUT_NAME_LEN];
} hoja_input_slot_cfg_s;

typedef struct
{
    hoja_input_slot_cfg_s slots[MAPPER_INPUT_COUNT];
} hoja_input_cfg_s;

// One default mapping for a gamepad mode: physical input -> protocol output.
// Unlisted inputs stay at that mode's *_CODE_UNUSED value.
typedef struct
{
    mapper_input_code_t input;
    int8_t              output;
} hoja_input_default_map_s;

typedef struct
{
    hoja_input_default_map_s maps[HOJA_INPUT_MAX_DEFAULT_MAPS];
} hoja_input_mode_defaults_s;

// Shorthand for designated initializers in main.c:
// INPUT_DEFAULT(INPUT_CODE_SOUTH, SWITCH_CODE_A)
#define INPUT_DEFAULT(in, out)  { .input = (in), .output = (out) }

static inline bool hoja_input_slot_enabled(const hoja_input_slot_cfg_s *slot)
{
    for(int i = 0; i < HOJA_INPUT_NAME_LEN; i++)
        if(slot->name[i] != '\0')
            return true;
    return false;
}

#endif

#ifndef DEVICES_RGB_H
#define DEVICES_RGB_H

#include <stdint.h>
#include <stdbool.h>
#include "utilities/callback.h"
#include "board_config.h"
#include "devices_shared_types.h"
#include "input_shared_types.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

#if !defined(RGB_DRIVER_REFRESHRATE) 
    #define RGB_DRIVER_REFRESHRATE 60
#endif

#define RGB_TASK_INTERVAL (1000000/RGB_DRIVER_REFRESHRATE) 
#define RGB_FADE_FIXED_SHIFT 16
#define RGB_FADE_FIXED_MULT (uint32_t) (1<<RGB_FADE_FIXED_SHIFT)
#define RGB_FLOAT_TO_FIXED(f) ((uint32_t) (f * RGB_FADE_FIXED_MULT))

// One reactive-mode slot: when the given input is driven, it lights the LEDs of
// the referenced group (0-based index into the groupings table).
typedef struct
{
    mapper_input_code_t input;  // driving input (INPUT_CODE_*)
    uint8_t             group;  // group index this input illuminates
} rgb_reactive_slot_s;

// Board RGB layout + indicator configuration. Lives in hoja_config_s.rgb.
// groupings[g] lists the physical LED indices that belong to group g (-1 marks
// an unused slot). The notification / player indicators and reactive mode all
// reference group indices into this table.
typedef struct
{
    uint8_t group_count;                                        // valid entries in groupings[]
    int8_t  groupings[RGB_MAX_GROUPS][RGB_MAX_LEDS_PER_GROUP];   // LED idx per group (-1 = unused)

    int8_t  notification_group_index;   // group used for notifications (-1 = none)
    uint8_t notification_group_size;    // LEDs in the notification group

    int8_t  player_group_index;         // group used for the player indicator (-1 = none, always 4 LEDs)

    uint8_t             reactive_count;                          // valid entries in reactive[]
    rgb_reactive_slot_s reactive[RGB_MAX_REACTIVE_SLOTS];        // input->group reactive map
} hoja_rgb_cfg_s;

extern int8_t  rgb_led_groups[RGB_MAX_GROUPS][RGB_MAX_LEDS_PER_GROUP];
extern rgb_s   rgb_colors_safe[RGB_MAX_GROUPS];
extern uint8_t rgb_group_count;
#endif

void rgb_set_idle(bool enable); 
void rgb_deinit(callback_t cb);
void rgb_init(int mode, int brightness);
void rgb_task(uint64_t timestamp);

#endif

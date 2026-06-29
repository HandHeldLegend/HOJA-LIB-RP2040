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

// One RGB group: a display name plus the physical LED indices that belong to it.
// Use RGB_GROUP(...) for .leds. An all-zero .name disables the slot; the active
// group count is inferred from the highest enabled index + 1.
typedef struct
{
    char  name[RGB_MAX_GROUP_NAME_LEN];       // empty => group disabled
    int8_t leds[RGB_MAX_LEDS_PER_GROUP];      // physical LED indices (-1 = unused slot)
} rgb_group_cfg_s;

// Maps a physical input code to an RGB group index (for Authentic + React modes).
typedef struct
{
    mapper_input_code_t input;  // driving input (INPUT_CODE_*)
    uint8_t             group;  // group index this input illuminates
} rgb_key_mapping_s;

// Board RGB layout + indicator configuration. Lives in hoja_config_s.rgb.
// groups[] holds up to RGB_MAX_GROUPS entries; notification / player indicators
// and key_mappings[] reference group indices into this table.
typedef struct
{
    rgb_group_cfg_s groups[RGB_MAX_GROUPS];

    int8_t  notification_group_index;   // group used for notifications (-1 = none)
    uint8_t notification_group_size;    // LEDs in the notification group

    int8_t  player_group_index;         // group used for the player indicator (-1 = none, always 4 LEDs)

    uint8_t            key_mapping_count;                       // valid entries in key_mappings[]
    rgb_key_mapping_s  key_mappings[RGB_MAX_KEY_MAPPINGS];      // input code -> RGB group
} hoja_rgb_cfg_s;

static inline bool rgb_group_cfg_enabled(const rgb_group_cfg_s *group)
{
    for(int i = 0; i < RGB_MAX_GROUP_NAME_LEN; i++)
        if(group->name[i] != '\0')
            return true;
    return false;
}

// Highest enabled group index + 1 (0 when every slot is disabled).
static inline uint8_t rgb_config_infer_group_count(const hoja_rgb_cfg_s *cfg)
{
    uint8_t count = 0;
    for(int i = 0; i < RGB_MAX_GROUPS; i++)
    {
        if(rgb_group_cfg_enabled(&cfg->groups[i]))
            count = (uint8_t)(i + 1);
    }
    return count;
}

extern int8_t  rgb_led_groups[RGB_MAX_GROUPS][RGB_MAX_LEDS_PER_GROUP];
extern rgb_s   rgb_colors_safe[RGB_MAX_GROUPS];
extern uint8_t rgb_group_count;
#endif

bool rgb_get_pulsing(rgb_s *out);
void rgb_set_pulsing(rgb_s color);
void rgb_clear_pulsing(void);

bool rgb_get_notification(rgb_s *out);
void rgb_send_notification(rgb_s color);
void rgb_clear_notification(void);

void rgb_set_idle(bool enable); 
bool rgb_deinit(callback_t cb);
void rgb_init(int mode, int brightness);
void rgb_task(uint64_t timestamp);

#endif

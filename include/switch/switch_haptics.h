#ifndef SWITCH_HAPTICS_H
#define SWITCH_HAPTICS_H

#include <stdint.h>
#include <stdbool.h>

#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

typedef enum
{
    Action_Ignore = 0x0,
    Action_Default = 0x1,
    Action_Substitute = 0x2,
    Action_Sum = 0x3,
} Action_t;

typedef struct
{
    Action_t am_action : 8;
    Action_t fm_action : 8;
    float am_offset;
    float fm_offset;
} OLDSwitch5BitCommand_s;

typedef struct
{
    Action_t am_action : 8;
    Action_t fm_action : 8;
    int16_t  am_offset; // AM Index offset
    int16_t  fm_offset; // FM Index offset
} Switch5BitCommand_s;

// This represents the 4 uint8_t bytes of data for a single side or controller
typedef struct
{
    union
    {
        struct
        {
            uint32_t data : 30;
            uint32_t frame_count : 2;
        }; // placeholder

        // Valid for case 0, 1, 2, 3 in the original decompiled code
        // Calling this type 1 pattern
        struct
        {
            uint32_t cmd_hi_2 : 5; // 5-bit amfm hi [2]
            uint32_t cmd_lo_2 : 5; // 5-bit amfm lo [2]
            uint32_t cmd_hi_1 : 5; // 5-bit amfm hi [1]
            uint32_t cmd_lo_1 : 5; // 5-bit amfm lo [1]
            uint32_t cmd_hi_0 : 5; // 5-bit amfm hi [0]
            uint32_t cmd_lo_0 : 5; // 5-bit amfm lo [0]
            uint32_t frame_count : 2;
        } type1; // three5bit
        
        // Valid for case 4 in the original decompiled code
        // Calling this type 2 pattern
        struct
        {
            uint32_t padding        : 2; // Zero padding
            uint32_t freq_hi        : 7; // 7-bit fm hi [0]
            uint32_t amp_hi         : 7; // 7-bit am hi [0]
            uint32_t freq_lo        : 7; // 7-bit fm lo [0]
            uint32_t amp_lo         : 7; // 7-bit am lo [0]
            uint32_t frame_count    : 2;
        } type2; // one7bit

        // Valid for case 5, 6 in the original decompiled code
        // Calling this type 3 pattern
        struct
        {
            uint32_t high_select    : 1; // Whether 7-bit values are high or low
            uint32_t freq_xx_0      : 7; // 7-bit fm hi/lo [0], hi or lo denoted by high_select bit
            uint32_t cmd_hi_1       : 5; // 5-bit amfm hi [1]
            uint32_t cmd_lo_1       : 5; // 5-bit amfm lo [1]
            uint32_t cmd_xx_0       : 5; // 5-bit amfm lo/hi [0], denoted by ~high_select
            uint32_t amp_xx_0       : 7; // 7-bit am hi/lo [0], hi or lo denoted by high_select bit
            uint32_t frame_count    : 2;
        } type3; // two7bit

        struct
        {
            uint32_t high_select    : 1; // Whether 7-bit value is high or low
            uint32_t blank          : 1; // Always 1
            uint32_t freq_select    : 1; // Whether 7-bit value is freq or amp
            uint32_t cmd_hi_2 : 5; // 5-bit amfm hi [2]
            uint32_t cmd_lo_2 : 5; // 5-bit amfm lo [2]
            uint32_t cmd_hi_1 : 5; // 5-bit amfm hi [1]
            uint32_t cmd_lo_1 : 5; // 5-bit amfm lo [1]
            uint32_t xx_xx_0        : 7; // 7-bit am/fm lo/hi [0], denoted by freq_select and high_select bits
            uint32_t frame_count    : 2; // 1 frame
        } type4; // three7bit
    };
} __attribute__((packed)) SwitchHapticPacket_s;

typedef struct 
{
    uint8_t hi_amplitude_idx; // Hi amplitude index
    uint8_t lo_amplitude_idx; // Lo amplitude index
    uint8_t hi_frequency_idx; // Hi frequency index
    uint8_t lo_frequency_idx; // Lo frequency index
} haptic_raw_s;

typedef struct 
{
    uint8_t sample_count;
    haptic_raw_s state;
    haptic_raw_s samples[3];
} haptic_raw_state_s;

void switch_haptics_arbitrary_playback(uint8_t intensity);
void switch_haptics_get_basic(uint8_t amplitude, uint8_t frequency, uint16_t *amp_out, uint16_t *freq_out);
void switch_haptics_init();
void switch_haptics_rumble_translate(const uint8_t *data);

#endif
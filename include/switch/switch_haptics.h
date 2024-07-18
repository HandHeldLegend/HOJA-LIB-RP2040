#ifndef SWITCH_HAPTICS_H
#define SWITCH_HAPTICS_H
#include "hoja_includes.h"

#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

typedef enum
{
    Switch5BitAction_Ignore = 0x0,
    Switch5BitAction_Substitute = 0x2,
    Switch5BitAction_Sum = 0x3,
} Switch5BitAction_t;

typedef struct
{
    Switch5BitAction_t am_action : 8;
    Switch5BitAction_t fm_action : 8;
    float am_offset;
    float fm_offset;
} Switch5BitCommand_s;


// This represents the 4 uint8_t bytes of data for a single side or controller
typedef struct
{
    union
    {
        struct
        {
            uint32_t data : 30;
            uint32_t packet_type : 2;
        };

        struct
        {
            uint32_t : 20;             // Zero padding
            uint32_t amfm_5bit_hi : 5; // 5-bit amfm hi [0]
            uint32_t amfm_5bit_lo : 5; // 5-bit amfm lo [0]
            uint32_t packet_type : 2;  // 1
        } one5bit;

        struct
        {
            uint32_t : 2;             // Zero padding
            uint32_t fm_7bit_hi : 7;  // 7-bit fm hi [0]
            uint32_t am_7bit_hi : 7;  // 7-bit am hi [0]
            uint32_t fm_7bit_lo : 7;  // 7-bit fm lo [0]
            uint32_t am_7bit_lo : 7;  // 7-bit am lo [0]
            uint32_t packet_type : 2; // 1
        } one7bit;

        struct
        {
            uint32_t : 10;               // Zero padding
            uint32_t amfm_5bit_hi_1 : 5; // 5-bit amfm hi [1]
            uint32_t amfm_5bit_lo_1 : 5; // 5-bit amfm lo [1]
            uint32_t amfm_5bit_hi_0 : 5; // 5-bit amfm hi [0]
            uint32_t amfm_5bit_lo_0 : 5; // 5-bit amfm lo [0]
            uint32_t packet_type : 2;    // 2
        } two5bit;

        struct
        {
            uint32_t high_select : 1;    // Whether 7-bit values are high or low
            uint32_t fm_7bit_xx : 7;     // 7-bit fm hi/lo [0], hi or lo denoted by high_select bit
            uint32_t amfm_5bit_hi_1 : 5; // 5-bit amfm hi [1]
            uint32_t amfm_5bit_lo_1 : 5; // 5-bit amfm lo [1]
            uint32_t amfm_5bit_xx_0 : 5; // 5-bit amfm lo/hi [0], denoted by ~high_select
            uint32_t am_7bit_xx : 7;     // 7-bit am hi/lo [0], hi or lo denoted by high_select bit
            uint32_t packet_type : 2;    // 2
        } two7bit;

        struct
        {
            uint32_t amfm_5bit_hi_2 : 5; // 5-bit amfm hi [2]
            uint32_t amfm_5bit_lo_2 : 5; // 5-bit amfm lo [2]
            uint32_t amfm_5bit_hi_1 : 5; // 5-bit amfm hi [1]
            uint32_t amfm_5bit_lo_1 : 5; // 5-bit amfm lo [1]
            uint32_t amfm_5bit_hi_0 : 5; // 5-bit amfm hi [0]
            uint32_t amfm_5bit_lo_0 : 5; // 5-bit amfm lo [0]
            uint32_t packet_type : 2;    // 3
        } three5bit;

        struct
        {
            uint32_t high_select : 1;    // Whether 7-bit value is high or low
            uint32_t blank : 1;          // Always 1
            uint32_t freq_select : 1;    // Whether 7-bit value is freq or amp
            uint32_t amfm_5bit_hi_2 : 5; // 5-bit amfm hi [2]
            uint32_t amfm_5bit_lo_2 : 5; // 5-bit amfm lo [2]
            uint32_t amfm_5bit_hi_1 : 5; // 5-bit amfm hi [1]
            uint32_t amfm_5bit_lo_1 : 5; // 5-bit amfm lo [1]
            uint32_t xx_7bit_xx : 7;     // 7-bit am/fm lo/hi [0], denoted by freq_select and high_select bits
            uint32_t packet_type : 2;    // 1
        } three7bit;
    };
} __attribute__((packed)) SwitchEncodedVibrationSamples_s;

// This can contain the left and right vibration data for the controller
typedef struct
{
    SwitchEncodedVibrationSamples_s left_samples;
    SwitchEncodedVibrationSamples_s right_samples;
} __attribute__((packed)) SwitchEncodedLeftRight_s;

typedef struct
{
    float high_band_freq;
    float high_band_amp;
    float low_band_freq;
    float low_band_amp;
    bool unread; // Use a bool to mark a sample as 'unread', meaning we haven't processed it yet.
} SwitchDecodedVibrationValues_s;

typedef struct
{
    float hi_freq_linear;
    float hi_amp_linear;
    float lo_freq_linear;
    float lo_amp_linear;
} __attribute__((packed)) SwitchLinearVibrationState_s;

typedef struct
{
    uint8_t count;
    SwitchLinearVibrationState_s linear;
    SwitchDecodedVibrationValues_s samples[3];
} SwitchDecodedVibrationSamples_s;

typedef struct
{
    SwitchDecodedVibrationSamples_s left_samples;
    SwitchDecodedVibrationSamples_s right_samples;
} SwitchDecodedLeftRight_s;



void haptics_rumble_translate(const uint8_t *data);

#endif
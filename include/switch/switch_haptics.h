#ifndef SWITCH_HAPTICS_H
#define SWITCH_HAPTICS_H
#include "hoja_includes.h"

/*
Single wave with resonance
BIT PATTERN:
bbbbbbba eeedcccc ihggggfe ?0iiiiii

a	High/Low select bit (0=low channel, 1=high channel)
bbbbbbb	Frequency
cccc	High freq channel amplitude
d	High freq channel switch
eeee	Low freq channel amplitude
f	Low freq channel switch
gggg	pulse1 amplitude
h	pulse1 switch
iiiiiii	pulse2 amplitude

if [a]=0, the [bbbbbbb] is the frequency of "low channel" and "high channel" frequency is 320Hz.
if [a]=1, the [bbbbbbb] is the frequency of "high channel" and "low channel" frequency is 160Hz.
if [d]/[f]/[h] bit is "1", the output of correspond channel is off(become silent).
I cannot figure out the last bit.
*/
typedef struct
{
    union
    {
        struct
        {
            uint8_t low_frequency_selection_bit    : 1;
            uint8_t selected_frequency      : 7;
            uint8_t high_amplitude          : 4;
            uint8_t high_freq_disable       : 1;
            uint8_t low_amplitude           : 4;
            uint8_t low_freq_disable        : 1;
            uint8_t pulse_1_amplitude       : 4;
            uint8_t pulse_1_disable         : 1;
            uint8_t pulse_2_amplitude       : 4;
            uint8_t pulse_2_disable         : 1;
        };
        uint32_t value;
    };
} type_X0_haptic_s; // Single wave with resonance

/*
TYPE 01-01: "dual resonance with 3 pulse"
baaaa?10 eeedcccc ihggggfe 01iiiiii

aaaa	High channel resonant(320Hz) amplitude
b	High channel resonant(320Hz) switch
cccc	Low channel resonant(160Hz) amplitude
d	Low channel resonant(160Hz) switch
eeee	pulse 1 amplitude
f	pulse 1 switch
gggg	pulse 2 amplitude
h	pulse 2 switch
iiiiiii	pulse 3 amplitude

if [b]/[d]/[f]/[h] bit is "1", the output of correspond channel is off(become silent).
I cannot figure out the one bit meaning.
*/
typedef struct
{
    union
    {
        struct
        {
            uint8_t hi_channel_amplitude    : 4; // 320Hz
            uint8_t hi_channel_disable      : 1; 
            uint8_t low_channel_amplitude   : 4; // 160Hz
            uint8_t low_channel_disable     : 1;
            uint8_t pulse_1_amplutide       : 4;
            uint8_t pulse_1_disable         : 1;
            uint8_t pulse_2_amplitude       : 4;
            uint8_t pulse_2_disable         : 1;
            uint8_t pulse_3_amplitude       : 7;
        };
        uint32_t value;
    };
} type_0101_haptic_s; // Dual resonance with 3 pulse

/*
TYPE 01-00: "dual wave"
BIT PATTERN:
aaaaaa00 bbbbbbba dccccccc 01dddddd

aaaaaaa	High channel Frequency
bbbbbbb	High channel Amplitude
ccccccc	Low channel Frequency
ddddddd	Low channel Amplitude
*/
typedef struct
{
    union
    {
        struct
        {
            uint8_t high_frequency          : 7;
            uint8_t high_amplitude          : 7;
            uint8_t low_frequency           : 7;
            uint8_t low_amplitude           : 7;
            uint8_t high_disable            : 1;
            uint8_t low_disable             : 1;
        };
        uint32_t value;
    };
} type_pattern4_haptic_s; // Dual sine wave

/*
cccbaaaa gfeeeedc iiiihggg 11lkkkkj

aaaa	High channel resonant(320Hz) amplitude
b	High channel resonant(320Hz) switch
cccc	Low channel resonant(160Hz) amplitude
d	Low channel resonant(160Hz) switch
eeee	pulse 1/400Hz amplitude
f	pulse 1/400Hz switch
gggg	pulse 2 amplitude
h	pulse 2 switch
iiii	pulse 3 amplitude
j	pulse 3 switch
kkkk	pulse 4 amplitude
l	pulse 4 switch

if [b] bit is "1", the "320Hz" is off and "pulse1/400Hz" channel make "400Hz" sin wave.
if [b] bit is "0", the "320Hz" is on and "pulse1/400Hz" channel make "pulse".
if [d]/[f]/[h]/[j]/[l] bit is "1", the output of correspond channel is off(become silent).
*/
typedef struct
{
    union
    {
        struct
        {
            uint8_t hi_channel_amplitude    : 4; // 320Hz
            uint8_t hi_channel_disable      : 1;
            uint8_t low_channel_amplitude   : 4; // 160Hz
            uint8_t low_channel_disable     : 1;
            uint8_t pulse_1_amplutide       : 4;
            uint8_t pulse_1_disable         : 1;
            uint8_t pulse_2_amplitude       : 4;
            uint8_t pulse_2_disable         : 1;
            uint8_t pulse_3_amplitude       : 4;
            uint8_t pulse_3_disable         : 1;
            uint8_t pulse_4_amplitude       : 4;
            uint8_t pulse_4_disable         : 1;
        };
        uint32_t value;
    };
} type_11_haptic_s; // Dual resonance with 4 pulse

void haptics_rumble_translate(const uint8_t *data);

#endif
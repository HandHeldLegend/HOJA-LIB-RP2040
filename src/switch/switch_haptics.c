#include "switch_haptics.h"
// Thanks to rei-github https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/issues/11#issuecomment-360379786

typedef enum
{
    HAPTIC_PULSE_1,
    HAPTIC_PULSE_2,
    HAPTIC_PULSE_3,
    HAPTIC_PULSE_4,
} haptic_effect_pulse_t;

// LUTS from https://github.com/ndeadly/MissionControl/blob/master/mc_mitm/source/controllers/emulated_switch_controller.cpp#L25
// Frequency in Hz rounded to nearest int
// https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/rumble_data_table.md#frequency-table
const uint16_t RumbleFreqLookup[] = {
    0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f, 0x0030, 0x0031,
    0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0039, 0x003a, 0x003b,
    0x003c, 0x003e, 0x003f, 0x0040, 0x0042, 0x0043, 0x0045, 0x0046, 0x0048,
    0x0049, 0x004b, 0x004d, 0x004e, 0x0050, 0x0052, 0x0054, 0x0055, 0x0057,
    0x0059, 0x005b, 0x005d, 0x005f, 0x0061, 0x0063, 0x0066, 0x0068, 0x006a,
    0x006c, 0x006f, 0x0071, 0x0074, 0x0076, 0x0079, 0x007b, 0x007e, 0x0081,
    0x0084, 0x0087, 0x0089, 0x008d, 0x0090, 0x0093, 0x0096, 0x0099, 0x009d,
    0x00a0, 0x00a4, 0x00a7, 0x00ab, 0x00ae, 0x00b2, 0x00b6, 0x00ba, 0x00be,
    0x00c2, 0x00c7, 0x00cb, 0x00cf, 0x00d4, 0x00d9, 0x00dd, 0x00e2, 0x00e7,
    0x00ec, 0x00f1, 0x00f7, 0x00fc, 0x0102, 0x0107, 0x010d, 0x0113, 0x0119,
    0x011f, 0x0125, 0x012c, 0x0132, 0x0139, 0x0140, 0x0147, 0x014e, 0x0155,
    0x015d, 0x0165, 0x016c, 0x0174, 0x017d, 0x0185, 0x018d, 0x0196, 0x019f,
    0x01a8, 0x01b1, 0x01bb, 0x01c5, 0x01ce, 0x01d9, 0x01e3, 0x01ee, 0x01f8,
    0x0203, 0x020f, 0x021a, 0x0226, 0x0232, 0x023e, 0x024b, 0x0258, 0x0265,
    0x0272, 0x0280, 0x028e, 0x029c, 0x02ab, 0x02ba, 0x02c9, 0x02d9, 0x02e9,
    0x02f9, 0x030a, 0x031b, 0x032c, 0x033e, 0x0350, 0x0363, 0x0376, 0x0389,
    0x039d, 0x03b1, 0x03c6, 0x03db, 0x03f1, 0x0407, 0x041d, 0x0434, 0x044c,
    0x0464, 0x047d, 0x0496, 0x04af, 0x04ca, 0x04e5};
const size_t RumbleFreqLookupSize = 159;

// Floats from dekunukem repo normalised and scaled by function used by yuzu
// https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/rumble_data_table.md#amplitude-table
// https://github.com/yuzu-emu/yuzu/blob/d3a4a192fe26e251f521f0311b2d712f5db9918e/src/input_common/sdl/sdl_impl.cpp#L429
const float RumbleAmpLookup[] = {
    0.000000, 0.120576, 0.137846, 0.146006, 0.154745, 0.164139, 0.174246,
    0.185147, 0.196927, 0.209703, 0.223587, 0.238723, 0.255268, 0.273420,
    0.293398, 0.315462, 0.321338, 0.327367, 0.333557, 0.339913, 0.346441,
    0.353145, 0.360034, 0.367112, 0.374389, 0.381870, 0.389564, 0.397476,
    0.405618, 0.413996, 0.422620, 0.431501, 0.436038, 0.440644, 0.445318,
    0.450062, 0.454875, 0.459764, 0.464726, 0.469763, 0.474876, 0.480068,
    0.485342, 0.490694, 0.496130, 0.501649, 0.507256, 0.512950, 0.518734,
    0.524609, 0.530577, 0.536639, 0.542797, 0.549055, 0.555413, 0.561872,
    0.568436, 0.575106, 0.581886, 0.588775, 0.595776, 0.602892, 0.610127,
    0.617482, 0.624957, 0.632556, 0.640283, 0.648139, 0.656126, 0.664248,
    0.672507, 0.680906, 0.689447, 0.698135, 0.706971, 0.715957, 0.725098,
    0.734398, 0.743857, 0.753481, 0.763273, 0.773235, 0.783370, 0.793684,
    0.804178, 0.814858, 0.825726, 0.836787, 0.848044, 0.859502, 0.871165,
    0.883035, 0.895119, 0.907420, 0.919943, 0.932693, 0.945673, 0.958889,
    0.972345, 0.986048, 1.000000};
const size_t RumbleAmpLookupSize = 101;

bool haptic_disabled_check(uint8_t *data)
{
    if (data[0] & 0b00000001)
    {
        if (data[3] & 0b01000000)
        {
            return true;
        }
    }
    return false;
}

float haptics_7bit_process_frequency(uint8_t value, bool low_frequency)
{
    if (low_frequency)
    {
        value &= 0b1111111;
        return (float)RumbleFreqLookup[value];
    }
    else
    {
        value += 31;
        return (float)RumbleFreqLookup[value];
    }
}

float haptics_7bit_process_amplitude(uint8_t value)
{
    if (value > 100)
        value = 100;
    return RumbleAmpLookup[value];
}

float haptics_4bit_process_amplitude(uint8_t value)
{
    const float lut_4bit[16] = {
        0.0f, 1.0f,
        0.713429339f,
        0.510491764f,
        0.364076932f,
        0.263212876f,
        0.187285343f,
        0.128740086f,
        0.096642284f,
        0.065562582f,
        0.047502641f,
        0.035863824f,
        0, 0, 0, 0};

    value &= 0b1111;

    return lut_4bit[value];
}

void haptics_process_pattern_4(const uint8_t *data, rumble_data_s *rumble)
{
    /*
    TYPE 01-00: "dual wave"
    BIT PATTERN:
    aaaaaa00 bbbbbbba dccccccc 01dddddd

    aaaaaaa	High channel Frequency
    bbbbbbb	High channel Amplitude
    ccccccc	Low channel Frequency
    ddddddd	Low channel Amplitude
    */

    uint16_t high_frequency = ((data[1] & 0x1) << 7) | (data[0] >> 2);
    uint8_t high_amplitude = (data[1] >> 1);

    uint16_t low_frequency = (data[2] & 0x7F);
    uint8_t low_amplitude = ((data[3] & 0x3F) << 1) | ((data[2] & 0x80) >> 7);

    float hf = haptics_7bit_process_frequency(high_frequency, false);
    float ha = haptics_7bit_process_amplitude(high_amplitude);
    float lf = haptics_7bit_process_frequency(low_frequency, true);
    float la = haptics_7bit_process_amplitude(low_amplitude);

    rumble->amplitude_high = ha;
    rumble->amplitude_low = la;
    rumble->frequency_high = hf;
    rumble->frequency_low = lf;
}

void haptics_process_pattern_5(const uint8_t *data, rumble_data_s *rumble)
{
    bool high_f_select = data[0] & 1;
    uint16_t    high_f_code = 0;
    uint8_t     high_amp_code = 0;
    uint16_t    low_f_code = 0;
    uint8_t     low_amp_code = 0;

    float high_f;
    float high_a;
    float low_f;
    float low_a;

    bool low_f_disable  = data[2] & 0b00000010;
    bool hi_f_disable   = data[1] & 0b00010000;

    if (high_f_select)
    {
        // printf("HF Bit ON\n");
        high_f_code     = (data[0] >> 1);
        high_amp_code   = (data[1] & 0xF) << 3;

        high_f = (!hi_f_disable) ? haptics_7bit_process_frequency(high_f_code, false) : 0;
        high_a = (!hi_f_disable) ? haptics_4bit_process_amplitude(high_amp_code) : 0;

        // printf("LF is 160hz.");
        low_f           = (!low_f_disable) ? 160.0f : 0;
        low_amp_code    = (((data[2] & 0x1) << 3) | ((data[1] & 0xE0) >> 5)) << 3;
        low_a           = (!low_f_disable) ? haptics_4bit_process_amplitude(low_amp_code) : 0;
    }

    else
    {
        // printf("LF Bit ON\n");
        low_f_code = (data[0] >> 1);
        high_amp_code   = (data[1] & 0xF) << 3;

        low_f = (!low_f_disable) ? haptics_7bit_process_frequency(low_f_code, true) : 0;
        high_a = (!hi_f_disable) ? haptics_4bit_process_amplitude(high_amp_code) : 0;

        high_f = (!hi_f_disable) ? 320.0f : 0;
        low_amp_code = (((data[2] & 0x1) << 3) | ((data[1] & 0xE0) >> 5)) << 3;

        low_a = (!low_f_disable) ? haptics_4bit_process_amplitude(low_amp_code) : 0;
    }

    rumble->amplitude_high  = high_a;
    rumble->amplitude_low   = low_a;
    rumble->frequency_high  = high_f;
    rumble->frequency_low   = low_f;
}

// Translate and handle rumble
// Big thanks to hexkyz for some info on Discord
void haptics_rumble_translate(const uint8_t *data)
{

    static rumble_data_s output_data = {0};

    // Extract modulation indicator in byte 3
    // v9 / result
    uint8_t upper2 = data[3] >> 6;
    int result = (upper2) > 0;

    uint16_t hfcode = 0;
    uint16_t lacode = 0;
    uint16_t lfcode = 0;
    uint16_t hacode = 0;

    float fhi = 0;
    float ahi = 0;

    float flo = 0;
    float alo = 0;

    // Single wave w/ resonance
    // v14
    bool high_f_select = false;

    // v4
    uint8_t patternType = 0x00;

    // Handle different modulation types
    if (upper2) // If upper 2 bits exist
    {
        if (upper2 != 1)
        {
            if (upper2 == 2) // Value is 2
            {
                // Check if lower 10 bytes of first two bytes exists
                uint16_t lower10 = (data[1] << 8) | data[0];
                if (lower10 & 0x03FF)
                {
                    patternType = 5;
                }
                else
                    patternType = 2;
            }
            else if (upper2 == 3) // Value is 3
            {
                patternType = 3;
            }

            goto LABEL_24;
        }

        // Upper 2 bits are unset...
        // The format of the rumble is treated differently.
        uint16_t lower12 = (data[2] << 8 | data[1]);

        if (!(lower12 & 0x0FFF)) // if lower 12 bits are empty
        {
            patternType = 1;
            goto LABEL_24;
        }

        // Lower 12 bits are set
        // Format is different again

        // Check lower 2 bits of byte 0
        // v10
        uint16_t lower2 = data[0] & 0x3;

        if (!lower2) // Lower 2 is blank (0x00)
        {
            patternType = 4;
            goto LABEL_24;
        }
        if ((lower2 & 2) != 0) // Lower 2, bit 1 is set (0x02 or 0x03)
        {
            result = 3;
            patternType = 7;
            goto LABEL_24;
        }

        // Lower 2, bit 0 is set only (0x01)
        // Do nothing?
        return;
    }

    // Check if there is no int value present in the data

    // if (!(4 * *(int *)data)) {
    //     return;
    // }
    // printf("Int found\n");

    result = 3;
    patternType = 6;

LABEL_24:

    // Unpack codes based on modulation type
    switch (patternType)
    {
    case 0:
    case 1:
    case 2:
    case 3:
        // printf("Case 0-3\n");
        /*
        amFmCodes[0] = (v9 >> 1) & 0x1F;
        amFmCodes[1] = (*((unsigned short *)data + 1) >> 4) & 0x1F;
        amFmCodes[2] = (*(unsigned short *)(data + 1) >> 7) & 0x1F;
        amFmCodes[3] = (data[1] >> 2) & 0x1F;
        amFmCodes[4] = (*(unsigned short *)data >> 5) & 0x1F;
        v12 = *data & 0x1F;
        */
        output_data.amplitude_high = 0;
        output_data.amplitude_low = 0;
        break;

    // Dual frequency mode
    case 4:
        // printf("Case 4\n");

        haptics_process_pattern_4(data, &output_data);

        break;

    // Seems to be single wave mode
    case 5:
    case 6:
        // printf("Case 5-6\n");
        
        haptics_process_pattern_5(data, &output_data);

        break;

    // Some kind of operation codes? Also contains frequency
    case 7:
        // printf("Case 7\n");
        /*
        v18 = *data;
        v19 = v18 & 1;
        v20 = ((v18 >> 2) & 1) == 0;
        v21 = (*((unsigned short *)data + 1) >> 7) & 0x7F;

        if (v20)
            v22 = v21 | 0x80;
        else
            v22 = (v21 << 8) | 0x8000;

        if (v19)
            v23 = 24;
        else
            v23 = v22;
        amFmCodes[0] = v23;
        if (!v19)
            v22 = 24;
        amFmCodes[1] = v22;
        amFmCodes[2] = (data[2] >> 2) & 0x1F;
        amFmCodes[3] = (*(unsigned short *)(data + 1) >> 5) & 0x1F;
        amFmCodes[4] = data[1] & 0x1F;
        v12 = *data >> 3;*/
        return;
        break;

    default:
        break;
    }

    hoja_rumble_set(output_data.frequency_high, output_data.amplitude_high, output_data.frequency_low, output_data.amplitude_low);
}

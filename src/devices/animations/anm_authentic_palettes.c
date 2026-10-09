#include "devices/animations/anm_authentic_palettes.h"
#include "input_shared_types.h"
#include "hoja.h"

#if defined(HOJA_RGB_DRIVER) && (HOJA_RGB_DRIVER > 0)

#define RGB_HEX(hex) ((rgb_s){.r = (uint8_t)(((hex) >> 16) & 0xFF), .g = (uint8_t)(((hex) >> 8) & 0xFF), .b = (uint8_t)((hex) & 0xFF)})

#define COLOR_LIGHT_GRAY   RGB_HEX(0xF0F0F0)
#define COLOR_SFC_YELLOW   RGB_HEX(0xF5D400)
#define COLOR_SFC_RED      RGB_HEX(0xFF0000)
#define COLOR_SFC_GREEN    RGB_HEX(0x00FF00)
#define COLOR_GC_TEAL      RGB_HEX(0x00F5A3)
#define COLOR_GC_RED       RGB_HEX(0xFF3838)
#define COLOR_GC_Z         RGB_HEX(0x7300FF)
// WS2812 pure #0000FF can read purple; a small green boost reads truer on LED.
#define COLOR_LED_BLUE     RGB_HEX(0x0032FF)

static rgb_s _mix_rgb(rgb_s color, rgb_s toward, uint8_t toward_weight)
{
    uint8_t color_weight = (uint8_t)(255 - toward_weight);
    return (rgb_s){
        .r = (uint8_t)(((uint16_t)color.r * color_weight + (uint16_t)toward.r * toward_weight) / 255),
        .g = (uint8_t)(((uint16_t)color.g * color_weight + (uint16_t)toward.g * toward_weight) / 255),
        .b = (uint8_t)(((uint16_t)color.b * color_weight + (uint16_t)toward.b * toward_weight) / 255),
    };
}

// ~75% saturation for SInput face buttons.
static rgb_s _sinput_color(rgb_s saturated)
{
    return _mix_rgb(saturated, COLOR_LIGHT_GRAY, 64);
}

rgb_s anm_authentic_fallback_color(void)
{
    return COLOR_LIGHT_GRAY;
}

rgb_s anm_authentic_stick_color(core_reportformat_t format)
{
    (void)format;
    return COLOR_LIGHT_GRAY;
}

typedef enum
{
    FACE_LETTER_A,
    FACE_LETTER_B,
    FACE_LETTER_X,
    FACE_LETTER_Y,
} face_letter_t;

// The letter printed at each face position (South, East, West, North) for each SEWN layout
static const face_letter_t _sewn_letters[3][4] = {
    [SEWN_LAYOUT_ABXY] = {FACE_LETTER_A, FACE_LETTER_B, FACE_LETTER_X, FACE_LETTER_Y}, // Xbox
    [SEWN_LAYOUT_BAYX] = {FACE_LETTER_B, FACE_LETTER_A, FACE_LETTER_Y, FACE_LETTER_X}, // Nintendo
    [SEWN_LAYOUT_AXBY] = {FACE_LETTER_A, FACE_LETTER_X, FACE_LETTER_B, FACE_LETTER_Y}, // GameCube
};

// position: 0..3 for South, East, West, North
static face_letter_t _face_letter(uint8_t position)
{
    const hoja_config_s *cfg = hoja_config_get();
    uint8_t layout = cfg ? cfg->sewn_layout : (uint8_t)SEWN_LAYOUT_ABXY;
    if(layout > (uint8_t)SEWN_LAYOUT_AXBY)
        layout = (uint8_t)SEWN_LAYOUT_ABXY;
    return _sewn_letters[layout][position & 3];
}

// Switch / SNES: A=red, B=yellow, X=blue, Y=green
static rgb_s _nintendo_letter_color(face_letter_t letter)
{
    switch(letter)
    {
        default:
        case FACE_LETTER_A: return COLOR_SFC_RED;
        case FACE_LETTER_B: return COLOR_SFC_YELLOW;
        case FACE_LETTER_X: return COLOR_LED_BLUE;
        case FACE_LETTER_Y: return COLOR_SFC_GREEN;
    }
}

// Xbox: A=green, B=red, X=blue, Y=yellow (saturated SFC primaries, LED-tuned blue)
static rgb_s _xbox_letter_color(face_letter_t letter)
{
    switch(letter)
    {
        default:
        case FACE_LETTER_A: return COLOR_SFC_GREEN;
        case FACE_LETTER_B: return COLOR_SFC_RED;
        case FACE_LETTER_X: return COLOR_LED_BLUE;
        case FACE_LETTER_Y: return COLOR_SFC_YELLOW;
    }
}

// Switch / SNES output codes 0..3 are A, B, X, Y
static bool _nintendo_abxy_palette(int8_t output_code, rgb_s *out)
{
    if(output_code < 0 || output_code > 3)
        return false;
    *out = _nintendo_letter_color((face_letter_t)output_code);
    return true;
}

static bool _xinput_palette(int8_t output_code, rgb_s *out)
{
    switch(output_code)
    {
        case XINPUT_CODE_A: *out = _xbox_letter_color(FACE_LETTER_A); return true;
        case XINPUT_CODE_B: *out = _xbox_letter_color(FACE_LETTER_B); return true;
        case XINPUT_CODE_X: *out = _xbox_letter_color(FACE_LETTER_X); return true;
        case XINPUT_CODE_Y: *out = _xbox_letter_color(FACE_LETTER_Y); return true;
        default: return false;
    }
}

// SInput outputs are positions: each takes the letter printed there on this board
static bool _sinput_palette(int8_t output_code, rgb_s *out)
{
    if(output_code < SINPUT_CODE_SOUTH || output_code > SINPUT_CODE_NORTH)
        return false;
    *out = _sinput_color(_xbox_letter_color(_face_letter((uint8_t)(output_code - SINPUT_CODE_SOUTH))));
    return true;
}

bool anm_authentic_face_color(core_reportformat_t format, mapper_input_code_t physical, rgb_s *out)
{
    if(!out || physical < INPUT_CODE_SOUTH || physical > INPUT_CODE_NORTH)
        return false;

    const face_letter_t letter = _face_letter((uint8_t)(physical - INPUT_CODE_SOUTH));
    switch(format)
    {
        case CORE_REPORTFORMAT_SWPRO:
        case CORE_REPORTFORMAT_SNES:
            *out = _nintendo_letter_color(letter);
            return true;

        case CORE_REPORTFORMAT_XINPUT:
            *out = _xbox_letter_color(letter);
            return true;

        case CORE_REPORTFORMAT_SINPUT:
            *out = _sinput_color(_xbox_letter_color(letter));
            return true;

        default:
            return false;
    }
}

static bool _gamecube_palette(int8_t output_code, rgb_s *out)
{
    switch(output_code)
    {
        case GAMECUBE_CODE_A: *out = COLOR_GC_TEAL;    return true;
        case GAMECUBE_CODE_B: *out = COLOR_GC_RED;     return true;
        case GAMECUBE_CODE_Z: *out = COLOR_GC_Z;      return true;
        case GAMECUBE_CODE_X:
        case GAMECUBE_CODE_Y: *out = COLOR_LIGHT_GRAY; return true;
        default: return false;
    }
}

static bool _n64_palette(int8_t output_code, rgb_s *out)
{
    switch(output_code)
    {
        case N64_CODE_A:      *out = COLOR_LED_BLUE;   return true;
        case N64_CODE_B:      *out = COLOR_SFC_GREEN;  return true;
        case N64_CODE_CUP:
        case N64_CODE_CDOWN:
        case N64_CODE_CLEFT:
        case N64_CODE_CRIGHT: *out = COLOR_SFC_YELLOW; return true;
        default: return false;
    }
}

bool anm_authentic_palette_color(core_reportformat_t format, int8_t output_code, rgb_s *out)
{
    if(!out || output_code < 0)
        return false;

    switch(format)
    {
        case CORE_REPORTFORMAT_SWPRO:
        case CORE_REPORTFORMAT_SNES:
            return _nintendo_abxy_palette(output_code, out);

        // Wii Remote, Nunchuk and Classic Controller Pro buttons are all plain white/gray,
        // so every Wii output uses the light-gray fallback.
        case CORE_REPORTFORMAT_WII:
            return false;

        case CORE_REPORTFORMAT_XINPUT:
            return _xinput_palette(output_code, out);

        case CORE_REPORTFORMAT_SINPUT:
            return _sinput_palette(output_code, out);

        case CORE_REPORTFORMAT_GAMECUBE:
        case CORE_REPORTFORMAT_SLIPPI:
            return _gamecube_palette(output_code, out);

        case CORE_REPORTFORMAT_N64:
            return _n64_palette(output_code, out);

        default:
            return false;
    }
}

#endif

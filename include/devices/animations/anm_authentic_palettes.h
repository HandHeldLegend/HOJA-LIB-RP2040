#ifndef ANM_AUTHENTIC_PALETTES_H
#define ANM_AUTHENTIC_PALETTES_H

#include "devices_shared_types.h"
#include "hoja_shared_types.h"
#include <stdbool.h>
#include <stdint.h>

// Light gray for unmapped / neutral controls (dpad, triggers, etc.)
rgb_s anm_authentic_fallback_color(void);

// Stick shell color for the active gamepad mode.
rgb_s anm_authentic_stick_color(core_reportformat_t format);

// Lookup an era-authentic color for a protocol output code in the active mode.
// Returns false when no palette entry exists (caller should use fallback).
bool anm_authentic_palette_color(core_reportformat_t format, int8_t output_code, rgb_s *out);

#endif

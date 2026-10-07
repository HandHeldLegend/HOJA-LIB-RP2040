/**
 * @file nwii_lib.c
 * @brief NWII-LIB-HID API entry points and weak default platform hooks.
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 *
 * TRADEMARK AND AFFILIATION DISCLAIMER:
 * This library is not affiliated, associated, authorized, endorsed by, or in any way officially
 * connected with Nintendo Co., Ltd., or any of its subsidiaries or its affiliates. Nintendo, Wii and
 * related marks are trademarks of their respective owners.
 */

#include "nwii_lib.h"
#include "nwii_lib_protocol.h"

#include <stddef.h>

bool nwii_api_init(const nwii_device_config_s *cfg)
{
    const nwii_extension_t extension = cfg ? cfg->extension : NWII_EXTENSION_NONE;
    if (extension >= NWII_EXTENSION_MAX)
    {
        return false;
    }

    nwii_protocol_init(extension, cfg ? cfg->motion_plus : false);
    return true;
}

void nwii_api_connection_reset(void)
{
    nwii_protocol_connection_reset();
}

bool nwii_api_generate_inputreport(uint8_t data[NWII_INPUT_REPORT_MAX], uint8_t *len)
{
    return nwii_protocol_generate_inputreport(data, len);
}

void nwii_api_output_tunnel(const uint8_t *data, uint16_t len)
{
    nwii_protocol_ingest_outputreport(data, len);
}

void nwii_api_set_extension(nwii_extension_t extension)
{
    nwii_protocol_set_extension(extension);
}

nwii_extension_t nwii_api_get_extension(void)
{
    return nwii_protocol_get_extension();
}

/*
 * Platform hooks (declared in nwii_lib.h): weak definitions — firmware should supply strong replacements.
 */
__attribute__((weak)) void nwii_api_hook_get_input(nwii_input_s *out)
{
    (void)out;
}

__attribute__((weak)) void nwii_api_hook_set_rumble(bool enable)
{
    (void)enable;
}

__attribute__((weak)) void nwii_api_hook_set_leds(uint8_t led_mask)
{
    (void)led_mask;
}

__attribute__((weak)) void nwii_api_hook_get_power(nwii_power_s *out)
{
    if (!out) return;
    out->level = 0xFF;
    out->low = false;
}

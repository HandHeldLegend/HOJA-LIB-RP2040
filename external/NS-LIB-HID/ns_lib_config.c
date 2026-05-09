/**
 * @file ns_lib_config.c
 * @brief Global NS-LIB-HID device configuration storage and helpers (@ref ns_lib_config.h).
 *
 * Copyright (c) 2026 Mitchell Cairns, Hand Held Legend, LLC.
 *
 * Licensed under the Creative Commons Attribution-NonCommercial 4.0 International License
 * (CC BY-NC 4.0). Non-commercial use with attribution; commercial licensing from Hand Held Legend.
 * Full terms: https://creativecommons.org/licenses/by-nc/4.0/legalcode — see LICENSING.md in this folder
 *
 * SPDX-License-Identifier: CC-BY-NC-4.0
 *
 * TRADEMARK AND AFFILIATION DISCLAIMER:
 * This library is not affiliated, associated, authorized, endorsed by, or in any way officially
 * connected with Nintendo Co., Ltd., or any of its subsidiaries or its affiliates. Nintendo and
 * related marks are trademarks of their respective owners.
 */

#include <stddef.h>
#include <string.h>

#include "ns_lib_config.h"
#include "ns_lib_motion.h"

static ns_device_config_s s_ns_device_config;
/** Updated only from ns_device_config_set / ns_device_config_reset — avoids re-validation on hot paths. */
static uint8_t s_ns_config_ready;
static const float NS_CFG_DEFAULT_GYRO_FS_DPS = 2000.0f;

ns_config_status_t ns_device_config_validate(const ns_device_config_s *cfg)
{
    if (!cfg)
    {
        return NS_CONFIG_INVALID_ARG;
    }
    if (cfg->type == NS_DEVTYPE_UNDEFINED)
    {
        return NS_CONFIG_NOT_CONFIGURED;
    }
    if (cfg->transport == NS_TRANSPORT_UNDEFINED)
    {
        return NS_CONFIG_NOT_CONFIGURED;
    }
    return NS_CONFIG_OK;
}

ns_config_status_t ns_device_config_set(const ns_device_config_s *cfg)
{
    ns_config_status_t st = ns_device_config_validate(cfg);
    if (st != NS_CONFIG_OK)
    {
        return st;
    }
    memcpy(&s_ns_device_config, cfg, sizeof(s_ns_device_config));
    if (s_ns_device_config.gyro_full_scale_dps <= 0.0f)
    {
        s_ns_device_config.gyro_full_scale_dps = NS_CFG_DEFAULT_GYRO_FS_DPS;
    }
    s_ns_device_config.gyro_rad_per_lsb =
        ns_motion_calculate_rps(s_ns_device_config.gyro_full_scale_dps);
    s_ns_config_ready = 1u;
    return NS_CONFIG_OK;
}

void ns_device_config_get(ns_device_config_s *out)
{
    if (!out)
    {
        return;
    }
    memcpy(out, &s_ns_device_config, sizeof(*out));
}

void ns_device_config_reset(void)
{
    memset(&s_ns_device_config, 0, sizeof(s_ns_device_config));
    s_ns_config_ready = 0u;
}

int ns_device_config_is_ready(void)
{
    return (int)s_ns_config_ready;
}


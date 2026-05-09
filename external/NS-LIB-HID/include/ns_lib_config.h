/**
 * @file ns_lib_config.h
 * @brief Global device configuration storage for NS-LIB-HID (`ns_lib_config.c`).
 *
 * Holds a single @ref ns_device_config_s instance set via @ref ns_device_config_set,
 * read with @ref ns_device_config_get, and cleared by @ref ns_device_config_reset.
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

#ifndef NS_LIB_CONFIG_H
#define NS_LIB_CONFIG_H

#include <stdint.h>

#include "ns_lib_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check whether @p cfg has required fields for active use.
 *
 * @param cfg Candidate configuration (may be NULL).
 * @return `NS_CONFIG_OK` if usable; `NS_CONFIG_INVALID_ARG` or `NS_CONFIG_NOT_CONFIGURED` otherwise.
 */
ns_config_status_t ns_device_config_validate(const ns_device_config_s *cfg);

/**
 * @brief Replace the library’s stored configuration (copy).
 *
 * Applies default gyro DPS/LSB scaling when an axis is unset (≤ 0). Marks config ready on success.
 *
 * @param cfg Valid configuration (same rules as @ref ns_device_config_validate).
 * @return `NS_CONFIG_OK` on success; validation status otherwise.
 */
ns_config_status_t ns_device_config_set(const ns_device_config_s *cfg);

/**
 * @brief Copy the current stored configuration into @p out.
 *
 * @param[out] out Destination; no-op if NULL.
 */
void ns_device_config_get(ns_device_config_s *out);

/** @brief Clear stored configuration and mark not ready. */
void ns_device_config_reset(void);

/**
 * @brief Nonzero after a successful @ref ns_device_config_set.
 *
 * @return 1 if ready, 0 otherwise.
 */
int ns_device_config_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* NS_LIB_CONFIG_H */

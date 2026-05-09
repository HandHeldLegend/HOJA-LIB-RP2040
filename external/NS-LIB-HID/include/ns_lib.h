/**
 * @file ns_lib.h
 * @brief User-facing NS-LIB-HID API: configuration, host weak callbacks, platform hooks, and protocol
 *        entry points.
 */

#ifndef NS_LIB_API_H
#define NS_LIB_API_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ns_lib_types.h"
#include "ns_lib_analog.h"
#include "ns_lib_config.h"
#include "ns_lib_haptics.h"
#include "ns_lib_motion.h"
#include "ns_lib_spi.h"


#ifdef __cplusplus
extern "C" {
#endif

/* --- Weak callbacks (user override points) --- */

/**
 * @brief Decoded HD-rumble index tuples callback (weak, user-overridable).
 * @param pairs Up to three decoded samples (index-only values).
 * @param pair_count Number of valid entries in @p pairs (0..3).
 */
void ns_set_haptic_indices_cb(const ns_lib_haptic_raw_sample_s *pairs, uint8_t pair_count);

/**
 * @brief Host-set player LED callback (weak, user-overridable).
 *
 * @param player_leds Host player slot `1`–`8` from SET_PLAYER decoding, or `-1` when disconnected /
 *                    no valid assignment (unknown bitmask / cleared LEDs).
 */
void ns_set_led_cb(int player_leds);

/** @brief Host power-state request callback (weak, user-overridable). */
void ns_set_power_cb(uint8_t shutdown);

/** @brief USB pairing data callback (weak, user-overridable). */
void ns_set_usbpair_cb(ns_usbpair_s pairing_data);

/** @brief IMU Mode Setter. */
void ns_set_imumode_cb(ns_imu_mode_t mode);

/**
 * @brief Fill battery / USB status for input report byte @c 1 (weak, user-overridable).
 *
 * Set @ref ns_powerstatus_s bitfields (or assign @c out->val directly). Layout matches HOJA
 * `bat_status_u` / Switch Pro Controller wire format.
 *
 * @param out Output status; ignored if NULL.
 */
void ns_get_powerstatus_cb(ns_powerstatus_s *out);

/**
 * @brief Fill packed input byte groups when needed (weak, user-overridable).
 * @param out Output button/stick packed bytes.
 */
void ns_get_inputdata_cb(ns_inputdata_s *out);

/**
 * @brief Fill one IMU sample for standard mode reports (weak, user-overridable).
 * @param out Output gyro/accel sample.
 */
void ns_get_imu_standard_cb(ns_gyrodata_s *out);

/**
 * @brief Fill quaternion state for IMU mode-2 reports (weak, user-overridable).
 *
 * Default weak implementation integrates from `ns_get_imu_raw_cb()`.
 */
void ns_get_imu_quaternion_cb(ns_quaternion_s *out);

/**
 * @defgroup ns_platform_hooks Platform hooks (weak)
 *
 * Optional firmware-provided symbols with the same names replace these defaults (GNU `weak`, or your
 * toolchain’s equivalent). Used for time, RNG, and IMU report packing—**not** host commands. Battery /
 * USB byte @c [1] comes from @ref ns_get_powerstatus_cb only.
 * @{
 */

/**
 * @brief Monotonic time in milliseconds (weak).
 *
 * Default advances a counter each call. Firmware should set `*ms` from a hardware timer or RTOS tick.
 *
 * @param[out] ms Elapsed milliseconds; ignored if NULL.
 */
void ns_get_time_ms(uint64_t *ms);

/**
 * @brief Non-cryptographic random byte (weak).
 *
 * Used for Bluetooth LTK placeholder generation during pairing. Replace with a hardware RNG or CSPRNG
 * if your product requires it.
 *
 * @return Uniform-ish byte in 0..255.
 */
uint8_t ns_get_random_u8(void);


/** @} */

/* --- Protocol-facing entry points --- */

/**
 * @brief Build one outgoing input report payload in-place.
 *
 * Writes report id to `data[0]` and payload to `data[1..]`.
 *
 * @param data Output buffer.
 * @param len Output buffer length in bytes (minimum 2).
 */
void ns_api_generate_inputreport(uint8_t *data, uint16_t len);


void ns_api_output_tunnel(uint8_t *data, uint16_t len);

/**
 * @brief One-shot initialize: apply device config, then initialize runtime state.
 *
 * This combines configuration and startup so callers do not need a separate
 * `ns_device_config_set()` step before initialization.
 *
 * @param cfg Device configuration to apply.
 * @return `NS_CONFIG_OK` on success; validation error otherwise.
 */
ns_config_status_t ns_lib_init(const ns_device_config_s *cfg);

#ifdef __cplusplus
}
#endif

#endif /* NS_LIB_API_H */

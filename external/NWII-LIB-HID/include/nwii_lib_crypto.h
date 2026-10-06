/**
 * @file nwii_lib_crypto.h
 * @brief Extension register encryption. Hosts may enable it by writing a 16-byte key to the
 *        extension's 0x40..0x4F registers and 0xAA to register 0xF0.
 *
 * NWII-LIB-HID is free and unencumbered software released into the public domain (The Unlicense).
 * See LICENSE in this folder. Written with the help of Claude Opus (Anthropic).
 *
 * SPDX-License-Identifier: Unlicense
 */

#ifndef NWII_LIB_CRYPTO_H
#define NWII_LIB_CRYPTO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t ft[8];
    uint8_t sb[8];
} nwii_crypto_state_s;

/**
 * @brief Derive the cipher tables from the host's key.
 *
 * @param state Cipher state to fill.
 * @param key The 16 bytes the host wrote to extension registers 0x40..0x4F.
 */
void nwii_crypto_generate(nwii_crypto_state_s *state, const uint8_t key[16]);

/**
 * @brief Encrypt bytes read from the extension register space, in place.
 *
 * @param state Cipher state from nwii_crypto_generate().
 * @param buf Bytes to encrypt.
 * @param addr Register address of buf[0] (the cipher is position dependent).
 * @param len Number of bytes.
 */
void nwii_crypto_encrypt(const nwii_crypto_state_s *state, uint8_t *buf, uint8_t addr, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* NWII_LIB_CRYPTO_H */

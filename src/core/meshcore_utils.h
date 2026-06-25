// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_CORE_UTILS_H_
#define MESHCORE_CORE_UTILS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal utility helpers for meshcore-C.
 */

void meshcore_utils_sha256(uint8_t *hash, size_t hash_len, const uint8_t *msg,
                           int msg_len);
void meshcore_utils_sha256_two_fragments(uint8_t *hash, size_t hash_len,
                                         const uint8_t *frag1, int frag1_len,
                                         const uint8_t *frag2, int frag2_len);

int meshcore_utils_encrypt(const uint8_t *shared_secret, uint8_t *dest,
                           const uint8_t *src, int src_len);
int meshcore_utils_decrypt(const uint8_t *shared_secret, uint8_t *dest,
                           const uint8_t *src, int src_len);
int meshcore_utils_encrypt_then_mac(const uint8_t *shared_secret, uint8_t *dest,
                                    const uint8_t *src, int src_len);
int meshcore_utils_mac_then_decrypt(const uint8_t *shared_secret, uint8_t *dest,
                                    const uint8_t *src, int src_len);

void meshcore_utils_to_hex(char *dest, const uint8_t *src, size_t len);
bool meshcore_utils_from_hex(uint8_t *dest, int dest_size,
                             const char *src_hex);
bool meshcore_utils_is_hex_char(char c);

int meshcore_utils_parse_text_parts(char *text, const char *parts[],
                                    int max_num, char separator);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_CORE_UTILS_H_ */

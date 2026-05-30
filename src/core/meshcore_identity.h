// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_CORE_IDENTITY_H_
#define MESHCORE_CORE_IDENTITY_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meshcore/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal identity model for meshcore-C.
 *
 * This module maps the non-Stream upstream Identity / LocalIdentity APIs into
 * plain C entry points.
 */

struct meshcore_identity {
  uint8_t pub_key[MESHCORE_PUBLIC_KEY_SIZE];
};

struct meshcore_local_identity {
  struct meshcore_identity identity;
  uint8_t prv_key[MESHCORE_PRIVATE_KEY_SIZE];
};

void meshcore_identity_init(struct meshcore_identity *identity);
bool meshcore_identity_init_from_pub_hex(struct meshcore_identity *identity,
                                         const char *pub_hex);
void meshcore_identity_init_from_pub_key(struct meshcore_identity *identity,
                                         const uint8_t *pub_key);

int meshcore_identity_copy_hash_to(const struct meshcore_identity *identity,
                                   uint8_t *dest);
int meshcore_identity_copy_hash_by_len(const struct meshcore_identity *identity,
                                       uint8_t *dest, uint8_t len);
bool meshcore_identity_is_hash_match(const struct meshcore_identity *identity,
                                     const uint8_t *hash);
bool meshcore_identity_is_hash_match_by_len(
    const struct meshcore_identity *identity, const uint8_t *hash,
    uint8_t len);
bool meshcore_identity_matches(const struct meshcore_identity *identity,
                               const struct meshcore_identity *other);
bool meshcore_identity_matches_by_pub_key(
    const struct meshcore_identity *identity, const uint8_t *other_pub_key);
bool meshcore_identity_verify(const struct meshcore_identity *identity,
                              const uint8_t *sig, const uint8_t *message,
                              int msg_len);

void meshcore_local_identity_init(struct meshcore_local_identity *local_identity);
bool meshcore_local_identity_init_from_hex(
    struct meshcore_local_identity *local_identity, const char *prv_hex,
    const char *pub_hex);
void meshcore_local_identity_init_from_bytes(
    struct meshcore_local_identity *local_identity, const uint8_t *prv_key_bytes,
    const uint8_t *pub_key_bytes);
void meshcore_local_identity_generate(
    struct meshcore_local_identity *local_identity);
void meshcore_local_identity_sign(
    const struct meshcore_local_identity *local_identity, uint8_t *sig,
    const uint8_t *message, int msg_len);
void meshcore_local_identity_calc_shared_secret(
    const struct meshcore_local_identity *local_identity, uint8_t *secret,
    const uint8_t *other_pub_key);
bool meshcore_local_identity_validate_private_key(
    const uint8_t prv_key[MESHCORE_PRIVATE_KEY_SIZE]);
size_t meshcore_local_identity_write_to(
    const struct meshcore_local_identity *local_identity, uint8_t *dest,
    size_t max_len);
void meshcore_local_identity_read_from(
    struct meshcore_local_identity *local_identity, const uint8_t *src,
    size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_CORE_IDENTITY_H_ */

// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_identity.h"

#include <string.h>

#include "crypto/monocypher-ed25519.h"
#include "crypto/monocypher.h"
#include "meshcore_rng.h"
#include "meshcore_utils.h"

#define MESHCORE_IDENTITY_HASH_DEFAULT_LEN MESHCORE_CHANNEL_HASH_BYTES
#define MESHCORE_IDENTITY_SEED_SIZE 32U
#define MESHCORE_IDENTITY_SIGNATURE_SIZE 64U

static void meshcore_identity_trimmed_scalar_to_pub(uint8_t *pub_key,
						    const uint8_t *trimmed_scalar)
{
	crypto_eddsa_scalarbase(pub_key, trimmed_scalar);
}

static void meshcore_identity_hash_reduce4(uint8_t out[32], const uint8_t *a,
					   size_t a_len, const uint8_t *b,
					   size_t b_len, const uint8_t *c,
					   size_t c_len, const uint8_t *d,
					   size_t d_len)
{
	uint8_t hash[64];
	crypto_sha512_ctx ctx;

	crypto_sha512_init(&ctx);
	if (a != NULL && a_len > 0U) {
		crypto_sha512_update(&ctx, a, a_len);
	}
	if (b != NULL && b_len > 0U) {
		crypto_sha512_update(&ctx, b, b_len);
	}
	if (c != NULL && c_len > 0U) {
		crypto_sha512_update(&ctx, c, c_len);
	}
	if (d != NULL && d_len > 0U) {
		crypto_sha512_update(&ctx, d, d_len);
	}
	crypto_sha512_final(&ctx, hash);
	crypto_eddsa_reduce(out, hash);
	crypto_wipe(hash, sizeof(hash));
}

static void meshcore_identity_sign_expanded_private_key(
	uint8_t signature[MESHCORE_IDENTITY_SIGNATURE_SIZE],
	const uint8_t public_key[MESHCORE_PUBLIC_KEY_SIZE],
	const uint8_t private_key[MESHCORE_PRIVATE_KEY_SIZE],
	const uint8_t *message, size_t message_size)
{
	uint8_t r[32];
	uint8_t h[32];
	uint8_t r_point[32];

	meshcore_identity_hash_reduce4(r, private_key + 32U, 32U, message,
				       message_size, NULL, 0U, NULL, 0U);
	crypto_eddsa_scalarbase(r_point, r);
	meshcore_identity_hash_reduce4(h, r_point, sizeof(r_point), public_key,
				       MESHCORE_PUBLIC_KEY_SIZE, message,
				       message_size, NULL, 0U);

	memcpy(signature, r_point, sizeof(r_point));
	crypto_eddsa_mul_add(signature + 32U, h, private_key, r);

	crypto_wipe(r, sizeof(r));
	crypto_wipe(h, sizeof(h));
	crypto_wipe(r_point, sizeof(r_point));
}

static void meshcore_identity_calculate_shared_secret(uint8_t *secret,
						      const uint8_t *prv_key,
						      const uint8_t *other_pub_key)
{
	uint8_t x25519_pub[32];

	crypto_eddsa_to_x25519(x25519_pub, other_pub_key);
	crypto_x25519(secret, prv_key, x25519_pub);
	crypto_wipe(x25519_pub, sizeof(x25519_pub));
}

void meshcore_identity_init(struct meshcore_identity *identity)
{
	if (identity == NULL) {
		return;
	}

	memset(identity->pub_key, 0, sizeof(identity->pub_key));
}

bool meshcore_identity_init_from_pub_hex(struct meshcore_identity *identity,
					 const char *pub_hex)
{
	if (identity == NULL || pub_hex == NULL) {
		return false;
	}

	return meshcore_utils_from_hex(identity->pub_key, MESHCORE_PUBLIC_KEY_SIZE,
				       pub_hex);
}

void meshcore_identity_init_from_pub_key(struct meshcore_identity *identity,
					 const uint8_t *pub_key)
{
	if (identity == NULL || pub_key == NULL) {
		return;
	}

	memcpy(identity->pub_key, pub_key, MESHCORE_PUBLIC_KEY_SIZE);
}

int meshcore_identity_copy_hash_to(const struct meshcore_identity *identity,
				   uint8_t *dest)
{
	return meshcore_identity_copy_hash_by_len(
		identity, dest, MESHCORE_IDENTITY_HASH_DEFAULT_LEN);
}

int meshcore_identity_copy_hash_by_len(const struct meshcore_identity *identity,
				       uint8_t *dest, uint8_t len)
{
	if (identity == NULL || dest == NULL) {
		return 0;
	}

	memcpy(dest, identity->pub_key, len);
	return len;
}

bool meshcore_identity_is_hash_match(const struct meshcore_identity *identity,
				     const uint8_t *hash)
{
	return meshcore_identity_is_hash_match_by_len(
		identity, hash, MESHCORE_IDENTITY_HASH_DEFAULT_LEN);
}

bool meshcore_identity_is_hash_match_by_len(
	const struct meshcore_identity *identity, const uint8_t *hash, uint8_t len)
{
	if (identity == NULL || hash == NULL) {
		return false;
	}

	return memcmp(hash, identity->pub_key, len) == 0;
}

bool meshcore_identity_matches(const struct meshcore_identity *identity,
			       const struct meshcore_identity *other)
{
	if (identity == NULL || other == NULL) {
		return false;
	}

	return meshcore_identity_matches_by_pub_key(identity, other->pub_key);
}

bool meshcore_identity_matches_by_pub_key(
	const struct meshcore_identity *identity, const uint8_t *other_pub_key)
{
	if (identity == NULL || other_pub_key == NULL) {
		return false;
	}

	return memcmp(identity->pub_key, other_pub_key,
		      MESHCORE_PUBLIC_KEY_SIZE) == 0;
}

bool meshcore_identity_verify(const struct meshcore_identity *identity,
			      const uint8_t *sig, const uint8_t *message,
			      int msg_len)
{
	if (identity == NULL || sig == NULL || message == NULL || msg_len < 0) {
		return false;
	}

	return crypto_ed25519_check(sig, identity->pub_key, message,
				    (size_t)msg_len) == 0;
}

void meshcore_local_identity_init(struct meshcore_local_identity *local_identity)
{
	if (local_identity == NULL) {
		return;
	}

	meshcore_identity_init(&local_identity->identity);
	memset(local_identity->prv_key, 0, sizeof(local_identity->prv_key));
}

bool meshcore_local_identity_init_from_hex(
	struct meshcore_local_identity *local_identity, const char *prv_hex,
	const char *pub_hex)
{
	bool ok;

	if (local_identity == NULL || prv_hex == NULL || pub_hex == NULL) {
		return false;
	}

	ok = meshcore_utils_from_hex(local_identity->prv_key,
				     MESHCORE_PRIVATE_KEY_SIZE, prv_hex);
	ok = ok && meshcore_utils_from_hex(local_identity->identity.pub_key,
					   MESHCORE_PUBLIC_KEY_SIZE, pub_hex);
	return ok;
}

void meshcore_local_identity_init_from_bytes(
	struct meshcore_local_identity *local_identity,
	const uint8_t *prv_key_bytes, const uint8_t *pub_key_bytes)
{
	if (local_identity == NULL || prv_key_bytes == NULL || pub_key_bytes == NULL) {
		return;
	}

	memcpy(local_identity->prv_key, prv_key_bytes, MESHCORE_PRIVATE_KEY_SIZE);
	memcpy(local_identity->identity.pub_key, pub_key_bytes,
	       MESHCORE_PUBLIC_KEY_SIZE);
}

void meshcore_local_identity_generate(
	struct meshcore_local_identity *local_identity)
{
	uint8_t seed[MESHCORE_IDENTITY_SEED_SIZE];

	if (local_identity == NULL) {
		return;
	}

	meshcore_rng_random(seed, sizeof(seed));
	crypto_sha512(local_identity->prv_key, seed, sizeof(seed));
	crypto_eddsa_trim_scalar(local_identity->prv_key, local_identity->prv_key);
	meshcore_identity_trimmed_scalar_to_pub(local_identity->identity.pub_key,
						local_identity->prv_key);
	crypto_wipe(seed, sizeof(seed));
}

void meshcore_local_identity_sign(
	const struct meshcore_local_identity *local_identity, uint8_t *sig,
	const uint8_t *message, int msg_len)
{
	if (local_identity == NULL || sig == NULL || message == NULL || msg_len < 0) {
		return;
	}

	meshcore_identity_sign_expanded_private_key(
		sig, local_identity->identity.pub_key, local_identity->prv_key,
		message, (size_t)msg_len);
}

void meshcore_local_identity_calc_shared_secret(
	const struct meshcore_local_identity *local_identity, uint8_t *secret,
	const uint8_t *other_pub_key)
{
	if (local_identity == NULL || secret == NULL || other_pub_key == NULL) {
		return;
	}

	meshcore_identity_calculate_shared_secret(secret, local_identity->prv_key,
						  other_pub_key);
}

bool meshcore_local_identity_validate_private_key(
	const uint8_t prv_key[MESHCORE_PRIVATE_KEY_SIZE])
{
	static const uint8_t test_client_prv[MESHCORE_PRIVATE_KEY_SIZE] = {
		0x70, 0x65, 0xe1, 0x8f, 0xd9, 0xfa, 0xbb, 0x70,
		0xc1, 0xed, 0x90, 0xdc, 0xa1, 0x99, 0x07, 0xde,
		0x69, 0x8c, 0x88, 0xb7, 0x09, 0xea, 0x14, 0x6e,
		0xaf, 0xd9, 0x3d, 0x9b, 0x83, 0x0c, 0x7b, 0x60,
		0xc4, 0x68, 0x11, 0x93, 0xc7, 0x9b, 0xbc, 0x39,
		0x94, 0x5b, 0xa8, 0x06, 0x41, 0x04, 0xbb, 0x61,
		0x8f, 0x8f, 0xd7, 0xa8, 0x4a, 0x0a, 0xf6, 0xf5,
		0x70, 0x33, 0xd6, 0xe8, 0xdd, 0xcd, 0x64, 0x71
	};
	static const uint8_t test_client_pub[MESHCORE_PUBLIC_KEY_SIZE] = {
		0x1e, 0xc7, 0x71, 0x75, 0xb0, 0x91, 0x8e, 0xd2,
		0x06, 0xf9, 0xae, 0x04, 0xec, 0x13, 0x6d, 0x6d,
		0x5d, 0x43, 0x15, 0xbb, 0x26, 0x30, 0x54, 0x27,
		0xf6, 0x45, 0xb4, 0x92, 0xe9, 0x35, 0x0c, 0x10
	};
	uint8_t pub[MESHCORE_PUBLIC_KEY_SIZE];
	uint8_t ss1[MESHCORE_PUBLIC_KEY_SIZE];
	uint8_t ss2[MESHCORE_PUBLIC_KEY_SIZE];
	bool ok = false;

	if (prv_key == NULL) {
		return false;
	}

	meshcore_identity_trimmed_scalar_to_pub(pub, prv_key);
	if (pub[0] == 0x00U || pub[0] == 0xFFU) {
		goto out;
	}

	meshcore_identity_calculate_shared_secret(ss1, prv_key, test_client_pub);
	meshcore_identity_calculate_shared_secret(ss2, test_client_prv, pub);
	if (memcmp(ss1, ss2, sizeof(ss1)) != 0) {
		goto out;
	}

	for (size_t i = 0U; i < sizeof(ss1); i++) {
		if (ss1[i] != 0U) {
			ok = true;
			break;
		}
	}

out:
	crypto_wipe(pub, sizeof(pub));
	crypto_wipe(ss1, sizeof(ss1));
	crypto_wipe(ss2, sizeof(ss2));
	return ok;
}

size_t meshcore_local_identity_write_to(
	const struct meshcore_local_identity *local_identity, uint8_t *dest,
	size_t max_len)
{
	if (local_identity == NULL || dest == NULL) {
		return 0U;
	}

	if (max_len < MESHCORE_PRIVATE_KEY_SIZE) {
		return 0U;
	}

	memcpy(dest, local_identity->prv_key, MESHCORE_PRIVATE_KEY_SIZE);
	if (max_len < (MESHCORE_PRIVATE_KEY_SIZE + MESHCORE_PUBLIC_KEY_SIZE)) {
		return MESHCORE_PRIVATE_KEY_SIZE;
	}

	memcpy(dest + MESHCORE_PRIVATE_KEY_SIZE, local_identity->identity.pub_key,
	       MESHCORE_PUBLIC_KEY_SIZE);
	return MESHCORE_PRIVATE_KEY_SIZE + MESHCORE_PUBLIC_KEY_SIZE;
}

void meshcore_local_identity_read_from(
	struct meshcore_local_identity *local_identity, const uint8_t *src,
	size_t len)
{
	if (local_identity == NULL || src == NULL) {
		return;
	}

	if (len == (MESHCORE_PRIVATE_KEY_SIZE + MESHCORE_PUBLIC_KEY_SIZE)) {
		memcpy(local_identity->prv_key, src, MESHCORE_PRIVATE_KEY_SIZE);
		memcpy(local_identity->identity.pub_key, src + MESHCORE_PRIVATE_KEY_SIZE,
		       MESHCORE_PUBLIC_KEY_SIZE);
	} else if (len == MESHCORE_PRIVATE_KEY_SIZE) {
		memcpy(local_identity->prv_key, src, MESHCORE_PRIVATE_KEY_SIZE);
		meshcore_identity_trimmed_scalar_to_pub(local_identity->identity.pub_key,
							local_identity->prv_key);
	}
}

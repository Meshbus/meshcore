// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_utils.h"

#include <errno.h>
#include <string.h>

#include "meshcore_platform_bridge.h"

#define MESHCORE_UTILS_PUB_KEY_SIZE 32U
#define MESHCORE_UTILS_CIPHER_KEY_SIZE 16U
#define MESHCORE_UTILS_CIPHER_BLOCK_SIZE 16U
#define MESHCORE_UTILS_CIPHER_MAC_SIZE 2U

void meshcore_utils_sha256(uint8_t *hash, size_t hash_len, const uint8_t *msg,
			   int msg_len)
{
	if (hash == NULL || hash_len == 0U || msg_len < 0 ||
	    (msg == NULL && msg_len > 0)) {
		if (hash != NULL && hash_len != 0U) {
			memset(hash, 0, hash_len);
		}
		return;
	}

	if (!meshcore_platform_bridge_crypto_sha256(hash, hash_len, msg, msg_len)) {
		memset(hash, 0, hash_len);
	}
}

void meshcore_utils_sha256_two_fragments(uint8_t *hash, size_t hash_len,
					 const uint8_t *frag1, int frag1_len,
					 const uint8_t *frag2, int frag2_len)
{
	if (hash == NULL || hash_len == 0U || frag1_len < 0 || frag2_len < 0 ||
	    (frag1 == NULL && frag1_len > 0) ||
	    (frag2 == NULL && frag2_len > 0)) {
		if (hash != NULL && hash_len != 0U) {
			memset(hash, 0, hash_len);
		}
		return;
	}

	if (!meshcore_platform_bridge_crypto_sha256_two_fragments(
		    hash, hash_len, frag1, frag1_len, frag2, frag2_len)) {
		memset(hash, 0, hash_len);
	}
}

int meshcore_utils_decrypt(const uint8_t *shared_secret, uint8_t *dest,
			   const uint8_t *src, int src_len)
{
	uint8_t *dp = dest;
	const uint8_t *sp = src;

	if (shared_secret == NULL || dest == NULL || src == NULL || src_len <= 0 ||
	    ((uint32_t)src_len % MESHCORE_UTILS_CIPHER_BLOCK_SIZE) != 0U) {
		return 0;
	}

	while (sp - src < src_len) {
		if (!meshcore_platform_bridge_crypto_aes128_decrypt_block(shared_secret, dp, sp)) {
			return 0;
		}
		dp += MESHCORE_UTILS_CIPHER_BLOCK_SIZE;
		sp += MESHCORE_UTILS_CIPHER_BLOCK_SIZE;
	}

	return (int)(sp - src);
}

int meshcore_utils_encrypt(const uint8_t *shared_secret, uint8_t *dest,
			   const uint8_t *src, int src_len)
{
	uint8_t *dp = dest;

	if (shared_secret == NULL || dest == NULL || src == NULL || src_len < 0) {
		return -EINVAL;
	}

	while (src_len >= (int)MESHCORE_UTILS_CIPHER_BLOCK_SIZE) {
		if (!meshcore_platform_bridge_crypto_aes128_encrypt_block(shared_secret, dp, src)) {
			return -EIO;
		}
		dp += MESHCORE_UTILS_CIPHER_BLOCK_SIZE;
		src += MESHCORE_UTILS_CIPHER_BLOCK_SIZE;
		src_len -= MESHCORE_UTILS_CIPHER_BLOCK_SIZE;
	}

	if (src_len > 0) {
		uint8_t block[MESHCORE_UTILS_CIPHER_BLOCK_SIZE];

		memset(block, 0, sizeof(block));
		memcpy(block, src, (size_t)src_len);

		if (!meshcore_platform_bridge_crypto_aes128_encrypt_block(shared_secret, dp, block)) {
			memset(block, 0, sizeof(block));
			return -EIO;
		}
		memset(block, 0, sizeof(block));
		dp += MESHCORE_UTILS_CIPHER_BLOCK_SIZE;
	}

	return (int)(dp - dest);
}

int meshcore_utils_encrypt_then_mac(const uint8_t *shared_secret, uint8_t *dest,
				    const uint8_t *src, int src_len)
{
	int enc_len;

	if (shared_secret == NULL || dest == NULL || src == NULL || src_len < 0) {
		return -EINVAL;
	}

	enc_len = meshcore_utils_encrypt(shared_secret,
					 dest + MESHCORE_UTILS_CIPHER_MAC_SIZE, src,
					 src_len);
	if (enc_len < 0) {
		return enc_len;
	}
	if (src_len > 0 && enc_len == 0) {
		return -EIO;
	}

	if (!meshcore_platform_bridge_crypto_hmac_sha256(
		    dest, MESHCORE_UTILS_CIPHER_MAC_SIZE, shared_secret,
		    MESHCORE_UTILS_PUB_KEY_SIZE,
		    dest + MESHCORE_UTILS_CIPHER_MAC_SIZE, (size_t)enc_len)) {
		return -EIO;
	}

	return (int)MESHCORE_UTILS_CIPHER_MAC_SIZE + enc_len;
}

int meshcore_utils_mac_then_decrypt(const uint8_t *shared_secret, uint8_t *dest,
				    const uint8_t *src, int src_len)
{
	uint8_t hmac[MESHCORE_UTILS_CIPHER_MAC_SIZE];

	if (shared_secret == NULL || dest == NULL || src == NULL ||
	    src_len <= (int)MESHCORE_UTILS_CIPHER_MAC_SIZE) {
		return 0;
	}

	if (!meshcore_platform_bridge_crypto_hmac_sha256(
		    hmac, sizeof(hmac), shared_secret, MESHCORE_UTILS_PUB_KEY_SIZE,
		    src + MESHCORE_UTILS_CIPHER_MAC_SIZE,
		    (size_t)(src_len - MESHCORE_UTILS_CIPHER_MAC_SIZE))) {
		return 0;
	}

	if (memcmp(hmac, src, MESHCORE_UTILS_CIPHER_MAC_SIZE) != 0) {
		memset(hmac, 0, sizeof(hmac));
		return 0;
	}
	memset(hmac, 0, sizeof(hmac));

	return meshcore_utils_decrypt(shared_secret, dest,
				      src + MESHCORE_UTILS_CIPHER_MAC_SIZE,
				      src_len - MESHCORE_UTILS_CIPHER_MAC_SIZE);
}

static const char hex_chars[] = "0123456789ABCDEF";

void meshcore_utils_to_hex(char *dest, const uint8_t *src, size_t len)
{
	while (len > 0U) {
		uint8_t b = *src++;

		*dest++ = hex_chars[b >> 4];
		*dest++ = hex_chars[b & 0x0F];
		len--;
	}

	*dest = '\0';
}

static uint8_t hex_val(char c)
{
	if (c >= 'A' && c <= 'F') {
		return (uint8_t)(c - 'A' + 10);
	}
	if (c >= 'a' && c <= 'f') {
		return (uint8_t)(c - 'a' + 10);
	}
	if (c >= '0' && c <= '9') {
		return (uint8_t)(c - '0');
	}
	return 0U;
}

bool meshcore_utils_is_hex_char(char c)
{
	return c == '0' || hex_val(c) > 0U;
}

bool meshcore_utils_from_hex(uint8_t *dest, int dest_size, const char *src_hex)
{
	size_t len;
	uint8_t *dp = dest;

	if (dest == NULL || src_hex == NULL || dest_size < 0) {
		return false;
	}

	len = strlen(src_hex);
	if (len != (size_t)dest_size * 2U) {
		return false;
	}

	while (dp - dest < dest_size) {
		char ch = *src_hex++;
		char cl = *src_hex++;

		if (!meshcore_utils_is_hex_char(ch) ||
		    !meshcore_utils_is_hex_char(cl)) {
			return false;
		}
		*dp++ = (uint8_t)((hex_val(ch) << 4) | hex_val(cl));
	}

	return true;
}

int meshcore_utils_parse_text_parts(char *text, const char *parts[], int max_num,
				    char separator)
{
	int num = 0;
	char *sp = text;

	while (*sp != '\0' && num < max_num) {
		parts[num++] = sp;
		while (*sp != '\0' && *sp != separator) {
			sp++;
		}
		if (*sp != '\0') {
			*sp++ = '\0';
		}
	}

	while (*sp != '\0' && *sp != separator) {
		sp++;
	}
	if (*sp != '\0') {
		*sp = '\0';
	}

	return num;
}

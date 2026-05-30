// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_packet.h"

#include <string.h>

#include "meshcore_utils.h"

static uint8_t meshcore_packet_path_len_field(
	const struct meshcore_packet *packet)
{
	if (packet == NULL) {
		return 0U;
	}

	return (uint8_t)packet->path_len;
}

void meshcore_packet_init(struct meshcore_packet *packet)
{
	if (packet == NULL) {
		return;
	}

	memset(packet, 0, sizeof(*packet));
}

uint8_t meshcore_packet_get_route_type(const struct meshcore_packet *packet)
{
	if (packet == NULL) {
		return 0U;
	}

	return packet->header & PH_ROUTE_MASK;
}

bool meshcore_packet_is_route_flood(const struct meshcore_packet *packet)
{
	uint8_t route_type = meshcore_packet_get_route_type(packet);

	return route_type == ROUTE_TYPE_FLOOD ||
	       route_type == ROUTE_TYPE_TRANSPORT_FLOOD;
}

bool meshcore_packet_is_route_direct(const struct meshcore_packet *packet)
{
	uint8_t route_type = meshcore_packet_get_route_type(packet);

	return route_type == ROUTE_TYPE_DIRECT ||
	       route_type == ROUTE_TYPE_TRANSPORT_DIRECT;
}

bool meshcore_packet_has_transport_codes(const struct meshcore_packet *packet)
{
	uint8_t route_type = meshcore_packet_get_route_type(packet);

	return route_type == ROUTE_TYPE_TRANSPORT_FLOOD ||
	       route_type == ROUTE_TYPE_TRANSPORT_DIRECT;
}

bool meshcore_packet_has_snr_flag(const struct meshcore_packet *packet)
{
	if (packet == NULL || !meshcore_packet_has_transport_codes(packet)) {
		return false;
	}

	return packet->transport_codes[0] == MESHCORE_SNR_TRANSPORT_CODE0 &&
	       packet->transport_codes[1] == MESHCORE_SNR_TRANSPORT_CODE1;
}

uint8_t meshcore_packet_get_payload_type(const struct meshcore_packet *packet)
{
	if (packet == NULL) {
		return 0U;
	}

	return (packet->header >> PH_TYPE_SHIFT) & PH_TYPE_MASK;
}

uint8_t meshcore_packet_get_payload_ver(const struct meshcore_packet *packet)
{
	if (packet == NULL) {
		return 0U;
	}

	return (packet->header >> PH_VER_SHIFT) & PH_VER_MASK;
}

uint8_t meshcore_packet_get_path_hash_size(const struct meshcore_packet *packet)
{
	uint8_t path_len = meshcore_packet_path_len_field(packet);

	return (uint8_t)((path_len >> 6) + 1U);
}

uint8_t meshcore_packet_get_path_hash_count(
	const struct meshcore_packet *packet)
{
	uint8_t path_len = meshcore_packet_path_len_field(packet);

	return path_len & 63U;
}

uint8_t meshcore_packet_get_path_byte_len(const struct meshcore_packet *packet)
{
	return (uint8_t)(meshcore_packet_get_path_hash_count(packet) *
			 meshcore_packet_get_path_hash_size(packet));
}

void meshcore_packet_set_path_hash_count(struct meshcore_packet *packet,
					 uint8_t hash_count)
{
	if (packet == NULL) {
		return;
	}

	packet->path_len &= (uint16_t)~63U;
	packet->path_len |= (uint16_t)(hash_count & 63U);
}

void meshcore_packet_set_path_hash_size_and_count(
	struct meshcore_packet *packet, uint8_t hash_size, uint8_t hash_count)
{
	if (packet == NULL) {
		return;
	}

	packet->path_len = ((uint16_t)(hash_size - 1U) << 6) |
			   (uint16_t)(hash_count & 63U);
}

bool meshcore_packet_is_valid_path_len(uint8_t path_len)
{
	uint8_t hash_count = path_len & 63U;
	uint8_t hash_size = (uint8_t)((path_len >> 6) + 1U);

	if (hash_size == 4U) {
		return false;
	}

	return (uint16_t)hash_count * (uint16_t)hash_size <= MESHCORE_MAX_PATH_LEN;
}

size_t meshcore_packet_write_path(uint8_t *dest, const uint8_t *src,
				  uint8_t path_len)
{
	uint8_t hash_count = path_len & 63U;
	uint8_t hash_size = (uint8_t)((path_len >> 6) + 1U);
	size_t len = (size_t)hash_count * (size_t)hash_size;

	if (dest == NULL || src == NULL) {
		return 0U;
	}
	if (len > MESHCORE_MAX_PATH_LEN) {
		return 0U;
	}

	memcpy(dest, src, len);
	return len;
}

uint8_t meshcore_packet_copy_path(uint8_t *dest, const uint8_t *src,
				  uint8_t path_len)
{
	(void)meshcore_packet_write_path(dest, src, path_len);
	return path_len;
}

void meshcore_packet_mark_do_not_retransmit(struct meshcore_packet *packet)
{
	if (packet == NULL) {
		return;
	}

	packet->header = 0xFFU;
}

bool meshcore_packet_is_marked_do_not_retransmit(
	const struct meshcore_packet *packet)
{
	return packet != NULL && packet->header == 0xFFU;
}

float meshcore_packet_get_snr(const struct meshcore_packet *packet)
{
	if (packet == NULL) {
		return 0.0f;
	}

	return ((float)packet->snr_q4) / 4.0f;
}

int meshcore_packet_get_raw_length(const struct meshcore_packet *packet)
{
	if (packet == NULL) {
		return 0;
	}

	return 2 + meshcore_packet_get_path_byte_len(packet) + packet->payload_len +
	       (meshcore_packet_has_transport_codes(packet) ? 4 : 0);
}

void meshcore_packet_calculate_hash(const struct meshcore_packet *packet,
				    uint8_t *hash)
{
	uint8_t data[1U + sizeof(uint16_t) + MESHCORE_PACKET_PAYLOAD_MAX_LEN];
	uint8_t payload_type;
	size_t pos = 0U;
	size_t extra;
	size_t data_len;

	if (hash == NULL) {
		return;
	}
	if (packet == NULL || packet->payload_len > MESHCORE_PACKET_PAYLOAD_MAX_LEN) {
		memset(hash, 0, MESHCORE_PACKET_HASH_SIZE);
		return;
	}

	payload_type = meshcore_packet_get_payload_type(packet);
	extra = (payload_type == PAYLOAD_TYPE_TRACE) ? sizeof(packet->path_len) : 0U;
	data_len = 1U + extra + packet->payload_len;
	if (data_len > sizeof(data)) {
		memset(hash, 0, MESHCORE_PACKET_HASH_SIZE);
		return;
	}

	data[pos++] = payload_type;
	if (extra != 0U) {
		/* TRACE packets include path_len to distinguish return-path revisits. */
		memcpy(&data[pos], &packet->path_len, sizeof(packet->path_len));
		pos += sizeof(packet->path_len);
	}
	if (packet->payload_len > 0U) {
		memcpy(&data[pos], packet->payload, packet->payload_len);
	}

	meshcore_utils_sha256(hash, MESHCORE_PACKET_HASH_SIZE, data, (int)data_len);
}

uint8_t meshcore_packet_write_to(const struct meshcore_packet *packet,
				 uint8_t dest[])
{
	uint8_t i = 0U;
	uint8_t path_len_field;
	size_t path_len_bytes;

	if (packet == NULL || dest == NULL) {
		return 0U;
	}
	path_len_field = meshcore_packet_path_len_field(packet);
	if (!meshcore_packet_is_valid_path_len(path_len_field) ||
	    packet->payload_len > MESHCORE_PACKET_PAYLOAD_MAX_LEN) {
		return 0U;
	}

	dest[i++] = packet->header;
	if (meshcore_packet_has_transport_codes(packet)) {
		memcpy(&dest[i], &packet->transport_codes[0], 2U);
		i += 2U;
		memcpy(&dest[i], &packet->transport_codes[1], 2U);
		i += 2U;
	}
	dest[i++] = path_len_field;
	path_len_bytes = meshcore_packet_write_path(&dest[i], packet->path,
						    path_len_field);
	i += (uint8_t)path_len_bytes;
	memcpy(&dest[i], packet->payload, packet->payload_len);
	i += (uint8_t)packet->payload_len;
	return i;
}

bool meshcore_packet_read_from(struct meshcore_packet *packet,
			       const uint8_t src[], uint8_t len)
{
	uint8_t i = 0U;
	uint8_t path_byte_len;

	if (packet == NULL || src == NULL || len == 0U) {
		return false;
	}

	packet->header = src[i++];
	if (meshcore_packet_has_transport_codes(packet)) {
		if ((size_t)i + sizeof(packet->transport_codes) > len) {
			return false;
		}
		memcpy(&packet->transport_codes[0], &src[i], 2U);
		i += 2U;
		memcpy(&packet->transport_codes[1], &src[i], 2U);
		i += 2U;
	} else {
		packet->transport_codes[0] = 0U;
		packet->transport_codes[1] = 0U;
	}

	if (i >= len) {
		return false;
	}

	packet->path_len = src[i++];
	if (!meshcore_packet_is_valid_path_len(meshcore_packet_path_len_field(packet))) {
		return false;
	}

	path_byte_len = meshcore_packet_get_path_byte_len(packet);
	if ((uint16_t)i + path_byte_len > len) {
		return false;
	}
	memcpy(packet->path, &src[i], path_byte_len);
	i += path_byte_len;

	if (i >= len) {
		return false;
	}

	packet->payload_len = (uint16_t)(len - i);
	if (packet->payload_len > sizeof(packet->payload)) {
		return false;
	}
	memcpy(packet->payload, &src[i], packet->payload_len);
	return true;
}

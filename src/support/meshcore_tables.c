// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_tables.h"

#include <string.h>

static void meshcore_tables_count_dup(
	struct meshcore_tables *tables,
	const struct meshcore_packet *packet)
{
	if (meshcore_packet_is_route_direct(packet)) {
		tables->direct_dups++;
	} else {
		tables->flood_dups++;
	}
}

static bool meshcore_tables_has_seen_hash(
	struct meshcore_tables *tables,
	const struct meshcore_packet *packet)
{
	uint8_t hash[MESHCORE_PACKET_HASH_SIZE];
	const uint8_t *sp = tables->hashes;
	int i;

	meshcore_packet_calculate_hash(packet, hash);
	for (i = 0; i < (int)MESHCORE_TABLES_MAX_PACKET_HASHES;
	     i++, sp += MESHCORE_PACKET_HASH_SIZE) {
		if (memcmp(hash, sp, MESHCORE_PACKET_HASH_SIZE) == 0) {
			meshcore_tables_count_dup(tables, packet);
			return true;
		}
	}

	memcpy(&tables->hashes[tables->next_idx * MESHCORE_PACKET_HASH_SIZE], hash,
	       MESHCORE_PACKET_HASH_SIZE);
	tables->next_idx =
		(tables->next_idx + 1) % (int)MESHCORE_TABLES_MAX_PACKET_HASHES;
	return false;
}

void meshcore_tables_init(struct meshcore_tables *tables)
{
	if (tables == NULL) {
		return;
	}

	memset(tables, 0, sizeof(*tables));
}

bool meshcore_tables_has_seen(struct meshcore_tables *tables,
			      const struct meshcore_packet *packet)
{
	if (tables == NULL || packet == NULL) {
		return false;
	}

	return meshcore_tables_has_seen_hash(tables, packet);
}

void meshcore_tables_clear(struct meshcore_tables *tables,
			   const struct meshcore_packet *packet)
{
	if (tables == NULL || packet == NULL) {
		return;
	}

	{
		uint8_t hash[MESHCORE_PACKET_HASH_SIZE];
		uint8_t *sp = tables->hashes;
		int i;

		meshcore_packet_calculate_hash(packet, hash);
		for (i = 0; i < (int)MESHCORE_TABLES_MAX_PACKET_HASHES;
		     i++, sp += MESHCORE_PACKET_HASH_SIZE) {
			if (memcmp(hash, sp, MESHCORE_PACKET_HASH_SIZE) == 0) {
				memset(sp, 0, MESHCORE_PACKET_HASH_SIZE);
				break;
			}
		}
	}
}

uint32_t meshcore_tables_get_num_direct_dups(
	const struct meshcore_tables *tables)
{
	if (tables == NULL) {
		return 0U;
	}

	return tables->direct_dups;
}

uint32_t meshcore_tables_get_num_flood_dups(
	const struct meshcore_tables *tables)
{
	if (tables == NULL) {
		return 0U;
	}

	return tables->flood_dups;
}

void meshcore_tables_reset_stats(struct meshcore_tables *tables)
{
	if (tables == NULL) {
		return;
	}

	tables->direct_dups = 0U;
	tables->flood_dups = 0U;
}

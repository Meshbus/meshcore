// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_SUPPORT_TABLES_H_
#define MESHCORE_SUPPORT_TABLES_H_

#include <stdbool.h>
#include <stdint.h>

#include "meshcore_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal duplicate-filter / ACK tables for meshcore-C runtime.
 */

#define MESHCORE_TABLES_MAX_PACKET_HASHES 128U
#define MESHCORE_TABLES_MAX_PACKET_ACKS 64U

struct meshcore_tables {
  uint8_t hashes[MESHCORE_TABLES_MAX_PACKET_HASHES *
                 MESHCORE_PACKET_HASH_SIZE];
  int next_idx;
  uint32_t acks[MESHCORE_TABLES_MAX_PACKET_ACKS];
  int next_ack_idx;
  uint32_t direct_dups;
  uint32_t flood_dups;
};

void meshcore_tables_init(struct meshcore_tables *tables);
bool meshcore_tables_has_seen(struct meshcore_tables *tables,
                              const struct meshcore_packet *packet);
void meshcore_tables_clear(struct meshcore_tables *tables,
                           const struct meshcore_packet *packet);
uint32_t meshcore_tables_get_num_direct_dups(
    const struct meshcore_tables *tables);
uint32_t meshcore_tables_get_num_flood_dups(
    const struct meshcore_tables *tables);
void meshcore_tables_reset_stats(struct meshcore_tables *tables);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_SUPPORT_TABLES_H_ */

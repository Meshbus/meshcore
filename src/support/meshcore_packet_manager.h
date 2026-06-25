// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_SUPPORT_PACKET_MANAGER_H_
#define MESHCORE_SUPPORT_PACKET_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>

#include "meshcore_packet.h"

#ifndef MESHCORE_PACKET_QUEUE_MANAGER_MAX_POOL_SIZE
#ifdef MESHCORE_RUNTIME_PACKET_POOL_SIZE
#define MESHCORE_PACKET_QUEUE_MANAGER_MAX_POOL_SIZE MESHCORE_RUNTIME_PACKET_POOL_SIZE
#else
#define MESHCORE_PACKET_QUEUE_MANAGER_MAX_POOL_SIZE 10U
#endif
#endif

#define MESHCORE_PACKET_QUEUE_MANAGER_STORAGE_SIZE                            \
  ((MESHCORE_PACKET_QUEUE_MANAGER_MAX_POOL_SIZE) > 0U                         \
       ? (MESHCORE_PACKET_QUEUE_MANAGER_MAX_POOL_SIZE)                        \
       : 1U)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal packet storage and queueing helpers for meshcore-C runtime.
 *
 * Storage is a fixed arena owned by each queue manager. Hosts that need a
 * larger runtime pool can override MESHCORE_RUNTIME_PACKET_POOL_SIZE or
 * MESHCORE_PACKET_QUEUE_MANAGER_MAX_POOL_SIZE at compile time.
 */

struct meshcore_packet_queue {
  struct meshcore_packet **table;
  uint8_t *pri_table;
  uint32_t *schedule_table;
  int size;
  int num;
  struct meshcore_packet *table_storage[MESHCORE_PACKET_QUEUE_MANAGER_STORAGE_SIZE];
  uint8_t pri_table_storage[MESHCORE_PACKET_QUEUE_MANAGER_STORAGE_SIZE];
  uint32_t schedule_table_storage[MESHCORE_PACKET_QUEUE_MANAGER_STORAGE_SIZE];
};

struct meshcore_packet_queue_manager {
  struct meshcore_packet packet_buf[MESHCORE_PACKET_QUEUE_MANAGER_STORAGE_SIZE];
  int pool_size;
  bool initialized;
  struct meshcore_packet_queue unused;
  struct meshcore_packet_queue send_queue;
  struct meshcore_packet_queue rx_queue;
};

bool meshcore_packet_queue_init(struct meshcore_packet_queue *queue,
                                int max_entries);
void meshcore_packet_queue_deinit(struct meshcore_packet_queue *queue);
struct meshcore_packet *meshcore_packet_queue_get(
    struct meshcore_packet_queue *queue, uint32_t now);
bool meshcore_packet_queue_add(struct meshcore_packet_queue *queue,
                               struct meshcore_packet *packet,
                               uint8_t priority, uint32_t scheduled_for);
int meshcore_packet_queue_count(const struct meshcore_packet_queue *queue);
int meshcore_packet_queue_count_before(const struct meshcore_packet_queue *queue,
                                       uint32_t now);
bool meshcore_packet_queue_next_scheduled_get(
    const struct meshcore_packet_queue *queue, uint32_t now,
    uint32_t *scheduled_for);
struct meshcore_packet *meshcore_packet_queue_item_at(
    const struct meshcore_packet_queue *queue, int i);
struct meshcore_packet *meshcore_packet_queue_remove_by_idx(
    struct meshcore_packet_queue *queue, int i);

void meshcore_packet_queue_manager_prepare(
    struct meshcore_packet_queue_manager *manager, int pool_size);
void meshcore_packet_queue_manager_deinit(
    struct meshcore_packet_queue_manager *manager);

struct meshcore_packet *meshcore_packet_queue_manager_alloc_new(
    struct meshcore_packet_queue_manager *manager);
void meshcore_packet_queue_manager_free(
    struct meshcore_packet_queue_manager *manager,
    struct meshcore_packet *packet);
void meshcore_packet_queue_manager_queue_outbound(
    struct meshcore_packet_queue_manager *manager,
    struct meshcore_packet *packet, uint8_t priority, uint32_t scheduled_for);
struct meshcore_packet *meshcore_packet_queue_manager_get_next_outbound(
    struct meshcore_packet_queue_manager *manager, uint32_t now);
int meshcore_packet_queue_manager_get_outbound_count(
    struct meshcore_packet_queue_manager *manager, uint32_t now);
int meshcore_packet_queue_manager_get_outbound_total(
    struct meshcore_packet_queue_manager *manager);
bool meshcore_packet_queue_manager_next_outbound_scheduled_get(
    struct meshcore_packet_queue_manager *manager, uint32_t now,
    uint32_t *scheduled_for);
int meshcore_packet_queue_manager_get_free_count(
    struct meshcore_packet_queue_manager *manager);
struct meshcore_packet *meshcore_packet_queue_manager_get_outbound_by_idx(
    struct meshcore_packet_queue_manager *manager, int i);
struct meshcore_packet *meshcore_packet_queue_manager_remove_outbound_by_idx(
    struct meshcore_packet_queue_manager *manager, int i);
void meshcore_packet_queue_manager_queue_inbound(
    struct meshcore_packet_queue_manager *manager,
    struct meshcore_packet *packet, uint32_t scheduled_for);
struct meshcore_packet *meshcore_packet_queue_manager_get_next_inbound(
    struct meshcore_packet_queue_manager *manager, uint32_t now);
bool meshcore_packet_queue_manager_next_inbound_scheduled_get(
    struct meshcore_packet_queue_manager *manager, uint32_t now,
    uint32_t *scheduled_for);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_SUPPORT_PACKET_MANAGER_H_ */

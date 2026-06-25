// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_packet_manager.h"

#include <string.h>

static void meshcore_packet_queue_compact_from(struct meshcore_packet_queue *queue,
					       int idx)
{
	queue->num--;
	while (idx < queue->num) {
		queue->table[idx] = queue->table[idx + 1];
		queue->pri_table[idx] = queue->pri_table[idx + 1];
		queue->schedule_table[idx] = queue->schedule_table[idx + 1];
		idx++;
	}
}

bool meshcore_packet_queue_init(struct meshcore_packet_queue *queue,
				int max_entries)
{
	if (queue == NULL || max_entries < 0) {
		return false;
	}

	memset(queue, 0, sizeof(*queue));
	if (max_entries > (int)MESHCORE_PACKET_QUEUE_MANAGER_MAX_POOL_SIZE) {
		return false;
	}
	if (max_entries == 0) {
		return true;
	}

	queue->table = queue->table_storage;
	queue->pri_table = queue->pri_table_storage;
	queue->schedule_table = queue->schedule_table_storage;
	queue->size = max_entries;
	return true;
}

void meshcore_packet_queue_deinit(struct meshcore_packet_queue *queue)
{
	if (queue == NULL) {
		return;
	}

	memset(queue, 0, sizeof(*queue));
}

struct meshcore_packet *meshcore_packet_queue_get(
	struct meshcore_packet_queue *queue, uint32_t now)
{
	uint8_t min_pri = 0xFFU;
	int best_idx = -1;
	int j;
	struct meshcore_packet *top;

	if (queue == NULL) {
		return NULL;
	}

	for (j = 0; j < queue->num; j++) {
		if ((int32_t)(queue->schedule_table[j] - now) > 0) {
			continue;
		}
		if (queue->pri_table[j] < min_pri) {
			min_pri = queue->pri_table[j];
			best_idx = j;
		}
	}

	if (best_idx < 0) {
		return NULL;
	}

	top = queue->table[best_idx];
	meshcore_packet_queue_compact_from(queue, best_idx);
	return top;
}

bool meshcore_packet_queue_add(struct meshcore_packet_queue *queue,
			       struct meshcore_packet *packet,
			       uint8_t priority, uint32_t scheduled_for)
{
	if (queue == NULL || packet == NULL || queue->num >= queue->size) {
		return false;
	}

	queue->table[queue->num] = packet;
	queue->pri_table[queue->num] = priority;
	queue->schedule_table[queue->num] = scheduled_for;
	queue->num++;
	return true;
}

int meshcore_packet_queue_count(const struct meshcore_packet_queue *queue)
{
	if (queue == NULL) {
		return 0;
	}

	return queue->num;
}

int meshcore_packet_queue_count_before(const struct meshcore_packet_queue *queue,
				       uint32_t now)
{
	if (queue == NULL) {
		return 0;
	}

	if (now == UINT32_MAX) {
		return queue->num;
	}

	int n = 0;
	int j;
	for (j = 0; j < queue->num; j++) {
		if ((int32_t)(queue->schedule_table[j] - now) > 0) {
			continue;
		}
		n++;
	}
	return n;
}

bool meshcore_packet_queue_next_scheduled_get(
	const struct meshcore_packet_queue *queue, uint32_t now,
	uint32_t *scheduled_for)
{
	uint32_t best = 0U;
	bool have_best = false;
	int j;

	if (queue == NULL || scheduled_for == NULL) {
		return false;
	}

	for (j = 0; j < queue->num; j++) {
		uint32_t candidate = queue->schedule_table[j];

		if ((int32_t)(candidate - now) <= 0) {
			*scheduled_for = now;
			return true;
		}
		if (!have_best || (int32_t)(candidate - best) < 0) {
			best = candidate;
			have_best = true;
		}
	}

	if (!have_best) {
		return false;
	}

	*scheduled_for = best;
	return true;
}

struct meshcore_packet *meshcore_packet_queue_item_at(
	const struct meshcore_packet_queue *queue, int i)
{
	if (queue == NULL || i < 0 || i >= queue->num) {
		return NULL;
	}

	return queue->table[i];
}

struct meshcore_packet *meshcore_packet_queue_remove_by_idx(
	struct meshcore_packet_queue *queue, int i)
{
	struct meshcore_packet *item;

	if (queue == NULL || i < 0 || i >= queue->num) {
		return NULL;
	}

	item = queue->table[i];
	meshcore_packet_queue_compact_from(queue, i);
	return item;
}

void meshcore_packet_queue_manager_prepare(
	struct meshcore_packet_queue_manager *manager, int pool_size)
{
	int i;

	if (manager == NULL) {
		return;
	}

	meshcore_packet_queue_manager_deinit(manager);
	memset(manager, 0, sizeof(*manager));
	if (pool_size <= 0) {
		manager->pool_size = 0;
		return;
	}
	if (pool_size > (int)MESHCORE_PACKET_QUEUE_MANAGER_MAX_POOL_SIZE) {
		return;
	}

	manager->pool_size = pool_size;
	if (!meshcore_packet_queue_init(&manager->unused, pool_size) ||
	    !meshcore_packet_queue_init(&manager->send_queue, pool_size) ||
	    !meshcore_packet_queue_init(&manager->rx_queue, pool_size)) {
		meshcore_packet_queue_manager_deinit(manager);
		return;
	}

	for (i = 0; i < pool_size; i++) {
		meshcore_packet_init(&manager->packet_buf[i]);
		if (!meshcore_packet_queue_add(&manager->unused,
						&manager->packet_buf[i], 0U, 0U)) {
			meshcore_packet_queue_manager_deinit(manager);
			return;
		}
	}

	manager->initialized = true;
}

void meshcore_packet_queue_manager_deinit(
	struct meshcore_packet_queue_manager *manager)
{
	if (manager == NULL) {
		return;
	}

	meshcore_packet_queue_deinit(&manager->unused);
	meshcore_packet_queue_deinit(&manager->send_queue);
	meshcore_packet_queue_deinit(&manager->rx_queue);
	memset(manager, 0, sizeof(*manager));
}

struct meshcore_packet *meshcore_packet_queue_manager_alloc_new(
	struct meshcore_packet_queue_manager *manager)
{
	if (manager == NULL || !manager->initialized) {
		return NULL;
	}

	return meshcore_packet_queue_remove_by_idx(&manager->unused, 0);
}

void meshcore_packet_queue_manager_free(
	struct meshcore_packet_queue_manager *manager,
	struct meshcore_packet *packet)
{
	if (manager == NULL || !manager->initialized || packet == NULL) {
		return;
	}

	(void)meshcore_packet_queue_add(&manager->unused, packet, 0U, 0U);
}

void meshcore_packet_queue_manager_queue_outbound(
	struct meshcore_packet_queue_manager *manager,
	struct meshcore_packet *packet, uint8_t priority, uint32_t scheduled_for)
{
	if (manager == NULL || !manager->initialized || packet == NULL) {
		return;
	}

	if (!meshcore_packet_queue_add(&manager->send_queue, packet, priority,
				       scheduled_for)) {
		meshcore_packet_queue_manager_free(manager, packet);
	}
}

struct meshcore_packet *meshcore_packet_queue_manager_get_next_outbound(
	struct meshcore_packet_queue_manager *manager, uint32_t now)
{
	if (manager == NULL || !manager->initialized) {
		return NULL;
	}

	return meshcore_packet_queue_get(&manager->send_queue, now);
}

int meshcore_packet_queue_manager_get_outbound_count(
	struct meshcore_packet_queue_manager *manager, uint32_t now)
{
	if (manager == NULL || !manager->initialized) {
		return 0;
	}

	return meshcore_packet_queue_count_before(&manager->send_queue, now);
}

int meshcore_packet_queue_manager_get_outbound_total(
	struct meshcore_packet_queue_manager *manager)
{
	if (manager == NULL || !manager->initialized) {
		return 0;
	}

	return meshcore_packet_queue_count(&manager->send_queue);
}

bool meshcore_packet_queue_manager_next_outbound_scheduled_get(
	struct meshcore_packet_queue_manager *manager, uint32_t now,
	uint32_t *scheduled_for)
{
	if (manager == NULL || !manager->initialized) {
		return false;
	}

	return meshcore_packet_queue_next_scheduled_get(&manager->send_queue, now,
							scheduled_for);
}

int meshcore_packet_queue_manager_get_free_count(
	struct meshcore_packet_queue_manager *manager)
{
	if (manager == NULL) {
		return 0;
	}
	if (!manager->initialized) {
		return manager->pool_size;
	}

	return meshcore_packet_queue_count(&manager->unused);
}

struct meshcore_packet *meshcore_packet_queue_manager_get_outbound_by_idx(
	struct meshcore_packet_queue_manager *manager, int i)
{
	if (manager == NULL || !manager->initialized) {
		return NULL;
	}

	return meshcore_packet_queue_item_at(&manager->send_queue, i);
}

struct meshcore_packet *
meshcore_packet_queue_manager_remove_outbound_by_idx(
	struct meshcore_packet_queue_manager *manager, int i)
{
	if (manager == NULL || !manager->initialized) {
		return NULL;
	}

	return meshcore_packet_queue_remove_by_idx(&manager->send_queue, i);
}

void meshcore_packet_queue_manager_queue_inbound(
	struct meshcore_packet_queue_manager *manager,
	struct meshcore_packet *packet, uint32_t scheduled_for)
{
	if (manager == NULL || !manager->initialized || packet == NULL) {
		return;
	}

	if (!meshcore_packet_queue_add(&manager->rx_queue, packet, 0U,
				       scheduled_for)) {
		meshcore_packet_queue_manager_free(manager, packet);
	}
}

struct meshcore_packet *meshcore_packet_queue_manager_get_next_inbound(
	struct meshcore_packet_queue_manager *manager, uint32_t now)
{
	if (manager == NULL || !manager->initialized) {
		return NULL;
	}

	return meshcore_packet_queue_get(&manager->rx_queue, now);
}

bool meshcore_packet_queue_manager_next_inbound_scheduled_get(
	struct meshcore_packet_queue_manager *manager, uint32_t now,
	uint32_t *scheduled_for)
{
	if (manager == NULL || !manager->initialized) {
		return false;
	}

	return meshcore_packet_queue_next_scheduled_get(&manager->rx_queue, now,
							scheduled_for);
}

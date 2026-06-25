// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_runtime_internal.h"

#include <string.h>

#include "meshcore_platform_bridge.h"

static void meshcore_runtime_expected_ack_clear(
    struct meshcore_runtime_expected_ack *entry) {
  if (entry == NULL) {
    return;
  }

  memset(entry, 0, sizeof(*entry));
}

void meshcore_runtime_pending_discovery_clear(void) {
  memset(&meshcore_runtime_context_get()->pending_discovery, 0,
         sizeof(meshcore_runtime_context_get()->pending_discovery));
}

void meshcore_runtime_pending_trace_clear(void) {
  memset(&meshcore_runtime_context_get()->pending_trace, 0,
         sizeof(meshcore_runtime_context_get()->pending_trace));
}

void meshcore_runtime_pending_telemetry_clear(void) {
  memset(&meshcore_runtime_context_get()->pending_telemetry, 0,
         sizeof(meshcore_runtime_context_get()->pending_telemetry));
}

void meshcore_runtime_pending_binary_clear(void) {
  memset(&meshcore_runtime_context_get()->pending_binary, 0,
         sizeof(meshcore_runtime_context_get()->pending_binary));
}

static void meshcore_runtime_expected_ack_cleanup(unsigned long now_ms) {
  size_t i;

  for (i = 0U; i < MESHCORE_RUNTIME_EXPECTED_ACK_TABLE_SIZE; i++) {
    struct meshcore_runtime_expected_ack *entry =
        &meshcore_runtime_context_get()->expected_acks[i];

    if (!entry->valid) {
      continue;
    }
    if (meshcore_runtime_time_reached(now_ms, entry->expires_at_ms)) {
      meshcore_runtime_expected_ack_clear(entry);
    }
  }
}

void meshcore_runtime_correlation_cleanup(unsigned long now_ms) {
  meshcore_runtime_expected_ack_cleanup(now_ms);

  if (meshcore_runtime_context_get()->pending_discovery.valid &&
      meshcore_runtime_time_reached(
          now_ms, meshcore_runtime_context_get()->pending_discovery.expires_at_ms)) {
    meshcore_runtime_pending_discovery_clear();
  }
  if (meshcore_runtime_context_get()->pending_trace.valid &&
      meshcore_runtime_time_reached(now_ms,
                                    meshcore_runtime_context_get()->pending_trace.expires_at_ms)) {
    meshcore_runtime_pending_trace_clear();
  }
  if (meshcore_runtime_context_get()->pending_telemetry.valid &&
      meshcore_runtime_time_reached(
          now_ms, meshcore_runtime_context_get()->pending_telemetry.expires_at_ms)) {
    meshcore_runtime_pending_telemetry_clear();
  }
  if (meshcore_runtime_context_get()->pending_binary.valid &&
      meshcore_runtime_time_reached(
          now_ms, meshcore_runtime_context_get()->pending_binary.expires_at_ms)) {
    meshcore_runtime_pending_binary_clear();
  }
}

bool meshcore_runtime_correlation_next_deadline_get(
    uint32_t now_ms, uint32_t *deadline_ms) {
  bool has_deadline = false;
  uint32_t deadline = 0U;
  size_t i;

  if (deadline_ms == NULL) {
    return false;
  }

  for (i = 0U; i < MESHCORE_RUNTIME_EXPECTED_ACK_TABLE_SIZE; i++) {
    const struct meshcore_runtime_expected_ack *entry =
        &meshcore_runtime_context_get()->expected_acks[i];

    if (!entry->valid) {
      continue;
    }
    if (meshcore_runtime_deadline_accumulate(
            now_ms, (uint32_t)entry->expires_at_ms, &has_deadline,
            &deadline)) {
      *deadline_ms = deadline;
      return true;
    }
  }

  if (meshcore_runtime_context_get()->pending_discovery.valid) {
    if (meshcore_runtime_deadline_accumulate(
            now_ms, (uint32_t)meshcore_runtime_context_get()->pending_discovery.expires_at_ms,
            &has_deadline, &deadline)) {
      *deadline_ms = deadline;
      return true;
    }
  }
  if (meshcore_runtime_context_get()->pending_trace.valid) {
    if (meshcore_runtime_deadline_accumulate(
            now_ms, (uint32_t)meshcore_runtime_context_get()->pending_trace.expires_at_ms,
            &has_deadline, &deadline)) {
      *deadline_ms = deadline;
      return true;
    }
  }
  if (meshcore_runtime_context_get()->pending_telemetry.valid) {
    if (meshcore_runtime_deadline_accumulate(
            now_ms, (uint32_t)meshcore_runtime_context_get()->pending_telemetry.expires_at_ms,
            &has_deadline, &deadline)) {
      *deadline_ms = deadline;
      return true;
    }
  }
  if (meshcore_runtime_context_get()->pending_binary.valid) {
    if (meshcore_runtime_deadline_accumulate(
            now_ms, (uint32_t)meshcore_runtime_context_get()->pending_binary.expires_at_ms,
            &has_deadline, &deadline)) {
      *deadline_ms = deadline;
      return true;
    }
  }

  if (!has_deadline) {
    return false;
  }

  *deadline_ms = deadline;
  return true;
}

void meshcore_runtime_expected_ack_register(uint32_t ack_crc,
                                                   const uint8_t *target,
                                                   uint8_t attempt) {
  size_t slot_idx = meshcore_runtime_context_get()->expected_ack_next;
  unsigned long now_ms = meshcore_clock_millis_get();
  size_t i;

  if (ack_crc == 0U || target == NULL) {
    return;
  }

  for (i = 0U; i < MESHCORE_RUNTIME_EXPECTED_ACK_TABLE_SIZE; i++) {
    size_t idx =
        (meshcore_runtime_context_get()->expected_ack_next + i) %
        MESHCORE_RUNTIME_EXPECTED_ACK_TABLE_SIZE;
    struct meshcore_runtime_expected_ack *entry =
        &meshcore_runtime_context_get()->expected_acks[idx];

    if (!entry->valid ||
        meshcore_runtime_time_reached(now_ms, entry->expires_at_ms)) {
      slot_idx = idx;
      break;
    }
  }

  memset(&meshcore_runtime_context_get()->expected_acks[slot_idx], 0,
         sizeof(meshcore_runtime_context_get()->expected_acks[slot_idx]));
  meshcore_runtime_context_get()->expected_acks[slot_idx].valid = true;
  meshcore_runtime_context_get()->expected_acks[slot_idx].msg_sent_ms = now_ms;
  meshcore_runtime_context_get()->expected_acks[slot_idx].expires_at_ms =
      now_ms + MESHCORE_RUNTIME_REQUEST_TIMEOUT_MS;
  meshcore_runtime_context_get()->expected_acks[slot_idx].ack_crc = ack_crc;
  memcpy(meshcore_runtime_context_get()->expected_acks[slot_idx].target, target,
         sizeof(meshcore_runtime_context_get()->expected_acks[slot_idx].target));
  meshcore_runtime_context_get()->expected_acks[slot_idx].attempt = attempt;
  meshcore_runtime_context_get()->expected_ack_next =
      (slot_idx + 1U) % MESHCORE_RUNTIME_EXPECTED_ACK_TABLE_SIZE;
}

bool meshcore_runtime_expected_ack_handle(uint32_t ack_crc) {
  unsigned long now_ms = meshcore_clock_millis_get();
  size_t i;

  for (i = 0U; i < MESHCORE_RUNTIME_EXPECTED_ACK_TABLE_SIZE; i++) {
    struct meshcore_runtime_expected_ack *entry =
        &meshcore_runtime_context_get()->expected_acks[i];

    if (!entry->valid) {
      continue;
    }
    if (meshcore_runtime_time_reached(now_ms, entry->expires_at_ms)) {
      meshcore_runtime_expected_ack_clear(entry);
      continue;
    }
    if (entry->ack_crc != ack_crc) {
      continue;
    }

    (void)meshcore_platform_bridge_message_ack_handler(entry->target, entry->attempt);
    meshcore_runtime_expected_ack_clear(entry);
    return true;
  }

  return false;
}

void meshcore_runtime_pending_discovery_register(uint32_t tag,
                                                        const uint8_t *key_prefix) {
  if (key_prefix == NULL) {
    return;
  }

  meshcore_runtime_pending_discovery_clear();
  meshcore_runtime_context_get()->pending_discovery.valid = true;
  meshcore_runtime_context_get()->pending_discovery.tag = tag;
  meshcore_runtime_context_get()->pending_discovery.expires_at_ms =
      meshcore_clock_millis_get() + MESHCORE_RUNTIME_REQUEST_TIMEOUT_MS;
  memcpy(meshcore_runtime_context_get()->pending_discovery.key_prefix, key_prefix,
         sizeof(meshcore_runtime_context_get()->pending_discovery.key_prefix));
}

void meshcore_runtime_pending_trace_register(uint32_t tag,
                                                    const uint8_t *key_prefix) {
  if (key_prefix == NULL) {
    return;
  }

  meshcore_runtime_pending_trace_clear();
  meshcore_runtime_context_get()->pending_trace.valid = true;
  meshcore_runtime_context_get()->pending_trace.tag = tag;
  meshcore_runtime_context_get()->pending_trace.expires_at_ms =
      meshcore_clock_millis_get() + MESHCORE_RUNTIME_REQUEST_TIMEOUT_MS;
  memcpy(meshcore_runtime_context_get()->pending_trace.key_prefix, key_prefix,
         sizeof(meshcore_runtime_context_get()->pending_trace.key_prefix));
}

void meshcore_runtime_pending_telemetry_register(
    uint32_t tag, const uint8_t *key_prefix, uint8_t permission_mask) {
  if (key_prefix == NULL) {
    return;
  }

  meshcore_runtime_pending_telemetry_clear();
  meshcore_runtime_context_get()->pending_telemetry.valid = true;
  meshcore_runtime_context_get()->pending_telemetry.tag = tag;
  meshcore_runtime_context_get()->pending_telemetry.expires_at_ms =
      meshcore_clock_millis_get() + MESHCORE_RUNTIME_REQUEST_TIMEOUT_MS;
  memcpy(meshcore_runtime_context_get()->pending_telemetry.key_prefix, key_prefix,
         sizeof(meshcore_runtime_context_get()->pending_telemetry.key_prefix));
  meshcore_runtime_context_get()->pending_telemetry.permission_mask = permission_mask;
}

void meshcore_runtime_pending_binary_register(uint32_t tag,
                                              const uint8_t *key_prefix) {
  if (key_prefix == NULL) {
    return;
  }

  meshcore_runtime_pending_binary_clear();
  meshcore_runtime_context_get()->pending_binary.valid = true;
  meshcore_runtime_context_get()->pending_binary.tag = tag;
  meshcore_runtime_context_get()->pending_binary.expires_at_ms =
      meshcore_clock_millis_get() + MESHCORE_RUNTIME_REQUEST_TIMEOUT_MS;
  memcpy(meshcore_runtime_context_get()->pending_binary.key_prefix, key_prefix,
         sizeof(meshcore_runtime_context_get()->pending_binary.key_prefix));
}

bool meshcore_runtime_pending_discovery_handle(
    const uint8_t *key_prefix, struct meshcore_packet *packet, uint8_t *path,
    uint8_t path_len_field, uint8_t extra_type, uint8_t *extra,
    uint8_t extra_len) {
  meshcore_common_peer_path_event_t event = {0};
  uint8_t out_path_len;
  uint8_t path_hash_size;
  uint32_t tag = 0U;

  if (!meshcore_runtime_context_get()->pending_discovery.valid || key_prefix == NULL ||
      extra_type != PAYLOAD_TYPE_RESPONSE ||
      memcmp(meshcore_runtime_context_get()->pending_discovery.key_prefix, key_prefix,
             sizeof(meshcore_runtime_context_get()->pending_discovery.key_prefix)) != 0 ||
      !meshcore_runtime_path_len_decode(path_len_field, &out_path_len,
                                        &path_hash_size)) {
    return false;
  }
  if (out_path_len > 0U && path == NULL) {
    return false;
  }
  if (extra == NULL || extra_len < sizeof(tag)) {
    return false;
  }
  memcpy(&tag, extra, sizeof(tag));
  if (meshcore_runtime_context_get()->pending_discovery.tag != tag) {
    return false;
  }

  event.tag = meshcore_runtime_context_get()->pending_discovery.tag;
  if (out_path_len > 0U) {
    memcpy(event.out_path, path, out_path_len);
  }

  memcpy(event.key_prefix, key_prefix, sizeof(event.key_prefix));
  event.timestamp = meshcore_runtime_timestamp_now_seconds();
  event.last_seen_snr = packet == NULL ? 0 : packet->snr_q4;
  event.out_path_len = out_path_len;
  event.path_hash_size = path_hash_size;

  (void)meshcore_platform_bridge_peer_path_publish(&event, true);
  meshcore_runtime_pending_discovery_clear();
  return true;
}

bool meshcore_runtime_pending_trace_handle(
    uint32_t tag, uint8_t flags, const uint8_t *path_snrs, uint8_t path_snr_count,
    uint8_t path_hash_bytes, int8_t response_snr) {
  int8_t out_path_snr[MESHCORE_MAX_PATH_LEN];
  int8_t return_path_snr[MESHCORE_MAX_PATH_LEN];
  uint8_t hash_size;
  uint8_t hash_count;
  uint8_t forward_hops;
  uint8_t out_count = 0U;
  uint8_t return_count = 0U;

  if (!meshcore_runtime_context_get()->pending_trace.valid ||
      meshcore_runtime_context_get()->pending_trace.tag != tag || path_snrs == NULL) {
    return false;
  }

  hash_size = (uint8_t)(1U << (flags & 0x03U));
  if (hash_size == 0U || hash_size == 4U ||
      path_hash_bytes == 0U || (path_hash_bytes % hash_size) != 0U) {
    meshcore_runtime_pending_trace_clear();
    return false;
  }

  hash_count = (uint8_t)(path_hash_bytes / hash_size);
  if (path_snr_count != hash_count || (hash_count % 2U) == 0U) {
    meshcore_runtime_pending_trace_clear();
    return false;
  }

  forward_hops = (uint8_t)((hash_count - 1U) / 2U);
  if (forward_hops > 0U) {
    memcpy(out_path_snr, path_snrs, forward_hops);
    out_count = forward_hops;
  }
  meshcore_runtime_append_snr_with_trunc(out_path_snr, &out_count,
                                         ARRAY_SIZE(out_path_snr),
                                         (int8_t)path_snrs[forward_hops]);

  if (forward_hops > 0U) {
    memcpy(return_path_snr, &path_snrs[forward_hops + 1U], forward_hops);
    return_count = forward_hops;
  }
  meshcore_runtime_append_snr_with_trunc(return_path_snr, &return_count,
                                         ARRAY_SIZE(return_path_snr),
                                         response_snr);

  (void)meshcore_platform_bridge_trace_path_handler(
      meshcore_runtime_context_get()->pending_trace.key_prefix, 1U, tag, out_path_snr, out_count,
      return_path_snr, return_count, true, response_snr,
      meshcore_runtime_timestamp_now_seconds());
  meshcore_runtime_pending_trace_clear();
  return true;
}

bool meshcore_runtime_pending_telemetry_handle(const uint8_t *key_prefix,
                                                      uint32_t timestamp,
                                                      const uint8_t *payload,
                                                      size_t payload_len) {
  uint32_t tag = 0U;

  if (!meshcore_runtime_context_get()->pending_telemetry.valid || key_prefix == NULL ||
      payload == NULL || payload_len < sizeof(tag)) {
    return false;
  }

  memcpy(&tag, payload, sizeof(tag));
  if (meshcore_runtime_context_get()->pending_telemetry.tag != tag ||
      memcmp(meshcore_runtime_context_get()->pending_telemetry.key_prefix, key_prefix,
             sizeof(meshcore_runtime_context_get()->pending_telemetry.key_prefix)) != 0) {
    return false;
  }

  (void)meshcore_platform_bridge_telemetry_handler(
      key_prefix, timestamp, tag, &payload[sizeof(tag)], payload_len - sizeof(tag));
  meshcore_runtime_pending_telemetry_clear();
  return true;
}

bool meshcore_runtime_pending_binary_handle(const uint8_t *key_prefix,
                                            uint32_t timestamp,
                                            const uint8_t *payload,
                                            size_t payload_len) {
  uint32_t tag = 0U;

  if (!meshcore_runtime_context_get()->pending_binary.valid || key_prefix == NULL ||
      payload == NULL || payload_len < sizeof(tag)) {
    return false;
  }

  memcpy(&tag, payload, sizeof(tag));
  if (meshcore_runtime_context_get()->pending_binary.tag != tag ||
      memcmp(meshcore_runtime_context_get()->pending_binary.key_prefix, key_prefix,
             sizeof(meshcore_runtime_context_get()->pending_binary.key_prefix)) != 0) {
    return false;
  }

  (void)meshcore_platform_bridge_binary_response_handler(key_prefix, timestamp, tag,
                                         &payload[sizeof(tag)],
                                         payload_len - sizeof(tag));
  meshcore_runtime_pending_binary_clear();
  return true;
}

#ifdef MESHCORE_ENABLE_TEST_HOOKS
int meshcore_test_runtime_expected_ack_used_count_get(void) {
  int count = 0;
  size_t i;

  for (i = 0U; i < MESHCORE_RUNTIME_EXPECTED_ACK_TABLE_SIZE; i++) {
    if (meshcore_runtime_context_get()->expected_acks[i].valid) {
      count++;
    }
  }

  return count;
}

bool meshcore_test_runtime_expected_ack_peek(uint32_t *ack_crc,
                                             uint8_t *target,
                                             uint8_t *attempt,
                                             unsigned long *expires_at_ms) {
  size_t i;

  for (i = 0U; i < MESHCORE_RUNTIME_EXPECTED_ACK_TABLE_SIZE; i++) {
    const struct meshcore_runtime_expected_ack *entry =
        &meshcore_runtime_context_get()->expected_acks[i];

    if (!entry->valid) {
      continue;
    }

    if (ack_crc != NULL) {
      *ack_crc = entry->ack_crc;
    }
    if (target != NULL) {
      memcpy(target, entry->target, sizeof(entry->target));
    }
    if (attempt != NULL) {
      *attempt = entry->attempt;
    }
    if (expires_at_ms != NULL) {
      *expires_at_ms = entry->expires_at_ms;
    }
    return true;
  }

  return false;
}

bool meshcore_test_runtime_pending_discovery_is_valid(void) {
  return meshcore_runtime_context_get()->pending_discovery.valid;
}

bool meshcore_test_runtime_pending_discovery_get(uint32_t *tag,
                                                 uint8_t *key_prefix,
                                                 unsigned long *expires_at_ms) {
  if (!meshcore_runtime_context_get()->pending_discovery.valid) {
    return false;
  }

  if (tag != NULL) {
    *tag = meshcore_runtime_context_get()->pending_discovery.tag;
  }
  if (key_prefix != NULL) {
    memcpy(key_prefix, meshcore_runtime_context_get()->pending_discovery.key_prefix,
           sizeof(meshcore_runtime_context_get()->pending_discovery.key_prefix));
  }
  if (expires_at_ms != NULL) {
    *expires_at_ms = meshcore_runtime_context_get()->pending_discovery.expires_at_ms;
  }
  return true;
}

bool meshcore_test_runtime_pending_trace_is_valid(void) {
  return meshcore_runtime_context_get()->pending_trace.valid;
}

bool meshcore_test_runtime_pending_trace_get(uint32_t *tag, uint8_t *key_prefix,
                                             unsigned long *expires_at_ms) {
  if (!meshcore_runtime_context_get()->pending_trace.valid) {
    return false;
  }

  if (tag != NULL) {
    *tag = meshcore_runtime_context_get()->pending_trace.tag;
  }
  if (key_prefix != NULL) {
    memcpy(key_prefix, meshcore_runtime_context_get()->pending_trace.key_prefix,
           sizeof(meshcore_runtime_context_get()->pending_trace.key_prefix));
  }
  if (expires_at_ms != NULL) {
    *expires_at_ms = meshcore_runtime_context_get()->pending_trace.expires_at_ms;
  }
  return true;
}

bool meshcore_test_runtime_pending_telemetry_is_valid(void) {
  return meshcore_runtime_context_get()->pending_telemetry.valid;
}

bool meshcore_test_runtime_pending_telemetry_get(uint32_t *tag,
                                                 uint8_t *key_prefix,
                                                 uint8_t *permission_mask,
                                                 unsigned long *expires_at_ms) {
  if (!meshcore_runtime_context_get()->pending_telemetry.valid) {
    return false;
  }

  if (tag != NULL) {
    *tag = meshcore_runtime_context_get()->pending_telemetry.tag;
  }
  if (key_prefix != NULL) {
    memcpy(key_prefix, meshcore_runtime_context_get()->pending_telemetry.key_prefix,
           sizeof(meshcore_runtime_context_get()->pending_telemetry.key_prefix));
  }
  if (permission_mask != NULL) {
    *permission_mask = meshcore_runtime_context_get()->pending_telemetry.permission_mask;
  }
  if (expires_at_ms != NULL) {
    *expires_at_ms = meshcore_runtime_context_get()->pending_telemetry.expires_at_ms;
  }
  return true;
}

bool meshcore_test_runtime_simulate_ack_recv(uint32_t ack_crc) {
  return meshcore_runtime_expected_ack_handle(ack_crc);
}

bool meshcore_test_runtime_simulate_peer_path_recv(
    const uint8_t *key_prefix, const uint8_t *out_path, uint8_t out_path_len,
    uint8_t path_hash_size, uint8_t extra_type, const int8_t *out_path_snr,
    uint8_t out_path_snr_count, const int8_t *return_path_snr,
    uint8_t return_path_snr_count, int8_t response_snr) {
  struct meshcore_packet packet;
  uint8_t path_len_field;
  uint8_t extra[1U + MESHCORE_MAX_PATH_LEN];

  if (key_prefix == NULL ||
      !meshcore_runtime_path_len_encode(path_hash_size, out_path_len,
                                        &path_len_field)) {
    return false;
  }

  memset(&packet, 0, sizeof(packet));
  packet.snr_q4 = response_snr;
  if (return_path_snr_count > 0U) {
    size_t i;
    size_t tuple_size = (size_t)path_hash_size;

    packet.header = ROUTE_TYPE_TRANSPORT_DIRECT;
    packet.transport_codes[0] = MESHCORE_SNR_TRANSPORT_CODE0;
    packet.transport_codes[1] = MESHCORE_SNR_TRANSPORT_CODE1;
    meshcore_packet_set_path_hash_size_and_count(&packet, path_hash_size,
                                                 return_path_snr_count);
    for (i = 0U; i < return_path_snr_count; i++) {
      size_t base = i * tuple_size;

      memset(&packet.path[base], 0, tuple_size);
      packet.path[base + tuple_size - 1U] = (uint8_t)return_path_snr[i];
    }
  }

  if (extra_type == PATH_EXTRA_TYPE_SNR) {
    extra[0] = out_path_snr_count;
    if (out_path_snr_count > 0U && out_path_snr != NULL) {
      memcpy(&extra[1], out_path_snr, out_path_snr_count);
    }
    return meshcore_runtime_pending_discovery_handle(
        key_prefix, &packet, (uint8_t *)out_path, path_len_field, extra_type,
        extra, (uint8_t)(1U + out_path_snr_count));
  }

  return meshcore_runtime_pending_discovery_handle(
      key_prefix, &packet, (uint8_t *)out_path, path_len_field, extra_type,
      (uint8_t *)out_path_snr, out_path_snr_count);
}

bool meshcore_test_runtime_simulate_trace_recv(uint32_t tag, uint8_t flags,
                                               const int8_t *path_snrs,
                                               uint8_t path_snr_count,
                                               int8_t response_snr) {
  return meshcore_runtime_pending_trace_handle(
      tag, flags, (const uint8_t *)path_snrs, path_snr_count, path_snr_count,
      response_snr);
}

bool meshcore_test_runtime_simulate_telemetry_response_recv(
    const uint8_t *key_prefix, uint32_t tag, const uint8_t *payload,
    size_t payload_len) {
  uint8_t data[sizeof(tag) + MESHCORE_MAX_SERVICE_RESPONSE_PAYLOAD_LEN];

  if (key_prefix == NULL || payload == NULL ||
      payload_len > MESHCORE_MAX_SERVICE_RESPONSE_PAYLOAD_LEN) {
    return false;
  }

  memcpy(data, &tag, sizeof(tag));
  if (payload_len > 0U) {
    memcpy(&data[sizeof(tag)], payload, payload_len);
  }

  return meshcore_runtime_pending_telemetry_handle(
      key_prefix, meshcore_runtime_timestamp_now_seconds(), data,
      sizeof(tag) + payload_len);
}
#endif /* MESHCORE_ENABLE_TEST_HOOKS */

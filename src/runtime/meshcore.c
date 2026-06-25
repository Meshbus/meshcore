// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_runtime_internal.h"

#include <errno.h>
#include <string.h>

#include "meshcore_runtime_bridge.h"
#include "meshcore_platform_bridge.h"

static struct meshcore_runtime s_runtime;

struct meshcore_runtime *meshcore_runtime_context_get(void) {
  return &s_runtime;
}

static void meshcore_runtime_protocol_on_peer_data_recv(
    void *user_data, struct meshcore_mesh *mesh, struct meshcore_packet *packet,
    uint8_t type, const struct meshcore_common_peer_identity *sender,
    const uint8_t *secret, uint8_t *data, size_t len) {
  (void)user_data;
  (void)mesh;

  meshcore_runtime_on_peer_data_recv(packet, type, sender, secret, data, len);
}

static void meshcore_runtime_protocol_on_trace_recv(
    void *user_data, struct meshcore_mesh *mesh, struct meshcore_packet *packet,
    uint32_t tag, uint32_t auth_code, uint8_t flags,
    const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) {
  (void)user_data;
  (void)mesh;

  meshcore_runtime_on_trace_recv(packet, tag, auth_code, flags, path_snrs,
                                 path_hashes, path_len);
}

static bool meshcore_runtime_protocol_on_peer_path_recv(
    void *user_data, struct meshcore_mesh *mesh, struct meshcore_packet *packet,
    const struct meshcore_common_peer_identity *sender, const uint8_t *secret,
    uint8_t *path, uint8_t path_len, uint8_t extra_type, uint8_t *extra,
    uint8_t extra_len) {
  (void)user_data;
  (void)mesh;

  return meshcore_runtime_on_peer_path_recv(packet, sender, secret, path,
                                            path_len, extra_type, extra,
                                            extra_len);
}

static void meshcore_runtime_protocol_on_advert_recv(
    void *user_data, struct meshcore_mesh *mesh, struct meshcore_packet *packet,
    const struct meshcore_identity *identity, uint32_t timestamp,
    const uint8_t *app_data, size_t app_data_len) {
  (void)user_data;
  (void)mesh;

  meshcore_runtime_on_advert_recv(packet, identity, timestamp, app_data,
                                  app_data_len);
}

static void meshcore_runtime_protocol_on_control_data_recv(
    void *user_data, struct meshcore_mesh *mesh, struct meshcore_packet *packet) {
  (void)user_data;
  (void)mesh;

  meshcore_runtime_on_control_data_recv(packet);
}

static void meshcore_runtime_protocol_on_raw_data_recv(
    void *user_data, struct meshcore_mesh *mesh, struct meshcore_packet *packet) {
  (void)user_data;
  (void)mesh;

  meshcore_runtime_on_raw_data_recv(packet);
}

static void meshcore_runtime_protocol_on_group_data_recv(
    void *user_data, struct meshcore_mesh *mesh, struct meshcore_packet *packet,
    uint8_t type, const struct meshcore_group_channel *channel, uint8_t *data,
    size_t len) {
  (void)user_data;
  (void)mesh;

  meshcore_runtime_on_group_data_recv(packet, type, channel, data, len);
}

static void meshcore_runtime_protocol_on_ack_recv(
    void *user_data, struct meshcore_mesh *mesh, struct meshcore_packet *packet,
    uint32_t ack_crc) {
  (void)user_data;
  (void)mesh;

  meshcore_runtime_on_ack_recv(packet, ack_crc);
}

static const struct meshcore_mesh_runtime_ops s_runtime_mesh_ops = {
    .allow_packet_forward = meshcore_runtime_protocol_allow_packet_forward,
    .get_retransmit_delay = meshcore_runtime_protocol_get_retransmit_delay,
    .get_direct_retransmit_delay =
        meshcore_runtime_protocol_get_direct_retransmit_delay,
    .on_peer_data_recv = meshcore_runtime_protocol_on_peer_data_recv,
    .on_trace_recv = meshcore_runtime_protocol_on_trace_recv,
    .on_peer_path_recv = meshcore_runtime_protocol_on_peer_path_recv,
    .on_advert_recv = meshcore_runtime_protocol_on_advert_recv,
    .on_control_data_recv = meshcore_runtime_protocol_on_control_data_recv,
    .on_raw_data_recv = meshcore_runtime_protocol_on_raw_data_recv,
    .on_group_data_recv = meshcore_runtime_protocol_on_group_data_recv,
    .on_ack_recv = meshcore_runtime_protocol_on_ack_recv,
};

int meshcore_runtime_require_initialized(void) {
  return meshcore_runtime_context_get()->initialized ? 0 : -ENODEV;
}

static int meshcore_runtime_next_deadline_get(uint32_t now_ms,
                                              bool *has_deadline,
                                              uint32_t *next_deadline_ms);

bool meshcore_runtime_time_reached(unsigned long now_ms,
                                   unsigned long expires_at_ms) {
  return (long)(now_ms - expires_at_ms) >= 0;
}

bool meshcore_runtime_deadline_accumulate(uint32_t now_ms,
                                          uint32_t candidate_ms,
                                          bool *has_deadline,
                                          uint32_t *deadline_ms) {
  if (has_deadline == NULL || deadline_ms == NULL) {
    return false;
  }

  if ((int32_t)(candidate_ms - now_ms) <= 0) {
    *deadline_ms = now_ms;
    *has_deadline = true;
    return true;
  }

  if (!*has_deadline || (int32_t)(candidate_ms - *deadline_ms) < 0) {
    *deadline_ms = candidate_ms;
    *has_deadline = true;
  }

  return false;
}

void meshcore_runtime_timer_sync(uint32_t now_ms) {
  bool has_deadline = false;
  uint32_t next_deadline_ms = 0U;

  if (!meshcore_runtime_context_get()->initialized) {
    return;
  }

  if (meshcore_runtime_next_deadline_get(now_ms, &has_deadline,
                                         &next_deadline_ms) != 0) {
    return;
  }

  if (has_deadline) {
    (void)meshcore_platform_timer_arm(next_deadline_ms);
  } else {
    meshcore_platform_timer_cancel();
  }
}

uint32_t meshcore_runtime_timestamp_now_seconds(void) {
  uint32_t timestamp = meshcore_platform_bridge_rtc_get_current_time();

  if (timestamp == 0U) {
    timestamp = (uint32_t)(meshcore_clock_millis_get() / 1000UL);
  }

  return timestamp == 0U ? 1U : timestamp;
}

bool meshcore_runtime_path_len_decode(uint8_t path_len_field,
                                      uint8_t *out_path_bytes,
                                      uint8_t *out_path_hash_size) {
  uint8_t path_hash_size;
  uint8_t hop_count;
  uint16_t path_bytes;

  if (out_path_bytes == NULL || out_path_hash_size == NULL) {
    return false;
  }

  path_hash_size = (uint8_t)((path_len_field >> 6) + 1U);
  hop_count = (uint8_t)(path_len_field & 0x3FU);
  if (path_hash_size == 4U) {
    return false;
  }

  path_bytes = (uint16_t)path_hash_size * (uint16_t)hop_count;
  if (path_bytes > MESHCORE_MAX_PATH_LEN) {
    return false;
  }

  *out_path_hash_size = path_hash_size;
  *out_path_bytes = (uint8_t)path_bytes;
  return true;
}

bool meshcore_runtime_extract_return_path_snrs(
    const struct meshcore_packet *packet, int8_t *snrs, uint8_t *hop_count) {
  uint8_t tuple_size;
  uint8_t hops;
  uint8_t i;

  if (packet == NULL || snrs == NULL || hop_count == NULL ||
      !meshcore_packet_has_snr_flag(packet)) {
    return false;
  }

  tuple_size = meshcore_packet_get_path_hash_size(packet);
  if (tuple_size < 2U || tuple_size == 4U) {
    return false;
  }

  hops = meshcore_packet_get_path_hash_count(packet);
  for (i = 0U; i < hops; i++) {
    size_t base = (size_t)i * tuple_size;

    snrs[i] = (int8_t)packet->path[base + tuple_size - 1U];
  }
  *hop_count = hops;
  return true;
}

void meshcore_runtime_append_snr_with_trunc(int8_t *snrs, uint8_t *len,
                                            uint8_t max_len, int8_t snr_q4) {
  if (snrs == NULL || len == NULL || *len >= max_len) {
    return;
  }

  snrs[*len] = snr_q4;
  (*len)++;
}

uint8_t meshcore_runtime_role_to_advert_type(
    meshcore_common_node_role_t role) {
  switch (role) {
    case MESHCORE_COMMON_NODE_ROLE_CHAT:
      return ADV_TYPE_CHAT;
    case MESHCORE_COMMON_NODE_ROLE_REPEATER:
      return ADV_TYPE_REPEATER;
    case MESHCORE_COMMON_NODE_ROLE_ROOM:
      return ADV_TYPE_ROOM;
    case MESHCORE_COMMON_NODE_ROLE_SENSOR:
      return ADV_TYPE_SENSOR;
    default:
      return ADV_TYPE_NONE;
  }
}

uint8_t meshcore_runtime_normalize_path_hash_size(uint8_t path_hash_size) {
  if (path_hash_size == 0U || path_hash_size > 3U) {
    return MESHCORE_RUNTIME_PATH_HASH_SIZE_DEFAULT;
  }

  return path_hash_size;
}

uint8_t meshcore_runtime_local_path_hash_size_get(void) {
  meshcore_common_node_runtime_policy_t policy;

  if (meshcore_platform_bridge_node_runtime_policy_get(&policy) != 0) {
    return MESHCORE_RUNTIME_PATH_HASH_SIZE_DEFAULT;
  }

  return meshcore_runtime_normalize_path_hash_size(policy.path_hash_size);
}

bool meshcore_runtime_path_len_encode(uint8_t path_hash_size,
                                      uint8_t path_byte_len,
                                      uint8_t *out_path_len) {
  uint8_t hop_count;

  if (out_path_len == NULL) {
    return false;
  }
  if (path_hash_size == 0U || path_hash_size > 3U) {
    return false;
  }
  if (path_byte_len == 0U) {
    *out_path_len = (uint8_t)((path_hash_size - 1U) << 6);
    return true;
  }
  if ((path_byte_len % path_hash_size) != 0U) {
    return false;
  }

  hop_count = (uint8_t)(path_byte_len / path_hash_size);
  if (hop_count > 63U) {
    return false;
  }

  *out_path_len = (uint8_t)(((path_hash_size - 1U) << 6) | hop_count);
  return true;
}

int meshcore_runtime_sync_local_identity(void) {
  meshcore_common_node_identity_t identity;
  int rc;

  rc = meshcore_platform_bridge_node_identity_get(&identity);
  if (rc != 0) {
    return rc;
  }

  meshcore_local_identity_init_from_bytes(&meshcore_runtime_context_get()->mesh.self_id,
                                          identity.private_key,
                                          identity.public_key);
  return 0;
}

bool meshcore_runtime_local_identity_get(
    meshcore_common_node_identity_t *out) {
  return out != NULL && meshcore_platform_bridge_node_identity_get(out) == 0;
}

bool meshcore_runtime_local_role_is(meshcore_common_node_role_t role) {
  meshcore_common_node_identity_t identity;

  return meshcore_runtime_local_identity_get(&identity) && identity.role == role;
}

bool meshcore_runtime_peer_path_get(const uint8_t *public_key,
                                    meshcore_common_peer_path_t *out,
                                    uint8_t *out_path_len) {
  uint8_t path_len;

  if (public_key == NULL || out == NULL || out_path_len == NULL ||
      meshcore_platform_bridge_peer_path_get_by_key(public_key, out) != 0) {
    return false;
  }

  if (out->out_path_len == 0U && !out->is_neighbor) {
    return false;
  }

  if (!meshcore_runtime_path_len_encode(
          meshcore_runtime_normalize_path_hash_size(out->path_hash_size),
          out->out_path_len, &path_len)) {
    return false;
  }

  *out_path_len = path_len;
  return true;
}

static void meshcore_runtime_state_reset(void) {
  meshcore_runtime_context_get()->last_now_ms = 0U;
  memset(meshcore_runtime_context_get()->expected_acks, 0,
         sizeof(meshcore_runtime_context_get()->expected_acks));
  meshcore_runtime_context_get()->expected_ack_next = 0U;
  memset(&meshcore_runtime_context_get()->pending_discovery, 0,
         sizeof(meshcore_runtime_context_get()->pending_discovery));
  memset(&meshcore_runtime_context_get()->pending_trace, 0,
         sizeof(meshcore_runtime_context_get()->pending_trace));
  memset(&meshcore_runtime_context_get()->pending_telemetry, 0,
         sizeof(meshcore_runtime_context_get()->pending_telemetry));
  memset(&meshcore_runtime_context_get()->pending_binary, 0,
         sizeof(meshcore_runtime_context_get()->pending_binary));
  meshcore_rtc_clock_state_init(&meshcore_runtime_context_get()->rtc_clock_state);
}

static int meshcore_runtime_begin(void) {
  int rc;

  meshcore_packet_queue_manager_prepare(&meshcore_runtime_context_get()->packet_manager,
                                        MESHCORE_RUNTIME_PACKET_POOL_SIZE);
  if (!meshcore_runtime_context_get()->packet_manager.initialized) {
    return -ENOMEM;
  }

  meshcore_tables_init(&meshcore_runtime_context_get()->tables);
  meshcore_mesh_init(&meshcore_runtime_context_get()->mesh,
                     &meshcore_runtime_context_get()->packet_manager,
                     &meshcore_runtime_context_get()->tables);
  meshcore_mesh_set_runtime_ops(&meshcore_runtime_context_get()->mesh,
                                &s_runtime_mesh_ops,
                                meshcore_runtime_context_get());
  meshcore_mesh_begin(&meshcore_runtime_context_get()->mesh);
  meshcore_runtime_state_reset();
  rc = meshcore_runtime_sync_local_identity();
  if (rc != 0) {
    return rc;
  }
  meshcore_runtime_context_get()->initialized = true;
  meshcore_runtime_timer_sync((uint32_t)meshcore_clock_millis_get());
  return 0;
}

static void meshcore_runtime_end(void) {
  meshcore_clock_millis_override_clear();
  meshcore_packet_queue_manager_deinit(&meshcore_runtime_context_get()->packet_manager);
  memset(meshcore_runtime_context_get(), 0, sizeof(*meshcore_runtime_context_get()));
}

int meshcore_init(void) {
  if (meshcore_runtime_context_get()->initialized) {
    return -EALREADY;
  }

  meshcore_runtime_end();
  int rc = meshcore_runtime_begin();
  if (rc != 0) {
    meshcore_runtime_end();
  }
  return rc;
}

void meshcore_deinit(void) {
  if (!meshcore_runtime_context_get()->initialized) {
    return;
  }

  meshcore_runtime_end();
}

int meshcore_timer_fired(uint32_t now_ms) {
  if (!meshcore_runtime_context_get()->initialized) {
    return -ENODEV;
  }

  meshcore_clock_millis_override_set(now_ms);
  meshcore_runtime_correlation_cleanup(now_ms);
  meshcore_mesh_loop(&meshcore_runtime_context_get()->mesh);
  meshcore_clock_millis_override_clear();
  meshcore_runtime_context_get()->last_now_ms = now_ms;
  meshcore_runtime_timer_sync(now_ms);
  return 0;
}

int meshcore_radio_rx_inject(const uint8_t *data, size_t len,
                             int16_t rssi_dbm, int8_t snr_q4,
                             uint32_t now_ms) {
  int rc;

  if (!meshcore_runtime_context_get()->initialized) {
    return -ENODEV;
  }
  if (data == NULL || len == 0U || len > MESHCORE_MAX_TRANS_UNIT_LEN) {
    return -EINVAL;
  }

  meshcore_clock_millis_override_set(now_ms);
  rc = meshcore_dispatcher_receive_raw(&meshcore_runtime_context_get()->mesh.dispatcher,
                                       data, len, rssi_dbm, snr_q4);
  if (rc == 0) {
    meshcore_mesh_loop(&meshcore_runtime_context_get()->mesh);
  } else if (rc == -EINVAL) {
    rc = 0;
  }
  meshcore_clock_millis_override_clear();
  meshcore_runtime_context_get()->last_now_ms = now_ms;
  meshcore_runtime_timer_sync(now_ms);
  return rc;
}

int meshcore_radio_tx_done(uint32_t now_ms, bool success) {
  bool handled;

  if (!meshcore_runtime_context_get()->initialized) {
    return -ENODEV;
  }

  meshcore_clock_millis_override_set(now_ms);
  handled = meshcore_dispatcher_tx_done(&meshcore_runtime_context_get()->mesh.dispatcher,
                                        success);
  meshcore_mesh_loop(&meshcore_runtime_context_get()->mesh);
  meshcore_clock_millis_override_clear();
  meshcore_runtime_context_get()->last_now_ms = now_ms;
  meshcore_runtime_timer_sync(now_ms);
  return handled ? 0 : -EALREADY;
}

static int meshcore_runtime_next_deadline_get(uint32_t now_ms,
                                              bool *has_deadline,
                                              uint32_t *next_deadline_ms) {
  bool have = false;
  uint32_t deadline = 0U;
  uint32_t candidate = 0U;

  if (!meshcore_runtime_context_get()->initialized) {
    return -ENODEV;
  }
  if (has_deadline == NULL || next_deadline_ms == NULL) {
    return -EINVAL;
  }

  if (meshcore_runtime_correlation_next_deadline_get(now_ms, &candidate)) {
    if (meshcore_runtime_deadline_accumulate(now_ms, candidate, &have,
                                             &deadline)) {
      *has_deadline = have;
      *next_deadline_ms = deadline;
      return 0;
    }
  }

  if (meshcore_dispatcher_next_deadline_get(
          &meshcore_runtime_context_get()->mesh.dispatcher, now_ms, &candidate)) {
    (void)meshcore_runtime_deadline_accumulate(now_ms, candidate, &have,
                                               &deadline);
  }

  *has_deadline = have;
  *next_deadline_ms = deadline;
  return 0;
}

#ifdef MESHCORE_ENABLE_TEST_HOOKS
bool meshcore_test_runtime_context_is_default(void) {
  return meshcore_runtime_context_get() == &s_runtime;
}

bool meshcore_test_runtime_is_initialized(void) {
  return meshcore_runtime_context_get()->initialized;
}

unsigned long meshcore_test_runtime_dispatcher_last_budget_update_get(void) {
  return meshcore_runtime_context_get()->mesh.dispatcher.last_budget_update;
}

unsigned long meshcore_test_runtime_dispatcher_next_floor_calib_time_get(void) {
  return meshcore_runtime_context_get()->mesh.dispatcher.next_floor_calib_time;
}

uint32_t meshcore_test_runtime_dispatcher_num_sent_flood_get(void) {
  return meshcore_dispatcher_get_num_sent_flood(
      &meshcore_runtime_context_get()->mesh.dispatcher);
}

uint32_t meshcore_test_runtime_dispatcher_num_sent_direct_get(void) {
  return meshcore_dispatcher_get_num_sent_direct(
      &meshcore_runtime_context_get()->mesh.dispatcher);
}

int meshcore_test_runtime_dispatcher_outbound_total_get(void) {
  return meshcore_packet_queue_manager_get_outbound_total(
      &meshcore_runtime_context_get()->packet_manager);
}

bool meshcore_test_runtime_dispatcher_has_active_outbound(void) {
  return meshcore_runtime_context_get()->mesh.dispatcher.outbound != NULL;
}

uint32_t meshcore_test_runtime_last_now_ms_get(void) {
  return meshcore_runtime_context_get()->last_now_ms;
}
#endif /* MESHCORE_ENABLE_TEST_HOOKS */

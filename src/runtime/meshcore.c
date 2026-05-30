// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_runtime_internal.h"

#include <errno.h>
#include <stdio.h>
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
    .on_peer_data_recv = meshcore_runtime_protocol_on_peer_data_recv,
    .on_trace_recv = meshcore_runtime_protocol_on_trace_recv,
    .on_peer_path_recv = meshcore_runtime_protocol_on_peer_path_recv,
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

static uint8_t meshcore_runtime_telemetry_wire_decode(uint8_t wire_mask) {
  return (uint8_t)(~wire_mask) & MESHCORE_RUNTIME_TELEM_PERM_SUPPORTED;
}

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

static bool meshcore_runtime_local_identity_get(
    meshcore_common_node_identity_t *out) {
  return out != NULL && meshcore_platform_bridge_node_identity_get(out) == 0;
}

static bool meshcore_runtime_local_role_is(meshcore_common_node_role_t role) {
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

  if (!meshcore_runtime_path_len_encode(
          meshcore_runtime_normalize_path_hash_size(out->path_hash_size),
          out->out_path_len, &path_len)) {
    return false;
  }

  *out_path_len = path_len;
  return true;
}

static void meshcore_runtime_peer_seen_update(const uint8_t *public_key,
                                              bool has_snr, int8_t snr_q4) {
  if (public_key == NULL) {
    return;
  }

  (void)meshcore_platform_bridge_peer_seen_update(public_key, has_snr, snr_q4);
}

static meshcore_common_message_route_t
meshcore_runtime_message_route_from_packet(const struct meshcore_packet *packet) {
  if (packet == NULL) {
    return MESHCORE_COMMON_MESSAGE_ROUTE_UNSPECIFIED;
  }
  if (meshcore_packet_is_route_flood(packet)) {
    return MESHCORE_COMMON_MESSAGE_ROUTE_FLOOD;
  }
  if (meshcore_packet_is_route_direct(packet)) {
    return MESHCORE_COMMON_MESSAGE_ROUTE_DIRECT;
  }

  return MESHCORE_COMMON_MESSAGE_ROUTE_UNSPECIFIED;
}

static void meshcore_runtime_text_message_publish(
    const meshcore_common_peer_identity_t *peer, const struct meshcore_packet *packet,
    uint32_t sender_timestamp, const uint8_t *text, size_t text_len) {
  meshcore_common_message_t message = {0};

  if (peer == NULL || text == NULL || text_len == 0U) {
    return;
  }

  message.type = MESHCORE_COMMON_MESSAGE_TYPE_RECEIVE_NODE;
  message.route = meshcore_runtime_message_route_from_packet(packet);
  message.target_len = MESHCORE_MESSAGE_TARGET_PREFIX_BYTES;
  memcpy(message.target, peer->public_key,
         MIN(sizeof(message.target), sizeof(peer->public_key)));
  (void)snprintf(message.sender_name, sizeof(message.sender_name), "%s",
                 peer->name);
  message.payload_len =
      (uint16_t)MIN(text_len, (size_t)sizeof(message.payload));
  memcpy(message.payload, text, message.payload_len);
  message.sender_timestamp = sender_timestamp;
  message.has_rx_snr = packet != NULL;
  if (packet != NULL) {
    message.rx_snr = (float)packet->snr_q4;
  }

  (void)meshcore_platform_bridge_message_handler(&message);
}

static uint8_t meshcore_runtime_packet_path_copy(
    const struct meshcore_packet *packet, uint8_t *dest, size_t dest_len) {
  uint8_t path_bytes;

  if (packet == NULL || dest == NULL || dest_len == 0U) {
    return 0U;
  }

  path_bytes = meshcore_packet_get_path_byte_len(packet);
  path_bytes = MIN((size_t)path_bytes, dest_len);
  if (path_bytes > 0U) {
    memcpy(dest, packet->path, path_bytes);
  }
  return (uint8_t)packet->path_len;
}

static void meshcore_runtime_channel_text_publish(
    const struct meshcore_group_channel *channel,
    const struct meshcore_packet *packet, uint32_t sender_timestamp,
    const uint8_t *text, size_t text_len) {
  meshcore_common_message_t message = {0};
  const uint8_t *payload = text;
  size_t payload_len = text_len;
  const uint8_t *colon;
  size_t name_len;

  if (channel == NULL || text == NULL || text_len == 0U) {
    return;
  }

  colon = memchr(text, ':', text_len);
  if (colon != NULL) {
    name_len = (size_t)(colon - text);
    if (name_len == 0U) {
      (void)snprintf(message.sender_name, sizeof(message.sender_name), "%s",
                     "unknown");
    } else {
      name_len = MIN(name_len, sizeof(message.sender_name) - 1U);
      memcpy(message.sender_name, text, name_len);
      message.sender_name[name_len] = '\0';
    }

    payload = colon + 1U;
    payload_len = (size_t)(&text[text_len] - payload);
    if (payload_len > 0U && payload[0] == ' ') {
      payload++;
      payload_len--;
    }
  } else {
    (void)snprintf(message.sender_name, sizeof(message.sender_name), "%s",
                   "unknown");
  }
  if (payload_len == 0U) {
    return;
  }

  message.type = MESHCORE_COMMON_MESSAGE_TYPE_RECEIVE_CHANNEL;
  message.route = meshcore_runtime_message_route_from_packet(packet);
  message.target_len = MESHCORE_CHANNEL_HASH_BYTES;
  memcpy(message.target, channel->hash,
         MIN(sizeof(message.target), sizeof(channel->hash)));
  message.payload_len =
      (uint16_t)MIN(payload_len, (size_t)sizeof(message.payload));
  memcpy(message.payload, payload, message.payload_len);
  message.sender_timestamp = sender_timestamp;
  message.has_rx_snr = packet != NULL;
  if (packet != NULL) {
    message.rx_snr = (float)packet->snr_q4;
  }

  (void)meshcore_platform_bridge_message_handler(&message);
}

static bool meshcore_runtime_build_forward_path_snrs(
    const struct meshcore_packet *packet, uint8_t *hashes, size_t hashes_cap,
    int8_t *snrs, size_t snrs_cap, uint8_t *hop_count,
    uint8_t *hash_len_bytes) {
  uint8_t tuple_size;
  uint8_t hash_size;
  uint8_t hops;
  size_t hash_bytes;
  uint8_t i;

  if (packet == NULL || hashes == NULL || snrs == NULL || hop_count == NULL ||
      hash_len_bytes == NULL || !meshcore_packet_has_snr_flag(packet)) {
    return false;
  }

  tuple_size = meshcore_packet_get_path_hash_size(packet);
  if (tuple_size < 2U || tuple_size == 4U) {
    return false;
  }

  hash_size = (uint8_t)(tuple_size - 1U);
  hops = meshcore_packet_get_path_hash_count(packet);
  hash_bytes = (size_t)hops * hash_size;
  if ((size_t)hops * tuple_size > MESHCORE_MAX_PATH_LEN ||
      hash_bytes > hashes_cap || hops > snrs_cap || hash_bytes > UINT8_MAX) {
    return false;
  }

  for (i = 0U; i < hops; i++) {
    size_t base = (size_t)i * tuple_size;

    memcpy(&hashes[(size_t)i * hash_size], &packet->path[base], hash_size);
    snrs[i] = (int8_t)packet->path[base + hash_size];
  }

  *hop_count = hops;
  *hash_len_bytes = (uint8_t)hash_bytes;
  return true;
}

static void meshcore_runtime_send_ack_direct(
    const meshcore_common_peer_identity_t *peer, uint32_t ack_hash) {
  meshcore_common_peer_path_t peer_path;
  uint8_t path_len = 0U;
  uint8_t extra_acks;
  struct meshcore_packet *packet;
  uint32_t delay_ms = MESHCORE_RUNTIME_TXT_ACK_DELAY_MS;

  if (peer == NULL) {
    return;
  }

  if (!meshcore_runtime_peer_path_get(peer->public_key, &peer_path, &path_len)) {
    packet = meshcore_mesh_create_ack(&meshcore_runtime_context_get()->mesh, ack_hash);
    if (packet != NULL) {
      meshcore_mesh_send_flood(&meshcore_runtime_context_get()->mesh, packet, delay_ms,
                               meshcore_runtime_local_path_hash_size_get());
    }
    return;
  }

  extra_acks = meshcore_platform_bridge_mesh_extra_ack_transmit_count_get();
  if (extra_acks > 0U) {
    packet = meshcore_mesh_create_multi_ack(&meshcore_runtime_context_get()->mesh, ack_hash, 1U);
    if (packet != NULL) {
      meshcore_mesh_send_direct(&meshcore_runtime_context_get()->mesh, packet, peer_path.out_path,
                                path_len, delay_ms);
      delay_ms += 300U;
    }
  }

  packet = meshcore_mesh_create_ack(&meshcore_runtime_context_get()->mesh, ack_hash);
  if (packet != NULL) {
    meshcore_mesh_send_direct(&meshcore_runtime_context_get()->mesh, packet, peer_path.out_path,
                              path_len, delay_ms);
  }
}

static void meshcore_runtime_send_ack_or_path_return(
    const meshcore_common_peer_identity_t *peer, const uint8_t *secret,
    const struct meshcore_packet *packet, uint32_t ack_hash) {
  struct meshcore_identity recipient;
  struct meshcore_packet *reply;

  if (peer == NULL || secret == NULL || packet == NULL) {
    return;
  }

  if (meshcore_packet_is_route_flood(packet)) {
    meshcore_identity_init_from_pub_key(&recipient, peer->public_key);
    reply = meshcore_mesh_create_path_return_by_identity(
        &meshcore_runtime_context_get()->mesh, &recipient, secret, packet->path,
        (uint8_t)packet->path_len, PAYLOAD_TYPE_ACK, (const uint8_t *)&ack_hash,
        sizeof(ack_hash));
    if (reply != NULL) {
      meshcore_mesh_send_flood(&meshcore_runtime_context_get()->mesh, reply,
                               MESHCORE_RUNTIME_TXT_ACK_DELAY_MS,
                               meshcore_runtime_local_path_hash_size_get());
    }
    return;
  }

  meshcore_runtime_send_ack_direct(peer, ack_hash);
}

static void meshcore_runtime_handle_text_follow_up(
    const meshcore_common_peer_identity_t *peer, const uint8_t *secret,
    const struct meshcore_packet *packet, uint8_t flags, const uint8_t *data,
    size_t text_end) {
  uint32_t ack_hash = 0U;

  if (peer == NULL || secret == NULL || packet == NULL || data == NULL ||
      text_end <= sizeof(uint32_t)) {
    return;
  }

  if (flags == MESHCORE_RUNTIME_TXT_TYPE_PLAIN) {
    meshcore_utils_sha256_two_fragments(
        (uint8_t *)&ack_hash, sizeof(ack_hash), data, (int)text_end,
        peer->public_key, (int)sizeof(peer->public_key));
  } else if (flags == MESHCORE_RUNTIME_TXT_TYPE_SIGNED_PLAIN) {
    meshcore_utils_sha256_two_fragments(
        (uint8_t *)&ack_hash, sizeof(ack_hash), data, (int)text_end,
        meshcore_runtime_context_get()->mesh.self_id.identity.pub_key,
        (int)sizeof(meshcore_runtime_context_get()->mesh.self_id.identity.pub_key));
  } else {
    return;
  }

  meshcore_runtime_send_ack_or_path_return(peer, secret, packet, ack_hash);
}

static void meshcore_runtime_handle_discover_path_request(
    const meshcore_common_peer_identity_t *peer, const uint8_t *secret,
    const struct meshcore_packet *packet) {
  static const uint16_t transport_codes[2] = {MESHCORE_SNR_TRANSPORT_CODE0,
                                              MESHCORE_SNR_TRANSPORT_CODE1};
  struct meshcore_identity recipient;
  struct meshcore_packet *reply;
  uint8_t path_hashes[MESHCORE_MAX_PATH_LEN];
  int8_t path_snrs[MESHCORE_MAX_PATH_LEN];
  uint8_t extra[MESHCORE_MAX_PATH_LEN + 1U];
  const uint8_t *path_ptr = NULL;
  uint8_t path_len = 0U;
  uint8_t extra_type = 0U;
  size_t extra_len = 0U;
  uint8_t hop_count = 0U;
  uint8_t hash_len_bytes = 0U;
  bool snr_ok;

  if (peer == NULL || secret == NULL || packet == NULL ||
      !meshcore_packet_is_route_flood(packet)) {
    return;
  }

  path_ptr = packet->path;
  path_len = (uint8_t)packet->path_len;
  snr_ok = meshcore_runtime_build_forward_path_snrs(
      packet, path_hashes, sizeof(path_hashes), path_snrs, ARRAY_SIZE(path_snrs),
      &hop_count, &hash_len_bytes);
  if (snr_ok) {
    uint8_t snr_count = hop_count;

    meshcore_runtime_append_snr_with_trunc(path_snrs, &snr_count,
                                           ARRAY_SIZE(path_snrs),
                                           packet->snr_q4);
    if (meshcore_runtime_path_len_encode(
            (uint8_t)(meshcore_packet_get_path_hash_size(packet) - 1U),
            hash_len_bytes, &path_len)) {
      path_ptr = path_hashes;
      extra_type = PATH_EXTRA_TYPE_SNR;
      extra[0] = snr_count;
      memcpy(&extra[1], path_snrs, snr_count);
      extra_len = (size_t)snr_count + 1U;
    } else {
      snr_ok = false;
      path_ptr = packet->path;
      path_len = (uint8_t)packet->path_len;
    }
  }

  meshcore_identity_init_from_pub_key(&recipient, peer->public_key);
  reply = meshcore_mesh_create_path_return_by_identity(
      &meshcore_runtime_context_get()->mesh, &recipient, secret, path_ptr, path_len, extra_type,
      extra_len > 0U ? extra : NULL, extra_len);
  if (reply == NULL) {
    return;
  }

  if (snr_ok) {
    meshcore_mesh_send_flood_by_transport_codes(
        &meshcore_runtime_context_get()->mesh, reply, transport_codes,
        MESHCORE_RUNTIME_SERVER_RESPONSE_DELAY_MS,
        meshcore_runtime_local_path_hash_size_get());
  } else {
    meshcore_mesh_send_flood(&meshcore_runtime_context_get()->mesh, reply,
                             MESHCORE_RUNTIME_SERVER_RESPONSE_DELAY_MS,
                             meshcore_runtime_local_path_hash_size_get());
  }
}

static void meshcore_runtime_handle_telemetry_request(
    const meshcore_common_peer_identity_t *peer, const uint8_t *secret,
    const struct meshcore_packet *packet, uint32_t tag, uint8_t permission_mask) {
  meshcore_platform_request_source_t requester = {0};
  meshcore_platform_telemetry_payload_t telemetry = {0};
  meshcore_common_peer_path_t peer_path;
  struct meshcore_identity recipient;
  struct meshcore_packet *reply;
  uint8_t response[MESHCORE_MAX_TELEMETRY_PAYLOAD_LEN + sizeof(tag)];
  uint8_t direct_path_len = 0U;
  size_t response_len;

  if (peer == NULL || secret == NULL || packet == NULL) {
    return;
  }

  memcpy(requester.key_prefix, peer->public_key, sizeof(requester.key_prefix));
  if (meshcore_platform_bridge_telemetry_node_get(&requester, permission_mask,
                                                   &telemetry) != 0) {
    return;
  }

  memcpy(response, &tag, sizeof(tag));
  memcpy(&response[sizeof(tag)], telemetry.payload, telemetry.payload_len);
  response_len = sizeof(tag) + telemetry.payload_len;
  meshcore_identity_init_from_pub_key(&recipient, peer->public_key);

  if (meshcore_packet_is_route_flood(packet)) {
    reply = meshcore_mesh_create_path_return_by_identity(
        &meshcore_runtime_context_get()->mesh, &recipient, secret, packet->path,
        (uint8_t)packet->path_len, PAYLOAD_TYPE_RESPONSE, response, response_len);
    if (reply != NULL) {
      meshcore_mesh_send_flood(&meshcore_runtime_context_get()->mesh, reply,
                               MESHCORE_RUNTIME_SERVER_RESPONSE_DELAY_MS,
                               meshcore_runtime_local_path_hash_size_get());
    }
    return;
  }

  reply = meshcore_mesh_create_datagram(&meshcore_runtime_context_get()->mesh,
                                        PAYLOAD_TYPE_RESPONSE, &recipient, secret,
                                        response, response_len);
  if (reply == NULL) {
    return;
  }

  if (meshcore_runtime_peer_path_get(peer->public_key, &peer_path, &direct_path_len)) {
    meshcore_mesh_send_direct(&meshcore_runtime_context_get()->mesh, reply, peer_path.out_path,
                              direct_path_len,
                              MESHCORE_RUNTIME_SERVER_RESPONSE_DELAY_MS);
  } else {
    meshcore_mesh_send_flood(&meshcore_runtime_context_get()->mesh, reply,
                             MESHCORE_RUNTIME_SERVER_RESPONSE_DELAY_MS,
                             meshcore_runtime_local_path_hash_size_get());
  }
}

static void meshcore_runtime_peer_path_publish(
    struct meshcore_packet *packet, const meshcore_common_peer_identity_t *peer,
    uint8_t *path, uint8_t path_len, uint8_t extra_type, uint8_t *extra,
    uint8_t extra_len) {
  meshcore_common_peer_path_event_t event = {0};
  int8_t out_path_snr[MESHCORE_MAX_PATH_LEN];
  int8_t return_path_snr[MESHCORE_MAX_PATH_LEN];
  uint8_t path_byte_len;
  uint8_t path_hash_size;
  uint8_t out_count = 0U;
  uint8_t return_count = 0U;

  if (peer == NULL ||
      !meshcore_runtime_path_len_decode(path_len, &path_byte_len, &path_hash_size) ||
      (path_byte_len > 0U && path == NULL) ||
      (extra_len > 0U && extra == NULL)) {
    return;
  }

  if (meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_CHAT) &&
      extra_type == PATH_EXTRA_TYPE_SNR && extra_len >= 1U) {
    out_count = extra[0];
    if (out_count > (extra_len - 1U)) {
      out_count = (uint8_t)(extra_len - 1U);
    }
    out_count = MIN(out_count, (uint8_t)ARRAY_SIZE(out_path_snr));
    if (out_count > 0U) {
      memcpy(out_path_snr, &extra[1], out_count);
    }
    if (meshcore_runtime_extract_return_path_snrs(packet, return_path_snr,
                                                  &return_count)) {
      meshcore_runtime_append_snr_with_trunc(return_path_snr, &return_count,
                                             ARRAY_SIZE(return_path_snr),
                                             packet->snr_q4);
    }
  }

  memcpy(event.key_prefix, peer->public_key, sizeof(event.key_prefix));
  event.timestamp = meshcore_runtime_timestamp_now_seconds();
  event.last_seen_snr = packet == NULL ? 0 : packet->snr_q4;
  event.out_path_len = path_byte_len;
  event.path_hash_size = path_hash_size;
  event.out_path_snr_count = out_count;
  event.return_path_snr_count = return_count;
  if (path_byte_len > 0U) {
    memcpy(event.out_path, path, path_byte_len);
  }
  if (out_count > 0U) {
    memcpy(event.out_path_snr, out_path_snr, out_count);
  }
  if (return_count > 0U) {
    memcpy(event.return_path_snr, return_path_snr, return_count);
  }

  (void)meshcore_platform_bridge_peer_path_handler(&event);
}

static void meshcore_runtime_on_peer_data_recv_internal(
    struct meshcore_packet *packet, uint8_t type,
    const meshcore_common_peer_identity_t *sender,
    const uint8_t *secret, uint8_t *data, size_t len) {
  uint32_t timestamp = 0U;

  if (packet == NULL || data == NULL || secret == NULL || sender == NULL) {
    return;
  }

  if (len >= sizeof(timestamp)) {
    memcpy(&timestamp, data, sizeof(timestamp));
  }

  if (meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_CHAT) &&
      type == PAYLOAD_TYPE_TXT_MSG && len > 5U) {
    uint8_t flags = data[4] >> 2;
    size_t text_off = 5U;
    size_t text_len;
    const void *nul;

    if (flags == MESHCORE_RUNTIME_TXT_TYPE_SIGNED_PLAIN) {
      if (len <= 9U || meshcore_runtime_sync_local_identity() != 0) {
        meshcore_runtime_peer_seen_update(sender->public_key, true, packet->snr_q4);
        return;
      }
      text_off = 9U;
    } else if (flags == MESHCORE_RUNTIME_TXT_TYPE_CLI_DATA ||
               flags != MESHCORE_RUNTIME_TXT_TYPE_PLAIN) {
      text_off = 0U;
    }

    if (text_off > 0U) {
      nul = memchr(&data[text_off], 0, len - text_off);
      text_len = nul != NULL ? (size_t)((const uint8_t *)nul - &data[text_off])
                             : (len - text_off);
      if (text_len > 0U) {
        meshcore_runtime_text_message_publish(sender, packet, timestamp,
                                              &data[text_off], text_len);
      }
      meshcore_runtime_handle_text_follow_up(sender, secret, packet, flags, data,
                                             text_off + text_len);
    }
  } else if (meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_CHAT) &&
             type == PAYLOAD_TYPE_REQ && len > 5U) {
    uint8_t req_type = data[4];
    uint8_t permission_mask = meshcore_runtime_telemetry_wire_decode(data[5]);
    bool wants_snr_path = meshcore_packet_has_snr_flag(packet);

    if (req_type == MESHCORE_RUNTIME_REQ_TYPE_DISCOVER_PATH_SNR ||
        (req_type == MESHCORE_RUNTIME_REQ_TYPE_GET_TELEMETRY_DATA &&
         wants_snr_path)) {
      meshcore_runtime_handle_discover_path_request(sender, secret, packet);
    } else if (req_type == MESHCORE_RUNTIME_REQ_TYPE_GET_TELEMETRY_DATA) {
      meshcore_runtime_handle_telemetry_request(sender, secret, packet, timestamp,
                                                permission_mask);
    }
  } else if (type == PAYLOAD_TYPE_RESPONSE && len >= sizeof(uint32_t)) {
    uint32_t response_timestamp = meshcore_runtime_timestamp_now_seconds();

    if (!meshcore_runtime_pending_telemetry_handle(sender->public_key,
                                                   response_timestamp, data,
                                                   len)) {
      (void)meshcore_runtime_pending_binary_handle(sender->public_key,
                                                   response_timestamp, data,
                                                   len);
    }
  }

  meshcore_runtime_peer_seen_update(sender->public_key, true, packet->snr_q4);
}

static bool meshcore_runtime_handle_control_discover_request(
    struct meshcore_packet *packet) {
  meshcore_common_node_identity_t identity;
  struct meshcore_packet *reply;
  uint8_t data[6U + MESHCORE_PUBLIC_KEY_SIZE];
  uint32_t last_modify = 0U;
  uint32_t tag;
  uint32_t since = 0U;
  uint8_t filter;
  bool prefix_only;

  if (packet == NULL || packet->payload_len < 6U ||
      !meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_REPEATER) ||
      (packet->payload[0] & 0xF0U) != MESHCORE_RUNTIME_CTL_TYPE_NODE_DISCOVER_REQ ||
      meshcore_platform_bridge_node_config_last_modify_get(&last_modify) != 0 ||
      !meshcore_runtime_local_identity_get(&identity)) {
    return false;
  }

  filter = packet->payload[1];
  memcpy(&tag, &packet->payload[2], sizeof(tag));
  if (packet->payload_len >= 10U) {
    memcpy(&since, &packet->payload[6], sizeof(since));
  }
  if ((filter & (1U << ADV_TYPE_REPEATER)) == 0U || last_modify < since) {
    return false;
  }

  prefix_only = (packet->payload[0] & 0x01U) != 0U;
  data[0] = MESHCORE_RUNTIME_CTL_TYPE_NODE_DISCOVER_RESP | ADV_TYPE_REPEATER;
  data[1] = (uint8_t)(packet->snr_q4 & 0xFFU);
  memcpy(&data[2], &tag, sizeof(tag));
  memcpy(&data[6], identity.public_key, sizeof(identity.public_key));

  reply = meshcore_mesh_create_control_data(
      &meshcore_runtime_context_get()->mesh, data,
      prefix_only ? (size_t)(6U + 8U) : (size_t)(6U + sizeof(identity.public_key)));
  if (reply == NULL) {
    return false;
  }

  meshcore_mesh_send_zero_hop(&meshcore_runtime_context_get()->mesh, reply,
                              meshcore_platform_bridge_mesh_retransmit_delay_get(reply) * 4U);
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
  (void)meshcore_runtime_sync_local_identity();
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

void meshcore_runtime_on_ack_recv(struct meshcore_packet *packet,
                                  uint32_t ack_crc) {
  if (!meshcore_runtime_context_get()->initialized) {
    meshcore_platform_bridge_mesh_on_ack_recv(packet, ack_crc);
    return;
  }

  (void)meshcore_runtime_expected_ack_handle(ack_crc);
}

void meshcore_runtime_on_peer_data_recv(struct meshcore_packet *packet,
                                        uint8_t type,
                                        const meshcore_common_peer_identity_t *sender,
                                        const uint8_t *secret, uint8_t *data,
                                        size_t len) {
  if (!meshcore_runtime_context_get()->initialized) {
    meshcore_platform_bridge_mesh_on_peer_data_recv(packet, type, sender, secret, data,
                                           len);
    return;
  }

  meshcore_runtime_on_peer_data_recv_internal(packet, type, sender, secret, data,
                                              len);
}

bool meshcore_runtime_on_peer_path_recv(struct meshcore_packet *packet,
                                        const meshcore_common_peer_identity_t *sender,
                                        const uint8_t *secret, uint8_t *path,
                                        uint8_t path_len, uint8_t extra_type,
                                        uint8_t *extra, uint8_t extra_len) {
  uint32_t ack_crc = 0U;

  if (!meshcore_runtime_context_get()->initialized) {
    return meshcore_platform_bridge_mesh_on_peer_path_recv(packet, sender, secret, path,
                                                  path_len, extra_type, extra,
                                                  extra_len);
  }

  if (sender == NULL) {
    return false;
  }

  if (meshcore_runtime_pending_discovery_handle(sender->public_key, packet, path,
                                                path_len, extra_type, extra,
                                                extra_len)) {
    return true;
  }

  if (meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_CHAT) &&
      extra_type == PAYLOAD_TYPE_ACK && extra != NULL &&
      extra_len >= sizeof(ack_crc)) {
    memcpy(&ack_crc, extra, sizeof(ack_crc));
    (void)meshcore_runtime_expected_ack_handle(ack_crc);
  } else if (meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_CHAT) &&
             extra_type == PAYLOAD_TYPE_RESPONSE && extra != NULL &&
             extra_len >= sizeof(uint32_t)) {
    uint32_t response_timestamp = meshcore_runtime_timestamp_now_seconds();

    if (!meshcore_runtime_pending_telemetry_handle(sender->public_key,
                                                   response_timestamp, extra,
                                                   extra_len)) {
      (void)meshcore_runtime_pending_binary_handle(sender->public_key,
                                                   response_timestamp, extra,
                                                   extra_len);
    }
  }

  meshcore_runtime_peer_path_publish(packet, sender, path, path_len, extra_type,
                                     extra, extra_len);
  return meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_CHAT);
}

void meshcore_runtime_on_control_data_recv(struct meshcore_packet *packet) {
  meshcore_common_control_data_event_t event = {0};

  if (!meshcore_runtime_context_get()->initialized) {
    meshcore_platform_bridge_mesh_on_control_data_recv(packet);
    return;
  }
  if (packet == NULL || packet->payload_len == 0U ||
      packet->payload_len > sizeof(event.payload)) {
    return;
  }

  (void)meshcore_runtime_handle_control_discover_request(packet);

  event.path_len =
      meshcore_runtime_packet_path_copy(packet, event.path, sizeof(event.path));
  event.payload_len = (uint8_t)packet->payload_len;
  memcpy(event.payload, packet->payload, event.payload_len);
  event.has_rx_snr = true;
  event.rx_snr_q4 = packet->snr_q4;
  (void)meshcore_platform_bridge_control_data_handler(&event);
}

void meshcore_runtime_on_raw_data_recv(struct meshcore_packet *packet) {
  meshcore_common_raw_data_event_t event = {0};

  if (!meshcore_runtime_context_get()->initialized) {
    meshcore_platform_bridge_mesh_on_raw_data_recv(packet);
    return;
  }
  if (packet == NULL || packet->payload_len == 0U ||
      packet->payload_len > sizeof(event.payload)) {
    return;
  }

  event.path_len =
      meshcore_runtime_packet_path_copy(packet, event.path, sizeof(event.path));
  event.payload_len = (uint8_t)packet->payload_len;
  memcpy(event.payload, packet->payload, event.payload_len);
  event.has_rx_snr = true;
  event.rx_snr_q4 = packet->snr_q4;
  (void)meshcore_platform_bridge_raw_data_handler(&event);
}

void meshcore_runtime_on_group_data_recv(
    struct meshcore_packet *packet, uint8_t type,
    const struct meshcore_group_channel *channel, uint8_t *data, size_t len) {
  if (!meshcore_runtime_context_get()->initialized) {
    meshcore_platform_bridge_mesh_on_group_data_recv(packet, type, channel, data, len);
    return;
  }
  if (!meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_CHAT) ||
      packet == NULL || channel == NULL || data == NULL) {
    return;
  }

  if (type == PAYLOAD_TYPE_GRP_TXT && len > 5U && (data[4] >> 2) == 0U) {
    const void *nul;
    size_t text_len;
    uint32_t sender_timestamp = 0U;

    memcpy(&sender_timestamp, data, sizeof(sender_timestamp));
    nul = memchr(&data[5], 0, len - 5U);
    text_len = nul != NULL ? (size_t)((const uint8_t *)nul - &data[5])
                           : (len - 5U);
    if (text_len > 0U) {
      meshcore_runtime_channel_text_publish(channel, packet, sender_timestamp,
                                            &data[5], text_len);
    }
  } else if (type == PAYLOAD_TYPE_GRP_DATA && len >= 3U) {
    meshcore_common_channel_data_event_t event = {0};
    uint16_t data_type;
    uint8_t data_len;
    size_t available_len;

    data_type = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    data_len = data[2];
    available_len = len - 3U;
    if (data_len > available_len || data_len > sizeof(event.payload)) {
      return;
    }

    memcpy(event.channel_hash, channel->hash, sizeof(event.channel_hash));
    event.path_len =
        meshcore_runtime_packet_path_copy(packet, event.path, sizeof(event.path));
    event.data_type = data_type;
    event.payload_len = data_len;
    if (data_len > 0U) {
      memcpy(event.payload, &data[3], data_len);
    }
    event.has_rx_snr = true;
    event.rx_snr_q4 = packet->snr_q4;
    (void)meshcore_platform_bridge_channel_data_handler(&event);
  }
}

void meshcore_runtime_on_trace_recv(struct meshcore_packet *packet,
                                    uint32_t tag, uint32_t auth_code,
                                    uint8_t flags, const uint8_t *path_snrs,
                                    const uint8_t *path_hashes,
                                    uint8_t path_len) {
  if (!meshcore_runtime_context_get()->initialized) {
    meshcore_platform_bridge_mesh_on_trace_recv(packet, tag, auth_code, flags, path_snrs,
                                       path_hashes, path_len);
    return;
  }

  (void)meshcore_runtime_pending_trace_handle(
      tag, flags, path_snrs,
      packet == NULL ? 0U : meshcore_packet_get_path_hash_count(packet),
      path_len, packet == NULL ? 0 : packet->snr_q4);
  (void)auth_code;
  (void)path_hashes;
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

// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_runtime_internal.h"

#include <string.h>

#include "meshcore_platform_bridge.h"

static uint8_t meshcore_runtime_telemetry_wire_decode(uint8_t wire_mask) {
  return (uint8_t)(~wire_mask) & MESHCORE_RUNTIME_TELEM_PERM_SUPPORTED;
}

static bool meshcore_runtime_local_role_answers_telemetry(void) {
  return meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_CHAT) ||
         meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_REPEATER) ||
         meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_ROOM) ||
         meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_SENSOR);
}

static void meshcore_runtime_peer_seen_update(const uint8_t *public_key,
                                              bool has_snr, int8_t snr_q4) {
  if (public_key == NULL) {
    return;
  }

  (void)meshcore_platform_bridge_peer_seen_update(public_key, has_snr, snr_q4);
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
  } else if (type == PAYLOAD_TYPE_REQ && len > sizeof(uint32_t)) {
    bool handled_request = false;

    if (len > 5U) {
      uint8_t req_type = data[4];
      uint8_t permission_mask = meshcore_runtime_telemetry_wire_decode(data[5]);
      bool wants_snr_path = meshcore_packet_has_snr_flag(packet);

      if (meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_CHAT) &&
          (req_type == MESHCORE_RUNTIME_REQ_TYPE_DISCOVER_PATH_SNR ||
           (req_type == MESHCORE_RUNTIME_REQ_TYPE_GET_TELEMETRY_DATA &&
            wants_snr_path))) {
        meshcore_runtime_handle_discover_path_request(sender, secret, packet);
        handled_request = true;
      } else if (req_type == MESHCORE_RUNTIME_REQ_TYPE_GET_TELEMETRY_DATA &&
                 meshcore_runtime_local_role_answers_telemetry()) {
        meshcore_runtime_handle_telemetry_request(sender, secret, packet, timestamp,
                                                  permission_mask);
        handled_request = true;
      }
    }

    if (!handled_request) {
      meshcore_runtime_binary_request_publish(sender, packet, timestamp,
                                              &data[sizeof(uint32_t)],
                                              len - sizeof(uint32_t));
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

static bool meshcore_runtime_advert_is_share(
    const struct meshcore_packet *packet) {
  return packet != NULL && meshcore_packet_has_transport_codes(packet) &&
         packet->transport_codes[0] == 0U && packet->transport_codes[1] == 0U;
}

void meshcore_runtime_on_advert_recv(struct meshcore_packet *packet,
                                     const struct meshcore_identity *identity,
                                     uint32_t timestamp,
                                     const uint8_t *app_data,
                                     size_t app_data_len) {
  struct meshcore_advert_data_parser parser;

  if (!meshcore_runtime_context_get()->initialized) {
    meshcore_platform_bridge_mesh_on_advert_recv(packet, identity, timestamp,
                                                 app_data, app_data_len);
    return;
  }
  if (packet == NULL || identity == NULL || app_data == NULL ||
      app_data_len == 0U || app_data_len > UINT8_MAX) {
    return;
  }

  meshcore_advert_data_parser_init(&parser, app_data, (uint8_t)app_data_len);
  if (!meshcore_advert_data_parser_is_valid(&parser)) {
    return;
  }

  if (meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_CHAT)) {
    if (meshcore_advert_data_parser_has_name(&parser)) {
      (void)meshcore_runtime_advert_publish(packet, identity, timestamp,
                                            &parser);
    }
    return;
  }

  if (meshcore_runtime_local_role_is(MESHCORE_COMMON_NODE_ROLE_REPEATER) &&
      packet->path_len == 0U && !meshcore_runtime_advert_is_share(packet) &&
      meshcore_advert_data_parser_get_type(&parser) == ADV_TYPE_REPEATER) {
    (void)meshcore_runtime_advert_publish(packet, identity, timestamp,
                                          &parser);
  }
}

void meshcore_runtime_on_control_data_recv(struct meshcore_packet *packet) {
  if (!meshcore_runtime_context_get()->initialized) {
    meshcore_platform_bridge_mesh_on_control_data_recv(packet);
    return;
  }
  if (packet == NULL || packet->payload_len == 0U ||
      packet->payload_len > MESHCORE_MAX_CONTROL_DATA_PAYLOAD_LEN) {
    return;
  }

  (void)meshcore_runtime_handle_control_discover_request(packet);
  meshcore_runtime_node_discover_publish(packet);
  meshcore_runtime_control_data_publish(packet);
}

void meshcore_runtime_on_raw_data_recv(struct meshcore_packet *packet) {
  if (!meshcore_runtime_context_get()->initialized) {
    meshcore_platform_bridge_mesh_on_raw_data_recv(packet);
    return;
  }
  if (packet == NULL || packet->payload_len == 0U ||
      packet->payload_len > MESHCORE_MAX_RAW_DATA_PAYLOAD_LEN) {
    return;
  }

  meshcore_runtime_raw_data_publish(packet);
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
    meshcore_runtime_channel_data_publish(channel, packet, data, len);
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

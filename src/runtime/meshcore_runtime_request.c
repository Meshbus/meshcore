// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_runtime_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "meshcore_platform_bridge.h"

static int meshcore_runtime_request_execute_now(
    const struct meshcore_runtime_request_slot *request);

static bool meshcore_runtime_permission_mask_valid(uint8_t permission_mask) {
  return (permission_mask & (uint8_t)(~MESHCORE_RUNTIME_TELEM_PERM_SUPPORTED)) ==
         0U;
}

static uint8_t meshcore_runtime_telemetry_wire_encode(uint8_t permission_mask) {
  return (uint8_t)(~permission_mask);
}

static bool meshcore_runtime_request_needs_packet(uint8_t type) {
  switch (type) {
    case MESHCORE_RUNTIME_REQUEST_NODE_ADVERT:
    case MESHCORE_RUNTIME_REQUEST_NODE_PEER_ADVERT:
    case MESHCORE_RUNTIME_REQUEST_MESSAGE_SEND_TO_NODE:
    case MESHCORE_RUNTIME_REQUEST_MESSAGE_SEND_TO_CHANNEL:
    case MESHCORE_RUNTIME_REQUEST_NODE_DISCOVER_PATH:
    case MESHCORE_RUNTIME_REQUEST_NODE_TRACE_PATH:
    case MESHCORE_RUNTIME_REQUEST_NODE_TELEMETRY:
    case MESHCORE_RUNTIME_REQUEST_NODE_BINARY:
    case MESHCORE_RUNTIME_REQUEST_CHANNEL_DATA:
    case MESHCORE_RUNTIME_REQUEST_RAW_DATA:
    case MESHCORE_RUNTIME_REQUEST_CONTROL_DATA:
      return true;
    default:
      return false;
  }
}

static void meshcore_runtime_request_log_transient_failure(uint8_t request_type,
                                                           int err_code) {
  if (err_code == -ENOBUFS || err_code == -EAGAIN || err_code == -EBUSY) {
    meshcore_platform_bridge_request_error(request_type, err_code);
  }
}

static int meshcore_runtime_request_validate_node_peer_advert(
    const uint8_t *raw_advert, size_t raw_advert_len) {
  struct meshcore_packet packet;

  if (raw_advert == NULL || raw_advert_len == 0U ||
      raw_advert_len > MESHCORE_MAX_RAW_ADVERT_LEN) {
    return -EINVAL;
  }
  meshcore_packet_init(&packet);
  if (!meshcore_packet_read_from(&packet, raw_advert, (uint8_t)raw_advert_len) ||
      meshcore_packet_get_payload_type(&packet) != PAYLOAD_TYPE_ADVERT) {
    return -EINVAL;
  }

  return 0;
}

static int meshcore_runtime_request_validate_message_send_to_node(
    const uint8_t *public_key, uint8_t attempt, const uint8_t *payload,
    size_t payload_len) {
  if (public_key == NULL || payload == NULL || payload_len == 0U ||
      payload_len > MESHCORE_MAX_MESSAGE_TX_LEN) {
    return -EINVAL;
  }
  if (attempt > 3U && payload_len > (MESHCORE_MAX_MESSAGE_TX_LEN - 2U)) {
    return -EINVAL;
  }

  return 0;
}

static int meshcore_runtime_request_validate_message_send_to_channel(
    const uint8_t *secret, size_t secret_len, const uint8_t *payload,
    size_t payload_len) {
  if (secret == NULL || payload == NULL ||
      (secret_len != MESHCORE_CHANNEL_SECRET_LEN_16 &&
       secret_len != MESHCORE_CHANNEL_SECRET_LEN_32) ||
      payload_len == 0U || payload_len > MESHCORE_MAX_MESSAGE_TX_LEN) {
    return -EINVAL;
  }

  return 0;
}

static int meshcore_runtime_request_validate_path(const uint8_t *path,
                                                  uint8_t path_len,
                                                  bool allow_unknown) {
  uint8_t path_bytes = 0U;
  uint8_t path_hash_size = 0U;

  if (path_len == MESHCORE_OUT_PATH_UNKNOWN) {
    return allow_unknown ? 0 : -EINVAL;
  }
  if (!meshcore_runtime_path_len_decode(path_len, &path_bytes,
                                        &path_hash_size)) {
    return -EINVAL;
  }
  if (path_bytes > 0U && path == NULL) {
    return -EINVAL;
  }

  (void)path_hash_size;
  return 0;
}

static int meshcore_runtime_request_validate_channel_data(
    const uint8_t *secret, size_t secret_len, const uint8_t *path,
    uint8_t path_len, const uint8_t *payload, size_t payload_len) {
  if (secret == NULL ||
      (secret_len != MESHCORE_CHANNEL_SECRET_LEN_16 &&
       secret_len != MESHCORE_CHANNEL_SECRET_LEN_32) ||
      (payload == NULL && payload_len > 0U) ||
      payload_len > MESHCORE_MAX_CHANNEL_DATA_PAYLOAD_LEN) {
    return -EINVAL;
  }

  return meshcore_runtime_request_validate_path(path, path_len, true);
}

static int meshcore_runtime_request_validate_public_key(
    const uint8_t *public_key) {
  return public_key == NULL ? -EINVAL : 0;
}

static int meshcore_runtime_request_validate_node_telemetry(
    const uint8_t *public_key, uint8_t permission_mask) {
  if (public_key == NULL ||
      !meshcore_runtime_permission_mask_valid(permission_mask)) {
    return -EINVAL;
  }

  return 0;
}

static int meshcore_runtime_request_validate_raw_data(
    const uint8_t *path, uint8_t path_len, const uint8_t *payload,
    size_t payload_len) {
  int rc;

  if (payload == NULL || payload_len == 0U ||
      payload_len > MESHCORE_MAX_RAW_DATA_PAYLOAD_LEN) {
    return -EINVAL;
  }

  rc = meshcore_runtime_request_validate_path(path, path_len, false);
  if (rc != 0) {
    return rc;
  }
  return path_len == MESHCORE_OUT_PATH_UNKNOWN ? -EINVAL : 0;
}

static int meshcore_runtime_request_validate_control_data(
    const uint8_t *payload, size_t payload_len) {
  if (payload == NULL || payload_len == 0U ||
      payload_len > MESHCORE_MAX_CONTROL_DATA_PAYLOAD_LEN ||
      (payload[0] & 0x80U) == 0U) {
    return -EINVAL;
  }

  return 0;
}

static int meshcore_runtime_request_validate_node_binary(
    const uint8_t *public_key, const uint8_t *payload, size_t payload_len) {
  if (public_key == NULL || payload == NULL || payload_len == 0U ||
      payload_len > MESHCORE_MAX_SERVICE_REQUEST_PAYLOAD_LEN) {
    return -EINVAL;
  }

  return 0;
}

static int meshcore_runtime_request_add(
    uint8_t type, const union meshcore_runtime_request_data *data) {
  struct meshcore_runtime_request_slot request = {0};

  request.used = true;
  request.type = type;
  if (data != NULL) {
    request.data = *data;
  }

  return meshcore_runtime_request_execute_now(&request);
}

static void meshcore_runtime_request_execute_node_advert(
    const struct meshcore_runtime_request_node_advert *request) {
  meshcore_common_node_identity_t node_identity;
  meshcore_common_node_advert_profile_t advert_profile;
  struct meshcore_advert_data_builder builder;
  struct meshcore_packet *packet;
  uint8_t app_data[MESHCORE_MAX_ADVERT_DATA_LEN];
  uint8_t advert_type;
  uint8_t app_data_len;

  if (request == NULL) {
    return;
  }
  if (meshcore_runtime_sync_local_identity() != 0 ||
      meshcore_platform_bridge_node_identity_get(&node_identity) != 0 ||
      meshcore_platform_bridge_node_advert_profile_get(&advert_profile) != 0) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_NODE_ADVERT, -EAGAIN);
    return;
  }

  advert_type = meshcore_runtime_role_to_advert_type(node_identity.role);
  if (advert_type == ADV_TYPE_NONE) {
    return;
  }

  meshcore_advert_data_builder_init_with_name(&builder, advert_type,
                                              node_identity.name);
  if (advert_profile.has_position) {
    builder.has_loc = true;
    builder.lat = advert_profile.latitude;
    builder.lon = advert_profile.longitude;
  }

  app_data_len = meshcore_advert_data_builder_encode_to(&builder, app_data);
  packet = meshcore_mesh_create_advert(&meshcore_runtime_context_get()->mesh,
                                       &meshcore_runtime_context_get()->mesh.self_id,
                                       app_data, app_data_len);
  if (packet == NULL) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_NODE_ADVERT, -ENOBUFS);
    return;
  }

  if (request->flood) {
    meshcore_mesh_send_flood(&meshcore_runtime_context_get()->mesh, packet, 0U,
                             meshcore_runtime_local_path_hash_size_get());
  } else {
    meshcore_mesh_send_zero_hop(&meshcore_runtime_context_get()->mesh, packet, 0U);
  }
}

static void meshcore_runtime_request_execute_node_peer_advert(
    const struct meshcore_runtime_request_node_peer_advert *request) {
  struct meshcore_packet *packet;

  if (request == NULL) {
    return;
  }

  packet = meshcore_dispatcher_obtain_new_packet(
      &meshcore_runtime_context_get()->mesh.dispatcher);
  if (packet == NULL) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_NODE_PEER_ADVERT, -ENOBUFS);
    return;
  }

  if (!meshcore_packet_read_from(packet, request->raw_advert,
                                 (uint8_t)request->raw_advert_len) ||
      meshcore_packet_get_payload_type(packet) != PAYLOAD_TYPE_ADVERT) {
    meshcore_dispatcher_release_packet(&meshcore_runtime_context_get()->mesh.dispatcher,
                                       packet);
    return;
  }

  meshcore_mesh_send_zero_hop(&meshcore_runtime_context_get()->mesh, packet, 0U);
}

static void meshcore_runtime_request_execute_message_send_to_node(
    const struct meshcore_runtime_request_message_send_to_node *request) {
  struct meshcore_identity recipient;
  struct meshcore_packet *packet;
  meshcore_common_peer_path_t peer_path;
  uint8_t path_len = 0U;
  uint8_t secret[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t data[5U + MESHCORE_MAX_MESSAGE_TX_LEN + 2U];
  uint32_t expected_ack = 0U;
  uint32_t timestamp;
  size_t len;
  bool can_direct = false;

  if (request == NULL) {
    return;
  }
  if (meshcore_runtime_sync_local_identity() != 0) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_MESSAGE_SEND_TO_NODE, -EAGAIN);
    return;
  }
  if (request->attempt > 3U && request->payload_len > (MESHCORE_MAX_MESSAGE_TX_LEN - 2U)) {
    return;
  }

  timestamp = meshcore_clock_rtc_get_current_time();
  memcpy(data, &timestamp, sizeof(timestamp));
  data[4] = (uint8_t)(request->attempt & 0x03U);
  memcpy(&data[5], request->payload, request->payload_len);
  len = 5U + request->payload_len;
  meshcore_utils_sha256_two_fragments(
      (uint8_t *)&expected_ack, sizeof(expected_ack), data,
      (int)(5U + request->payload_len),
      meshcore_runtime_context_get()->mesh.self_id.identity.pub_key,
      (int)sizeof(meshcore_runtime_context_get()->mesh.self_id.identity.pub_key));
  if (request->attempt > 3U) {
    data[len++] = 0U;
    data[len++] = request->attempt;
  }

  meshcore_identity_init_from_pub_key(&recipient, request->public_key);
  meshcore_local_identity_calc_shared_secret(&meshcore_runtime_context_get()->mesh.self_id,
                                             secret, request->public_key);
  packet = meshcore_mesh_create_datagram(&meshcore_runtime_context_get()->mesh,
                                         PAYLOAD_TYPE_TXT_MSG, &recipient,
                                         secret, data, len);
  if (packet == NULL) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_MESSAGE_SEND_TO_NODE, -ENOBUFS);
    memset(secret, 0, sizeof(secret));
    return;
  }
  memset(secret, 0, sizeof(secret));

  meshcore_runtime_expected_ack_register(expected_ack, request->public_key,
                                         request->attempt);
  if (!request->flood &&
      meshcore_runtime_peer_path_get(request->public_key, &peer_path,
                                     &path_len)) {
    can_direct = true;
  }

  if (request->flood || !can_direct) {
    meshcore_mesh_send_flood(&meshcore_runtime_context_get()->mesh, packet, 0U,
                             meshcore_runtime_local_path_hash_size_get());
  } else {
    meshcore_mesh_send_direct(&meshcore_runtime_context_get()->mesh, packet,
                              peer_path.out_path, path_len, 0U);
  }
}

static void meshcore_runtime_request_execute_message_send_to_channel(
    const struct meshcore_runtime_request_message_send_to_channel *request) {
  meshcore_common_node_identity_t node_identity;
  struct meshcore_group_channel channel;
  struct meshcore_packet *packet;
  char prefix[MESHCORE_NODE_NAME_MAX_LEN + 3U];
  uint8_t channel_hash[MESHCORE_CHANNEL_HASH_BYTES];
  uint8_t data[5U + MESHCORE_MAX_MESSAGE_TX_LEN + MESHCORE_NODE_NAME_MAX_LEN + 3U];
  uint32_t timestamp;
  size_t prefix_len;
  size_t len;

  if (request == NULL) {
    return;
  }
  if (meshcore_platform_bridge_node_identity_get(&node_identity) != 0) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_MESSAGE_SEND_TO_CHANNEL, -EAGAIN);
    return;
  }
  if (meshcore_platform_bridge_channel_secret_hash(request->secret, request->secret_len,
                                          channel_hash) != 0 ||
      meshcore_platform_bridge_channel_secret_match_exists(channel_hash[0], request->secret,
                                                  request->secret_len) <= 0) {
    return;
  }

  memset(&channel, 0, sizeof(channel));
  memcpy(channel.hash, channel_hash, sizeof(channel.hash));
  memcpy(channel.secret, request->secret, request->secret_len);

  timestamp = meshcore_clock_rtc_get_current_time();
  memcpy(data, &timestamp, sizeof(timestamp));
  data[4] = 0U;

  (void)snprintf(prefix, sizeof(prefix), "%s: ", node_identity.name);
  prefix_len = strnlen(prefix, sizeof(prefix));
  if (prefix_len >= MESHCORE_MAX_MESSAGE_TX_LEN ||
      request->payload_len + prefix_len > MESHCORE_MAX_MESSAGE_TX_LEN) {
    return;
  }

  memcpy(&data[5], prefix, prefix_len);
  memcpy(&data[5U + prefix_len], request->payload, request->payload_len);
  len = 5U + prefix_len + request->payload_len;
  packet = meshcore_mesh_create_group_datagram(&meshcore_runtime_context_get()->mesh,
                                               PAYLOAD_TYPE_GRP_TXT, &channel,
                                               data, len);
  if (packet == NULL) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_MESSAGE_SEND_TO_CHANNEL, -ENOBUFS);
    return;
  }

  meshcore_mesh_send_flood(&meshcore_runtime_context_get()->mesh, packet, 0U,
                           meshcore_runtime_local_path_hash_size_get());
}

static void meshcore_runtime_request_prepare_req_data(uint8_t req_data[9],
                                                      uint8_t permission_mask) {
  req_data[0] = MESHCORE_RUNTIME_REQ_TYPE_GET_TELEMETRY_DATA;
  req_data[1] = meshcore_runtime_telemetry_wire_encode(permission_mask);
  memset(&req_data[2], 0, 3U);
  meshcore_platform_bridge_rng_random(&req_data[5], 4U);
}

static void meshcore_runtime_request_execute_node_discover_path(
    const struct meshcore_runtime_request_node_discover_path *request) {
  static const uint16_t transport_codes[2] = {MESHCORE_SNR_TRANSPORT_CODE0,
                                              MESHCORE_SNR_TRANSPORT_CODE1};
  struct meshcore_identity recipient;
  struct meshcore_packet *packet;
  uint8_t secret[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t req_data[9];
  uint8_t data[13];
  uint32_t tag;

  if (request == NULL) {
    return;
  }
  if (meshcore_runtime_sync_local_identity() != 0) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_NODE_DISCOVER_PATH, -EAGAIN);
    return;
  }

  tag = meshcore_clock_rtc_get_current_time_unique(
      &meshcore_runtime_context_get()->rtc_clock_state);
  meshcore_runtime_request_prepare_req_data(req_data, MESHCORE_TELEM_PERM_BASE);
  memcpy(data, &tag, sizeof(tag));
  memcpy(&data[4], req_data, sizeof(req_data));

  meshcore_identity_init_from_pub_key(&recipient, request->public_key);
  meshcore_local_identity_calc_shared_secret(&meshcore_runtime_context_get()->mesh.self_id,
                                             secret, request->public_key);
  packet = meshcore_mesh_create_datagram(&meshcore_runtime_context_get()->mesh,
                                         PAYLOAD_TYPE_REQ, &recipient, secret,
                                         data, sizeof(data));
  memset(secret, 0, sizeof(secret));
  if (packet == NULL) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_NODE_DISCOVER_PATH, -ENOBUFS);
    return;
  }

  meshcore_runtime_pending_discovery_register(tag, request->public_key,
                                              request->record_snr);
  if (request->record_snr) {
    meshcore_mesh_send_flood_by_transport_codes(
        &meshcore_runtime_context_get()->mesh, packet, transport_codes, 0U,
        meshcore_runtime_local_path_hash_size_get());
  } else {
    meshcore_mesh_send_flood(&meshcore_runtime_context_get()->mesh, packet, 0U,
                             meshcore_runtime_local_path_hash_size_get());
  }
}

static void meshcore_runtime_request_execute_node_trace_path(
    const struct meshcore_runtime_request_node_trace_path *request) {
  meshcore_common_peer_path_t peer_path;
  struct meshcore_packet *packet;
  uint8_t trace_out_path[MESHCORE_MAX_PATH_LEN];
  uint8_t trace_path[MESHCORE_MAX_PATH_LEN];
  uint8_t peer_hash_size;
  uint8_t trace_hash_size;
  uint8_t flags = 0U;
  uint32_t tag;
  uint32_t auth_code;
  size_t trace_out_len;
  size_t trace_len = 0U;
  size_t max_trace_len = MESHCORE_PACKET_PAYLOAD_MAX_LEN - 9U;
  size_t hop_count;
  size_t hop;

  if (request == NULL ||
      meshcore_platform_bridge_peer_path_get_by_key(request->public_key, &peer_path) != 0 ||
      peer_path.out_path_len == 0U) {
    return;
  }

  peer_hash_size = meshcore_runtime_normalize_path_hash_size(peer_path.path_hash_size);
  if ((peer_path.out_path_len % peer_hash_size) != 0U) {
    return;
  }

  trace_hash_size = peer_hash_size;
  if (trace_hash_size == 3U) {
    trace_hash_size = 2U;
  }
  if (trace_hash_size == 2U) {
    flags = 1U;
  } else if (trace_hash_size != 1U) {
    return;
  }

  trace_out_len = peer_path.out_path_len;
  if (peer_hash_size != trace_hash_size) {
    hop_count = peer_path.out_path_len / peer_hash_size;
    trace_out_len = hop_count * trace_hash_size;
    for (hop = 0U; hop < hop_count; hop++) {
      memcpy(&trace_out_path[hop * trace_hash_size],
             &peer_path.out_path[hop * peer_hash_size], trace_hash_size);
    }
  } else if (trace_out_len > 0U) {
    memcpy(trace_out_path, peer_path.out_path, trace_out_len);
  }

  tag = meshcore_clock_rtc_get_current_time_unique(
      &meshcore_runtime_context_get()->rtc_clock_state);
  auth_code = meshcore_clock_millis_get() & 0x00FFFFFFUL;
  packet = meshcore_mesh_create_trace(&meshcore_runtime_context_get()->mesh, tag,
                                      auth_code, flags);
  if (packet == NULL) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_NODE_TRACE_PATH, -ENOBUFS);
    return;
  }

  if (trace_out_len > max_trace_len) {
    meshcore_dispatcher_release_packet(&meshcore_runtime_context_get()->mesh.dispatcher,
                                       packet);
    return;
  }
  if (trace_out_len > 0U) {
    memcpy(trace_path, trace_out_path, trace_out_len);
    trace_len = trace_out_len;
  }

  for (hop = trace_out_len; hop >= (size_t)(2U * trace_hash_size);
       hop -= trace_hash_size) {
    if (trace_len + trace_hash_size > max_trace_len) {
      meshcore_dispatcher_release_packet(&meshcore_runtime_context_get()->mesh.dispatcher,
                                         packet);
      return;
    }
    memcpy(&trace_path[trace_len], &trace_out_path[hop - (2U * trace_hash_size)],
           trace_hash_size);
    trace_len += trace_hash_size;
  }

  meshcore_runtime_pending_trace_register(tag, request->public_key);
  meshcore_mesh_send_direct(&meshcore_runtime_context_get()->mesh, packet, trace_path,
                            (uint8_t)trace_len, 0U);
}

static void meshcore_runtime_request_execute_node_telemetry(
    const struct meshcore_runtime_request_node_telemetry *request) {
  struct meshcore_identity recipient;
  struct meshcore_packet *packet;
  uint8_t secret[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t req_data[9];
  uint8_t data[13];
  uint32_t tag;

  if (request == NULL) {
    return;
  }
  if (meshcore_runtime_sync_local_identity() != 0) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_NODE_TELEMETRY, -EAGAIN);
    return;
  }

  tag = meshcore_clock_rtc_get_current_time_unique(
      &meshcore_runtime_context_get()->rtc_clock_state);
  meshcore_runtime_request_prepare_req_data(req_data, request->permission_mask);
  memcpy(data, &tag, sizeof(tag));
  memcpy(&data[4], req_data, sizeof(req_data));

  meshcore_identity_init_from_pub_key(&recipient, request->public_key);
  meshcore_local_identity_calc_shared_secret(&meshcore_runtime_context_get()->mesh.self_id,
                                             secret, request->public_key);
  packet = meshcore_mesh_create_datagram(&meshcore_runtime_context_get()->mesh,
                                         PAYLOAD_TYPE_REQ, &recipient, secret,
                                         data, sizeof(data));
  memset(secret, 0, sizeof(secret));
  if (packet == NULL) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_NODE_TELEMETRY, -ENOBUFS);
    return;
  }

  meshcore_runtime_pending_telemetry_register(tag, request->public_key,
                                              request->permission_mask);
  meshcore_mesh_send_flood(&meshcore_runtime_context_get()->mesh, packet, 0U,
                           meshcore_runtime_local_path_hash_size_get());
}

static void meshcore_runtime_request_execute_node_binary(
    const struct meshcore_runtime_request_node_binary *request) {
  struct meshcore_identity recipient;
  struct meshcore_packet *packet;
  meshcore_common_peer_path_t peer_path;
  uint8_t path_len = 0U;
  uint8_t secret[MESHCORE_PUBLIC_KEY_SIZE];
  uint8_t data[sizeof(uint32_t) + MESHCORE_MAX_SERVICE_REQUEST_PAYLOAD_LEN];
  uint32_t tag;

  if (request == NULL) {
    return;
  }
  if (meshcore_runtime_sync_local_identity() != 0) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_NODE_BINARY, -EAGAIN);
    return;
  }

  tag = meshcore_clock_rtc_get_current_time_unique(
      &meshcore_runtime_context_get()->rtc_clock_state);
  if (request->tag != 0U) {
    tag = request->tag;
  }
  memcpy(data, &tag, sizeof(tag));
  memcpy(&data[sizeof(tag)], request->payload, request->payload_len);

  meshcore_identity_init_from_pub_key(&recipient, request->public_key);
  meshcore_local_identity_calc_shared_secret(&meshcore_runtime_context_get()->mesh.self_id,
                                             secret, request->public_key);
  packet = meshcore_mesh_create_datagram(
      &meshcore_runtime_context_get()->mesh, PAYLOAD_TYPE_REQ, &recipient, secret, data,
      sizeof(tag) + request->payload_len);
  memset(secret, 0, sizeof(secret));
  if (packet == NULL) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_NODE_BINARY, -ENOBUFS);
    return;
  }

  meshcore_runtime_pending_binary_register(tag, request->public_key);
  if (meshcore_runtime_peer_path_get(request->public_key, &peer_path,
                                     &path_len)) {
    meshcore_mesh_send_direct(&meshcore_runtime_context_get()->mesh, packet,
                              peer_path.out_path, path_len, 0U);
  } else {
    meshcore_mesh_send_flood(&meshcore_runtime_context_get()->mesh, packet, 0U,
                             meshcore_runtime_local_path_hash_size_get());
  }
}

static void meshcore_runtime_request_execute_channel_data(
    const struct meshcore_runtime_request_channel_data *request) {
  struct meshcore_group_channel channel;
  struct meshcore_packet *packet;
  uint8_t channel_hash[MESHCORE_CHANNEL_HASH_BYTES];
  uint8_t data[3U + MESHCORE_MAX_CHANNEL_DATA_PAYLOAD_LEN];

  if (request == NULL) {
    return;
  }
  if (meshcore_platform_bridge_channel_secret_hash(request->secret, request->secret_len,
                                   channel_hash) != 0 ||
      meshcore_platform_bridge_channel_secret_match_exists(channel_hash[0], request->secret,
                                           request->secret_len) <= 0) {
    return;
  }

  memset(&channel, 0, sizeof(channel));
  memcpy(channel.hash, channel_hash, sizeof(channel.hash));
  memcpy(channel.secret, request->secret, request->secret_len);

  data[0] = (uint8_t)(request->data_type & 0xFFU);
  data[1] = (uint8_t)((request->data_type >> 8) & 0xFFU);
  data[2] = (uint8_t)request->payload_len;
  if (request->payload_len > 0U) {
    memcpy(&data[3], request->payload, request->payload_len);
  }

  packet = meshcore_mesh_create_group_datagram(
      &meshcore_runtime_context_get()->mesh, PAYLOAD_TYPE_GRP_DATA, &channel, data,
      3U + request->payload_len);
  if (packet == NULL) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_CHANNEL_DATA, -ENOBUFS);
    return;
  }

  if (request->path_len == MESHCORE_OUT_PATH_UNKNOWN) {
    meshcore_mesh_send_flood(&meshcore_runtime_context_get()->mesh, packet, 0U,
                             meshcore_runtime_local_path_hash_size_get());
  } else {
    meshcore_mesh_send_direct(&meshcore_runtime_context_get()->mesh, packet, request->path,
                              request->path_len, 0U);
  }
}

static void meshcore_runtime_request_execute_raw_data(
    const struct meshcore_runtime_request_raw_data *request) {
  struct meshcore_packet *packet;

  if (request == NULL) {
    return;
  }

  packet = meshcore_mesh_create_raw_data(&meshcore_runtime_context_get()->mesh,
                                         request->payload,
                                         request->payload_len);
  if (packet == NULL) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_RAW_DATA, -ENOBUFS);
    return;
  }

  meshcore_mesh_send_direct(&meshcore_runtime_context_get()->mesh, packet, request->path,
                            request->path_len, 0U);
}

static void meshcore_runtime_request_execute_control_data(
    const struct meshcore_runtime_request_control_data *request) {
  struct meshcore_packet *packet;

  if (request == NULL) {
    return;
  }

  packet = meshcore_mesh_create_control_data(&meshcore_runtime_context_get()->mesh,
                                             request->payload,
                                             request->payload_len);
  if (packet == NULL) {
    meshcore_runtime_request_log_transient_failure(
        MESHCORE_RUNTIME_REQUEST_CONTROL_DATA, -ENOBUFS);
    return;
  }

  meshcore_mesh_send_zero_hop(&meshcore_runtime_context_get()->mesh, packet, 0U);
}

static void meshcore_runtime_request_execute(
    const struct meshcore_runtime_request_slot *request) {
  if (request == NULL || !request->used) {
    return;
  }

  switch (request->type) {
    case MESHCORE_RUNTIME_REQUEST_NODE_ADVERT:
      meshcore_runtime_request_execute_node_advert(&request->data.node_advert);
      break;
    case MESHCORE_RUNTIME_REQUEST_NODE_PEER_ADVERT:
      meshcore_runtime_request_execute_node_peer_advert(
          &request->data.node_peer_advert);
      break;
    case MESHCORE_RUNTIME_REQUEST_MESSAGE_SEND_TO_NODE:
      meshcore_runtime_request_execute_message_send_to_node(
          &request->data.message_send_to_node);
      break;
    case MESHCORE_RUNTIME_REQUEST_MESSAGE_SEND_TO_CHANNEL:
      meshcore_runtime_request_execute_message_send_to_channel(
          &request->data.message_send_to_channel);
      break;
    case MESHCORE_RUNTIME_REQUEST_NODE_DISCOVER_PATH:
      meshcore_runtime_request_execute_node_discover_path(
          &request->data.node_discover_path);
      break;
    case MESHCORE_RUNTIME_REQUEST_NODE_TRACE_PATH:
      meshcore_runtime_request_execute_node_trace_path(
          &request->data.node_trace_path);
      break;
    case MESHCORE_RUNTIME_REQUEST_NODE_TELEMETRY:
      meshcore_runtime_request_execute_node_telemetry(
          &request->data.node_telemetry);
      break;
    case MESHCORE_RUNTIME_REQUEST_NODE_BINARY:
      meshcore_runtime_request_execute_node_binary(&request->data.node_binary);
      break;
    case MESHCORE_RUNTIME_REQUEST_CHANNEL_DATA:
      meshcore_runtime_request_execute_channel_data(&request->data.channel_data);
      break;
    case MESHCORE_RUNTIME_REQUEST_RAW_DATA:
      meshcore_runtime_request_execute_raw_data(&request->data.raw_data);
      break;
    case MESHCORE_RUNTIME_REQUEST_CONTROL_DATA:
      meshcore_runtime_request_execute_control_data(&request->data.control_data);
      break;
    default:
      break;
  }
}

static int meshcore_runtime_request_execute_now(
    const struct meshcore_runtime_request_slot *request) {
  int rc = meshcore_runtime_require_initialized();

  if (rc != 0) {
    return rc;
  }
  if (request == NULL || !request->used) {
    return -EINVAL;
  }
  if (meshcore_runtime_request_needs_packet(request->type) &&
      meshcore_packet_queue_manager_get_free_count(
          &meshcore_runtime_context_get()->packet_manager) <= 0) {
    return -ENOBUFS;
  }

  meshcore_runtime_request_execute(request);
  meshcore_runtime_timer_sync((uint32_t)meshcore_clock_millis_get());
  return 0;
}

int meshcore_node_advert_request(bool flood) {
  int rc = meshcore_runtime_require_initialized();
  union meshcore_runtime_request_data data;

  if (rc != 0) {
    return rc;
  }

  memset(&data, 0, sizeof(data));
  data.node_advert.flood = flood;
  return meshcore_runtime_request_add(MESHCORE_RUNTIME_REQUEST_NODE_ADVERT,
                                      &data);
}

int meshcore_node_peer_advert_request(const uint8_t *raw_advert,
                                      size_t raw_advert_len) {
  int rc = meshcore_runtime_require_initialized();
  union meshcore_runtime_request_data data;

  if (rc != 0) {
    return rc;
  }

  rc = meshcore_runtime_request_validate_node_peer_advert(raw_advert,
                                                          raw_advert_len);
  if (rc != 0) {
    return rc;
  }

  memset(&data, 0, sizeof(data));
  memcpy(data.node_peer_advert.raw_advert, raw_advert, raw_advert_len);
  data.node_peer_advert.raw_advert_len = raw_advert_len;
  return meshcore_runtime_request_add(
      MESHCORE_RUNTIME_REQUEST_NODE_PEER_ADVERT, &data);
}

int meshcore_message_send_to_node(const uint8_t *public_key, bool flood,
                                  uint8_t attempt, const uint8_t *payload,
                                  size_t payload_len) {
  int rc = meshcore_runtime_require_initialized();
  union meshcore_runtime_request_data data;

  if (rc != 0) {
    return rc;
  }

  rc = meshcore_runtime_request_validate_message_send_to_node(
      public_key, attempt, payload, payload_len);
  if (rc != 0) {
    return rc;
  }

  memset(&data, 0, sizeof(data));
  memcpy(data.message_send_to_node.public_key, public_key,
         sizeof(data.message_send_to_node.public_key));
  data.message_send_to_node.flood = flood;
  data.message_send_to_node.attempt = attempt;
  memcpy(data.message_send_to_node.payload, payload, payload_len);
  data.message_send_to_node.payload_len = payload_len;
  return meshcore_runtime_request_add(
      MESHCORE_RUNTIME_REQUEST_MESSAGE_SEND_TO_NODE, &data);
}

int meshcore_message_send_to_channel(const uint8_t *secret, size_t secret_len,
                                     const uint8_t *payload,
                                     size_t payload_len) {
  int rc = meshcore_runtime_require_initialized();
  union meshcore_runtime_request_data data;

  if (rc != 0) {
    return rc;
  }

  rc = meshcore_runtime_request_validate_message_send_to_channel(
      secret, secret_len, payload, payload_len);
  if (rc != 0) {
    return rc;
  }

  memset(&data, 0, sizeof(data));
  memcpy(data.message_send_to_channel.secret, secret, secret_len);
  data.message_send_to_channel.secret_len = secret_len;
  memcpy(data.message_send_to_channel.payload, payload, payload_len);
  data.message_send_to_channel.payload_len = payload_len;
  return meshcore_runtime_request_add(
      MESHCORE_RUNTIME_REQUEST_MESSAGE_SEND_TO_CHANNEL, &data);
}

int meshcore_channel_data_send(const uint8_t *secret, size_t secret_len,
                               const uint8_t *path, uint8_t path_len,
                               uint16_t data_type, const uint8_t *payload,
                               size_t payload_len) {
  int rc = meshcore_runtime_require_initialized();
  union meshcore_runtime_request_data data;
  uint8_t path_bytes = 0U;
  uint8_t path_hash_size = 0U;

  if (rc != 0) {
    return rc;
  }

  rc = meshcore_runtime_request_validate_channel_data(
      secret, secret_len, path, path_len, payload, payload_len);
  if (rc != 0) {
    return rc;
  }

  memset(&data, 0, sizeof(data));
  memcpy(data.channel_data.secret, secret, secret_len);
  data.channel_data.secret_len = secret_len;
  data.channel_data.path_len = path_len;
  data.channel_data.data_type = data_type;
  if (path_len != MESHCORE_OUT_PATH_UNKNOWN &&
      meshcore_runtime_path_len_decode(path_len, &path_bytes,
                                       &path_hash_size) &&
      path_bytes > 0U) {
    memcpy(data.channel_data.path, path, path_bytes);
  }
  if (payload_len > 0U) {
    memcpy(data.channel_data.payload, payload, payload_len);
  }
  data.channel_data.payload_len = payload_len;
  (void)path_hash_size;
  return meshcore_runtime_request_add(MESHCORE_RUNTIME_REQUEST_CHANNEL_DATA,
                                      &data);
}

int meshcore_node_discover_path_request(const uint8_t *public_key,
                                        bool record_snr) {
  int rc = meshcore_runtime_require_initialized();
  union meshcore_runtime_request_data data;

  if (rc != 0) {
    return rc;
  }

  rc = meshcore_runtime_request_validate_public_key(public_key);
  if (rc != 0) {
    return rc;
  }

  memset(&data, 0, sizeof(data));
  memcpy(data.node_discover_path.public_key, public_key,
         sizeof(data.node_discover_path.public_key));
  data.node_discover_path.record_snr = record_snr;
  return meshcore_runtime_request_add(
      MESHCORE_RUNTIME_REQUEST_NODE_DISCOVER_PATH, &data);
}

int meshcore_node_trace_path_request(const uint8_t *public_key) {
  int rc = meshcore_runtime_require_initialized();
  union meshcore_runtime_request_data data;

  if (rc != 0) {
    return rc;
  }

  rc = meshcore_runtime_request_validate_public_key(public_key);
  if (rc != 0) {
    return rc;
  }

  memset(&data, 0, sizeof(data));
  memcpy(data.node_trace_path.public_key, public_key,
         sizeof(data.node_trace_path.public_key));
  return meshcore_runtime_request_add(MESHCORE_RUNTIME_REQUEST_NODE_TRACE_PATH,
                                      &data);
}

int meshcore_node_telemetry_request(const uint8_t *public_key,
                                    uint8_t permission_mask) {
  int rc = meshcore_runtime_require_initialized();
  union meshcore_runtime_request_data data;

  if (rc != 0) {
    return rc;
  }

  rc = meshcore_runtime_request_validate_node_telemetry(public_key,
                                                        permission_mask);
  if (rc != 0) {
    return rc;
  }

  memset(&data, 0, sizeof(data));
  memcpy(data.node_telemetry.public_key, public_key,
         sizeof(data.node_telemetry.public_key));
  data.node_telemetry.permission_mask = permission_mask;
  return meshcore_runtime_request_add(MESHCORE_RUNTIME_REQUEST_NODE_TELEMETRY,
                                      &data);
}

int meshcore_node_binary_request(const uint8_t *public_key,
                                 const uint8_t *payload,
                                 size_t payload_len) {
  return meshcore_node_binary_request_with_tag(public_key, payload, payload_len,
                                               0U);
}

int meshcore_node_binary_request_with_tag(const uint8_t *public_key,
                                          const uint8_t *payload,
                                          size_t payload_len,
                                          uint32_t tag) {
  int rc = meshcore_runtime_require_initialized();
  union meshcore_runtime_request_data data;

  if (rc != 0) {
    return rc;
  }

  rc = meshcore_runtime_request_validate_node_binary(public_key, payload,
                                                     payload_len);
  if (rc != 0) {
    return rc;
  }

  memset(&data, 0, sizeof(data));
  memcpy(data.node_binary.public_key, public_key,
         sizeof(data.node_binary.public_key));
  memcpy(data.node_binary.payload, payload, payload_len);
  data.node_binary.payload_len = payload_len;
  data.node_binary.tag = tag;
  return meshcore_runtime_request_add(MESHCORE_RUNTIME_REQUEST_NODE_BINARY,
                                      &data);
}

int meshcore_raw_data_send(const uint8_t *path, uint8_t path_len,
                           const uint8_t *payload, size_t payload_len) {
  int rc = meshcore_runtime_require_initialized();
  union meshcore_runtime_request_data data;
  uint8_t path_bytes = 0U;
  uint8_t path_hash_size = 0U;

  if (rc != 0) {
    return rc;
  }

  rc = meshcore_runtime_request_validate_raw_data(path, path_len, payload,
                                                  payload_len);
  if (rc != 0) {
    return rc;
  }

  memset(&data, 0, sizeof(data));
  data.raw_data.path_len = path_len;
  if (meshcore_runtime_path_len_decode(path_len, &path_bytes,
                                       &path_hash_size) &&
      path_bytes > 0U) {
    memcpy(data.raw_data.path, path, path_bytes);
  }
  memcpy(data.raw_data.payload, payload, payload_len);
  data.raw_data.payload_len = payload_len;
  (void)path_hash_size;
  return meshcore_runtime_request_add(MESHCORE_RUNTIME_REQUEST_RAW_DATA,
                                      &data);
}

int meshcore_control_data_send(const uint8_t *payload, size_t payload_len) {
  int rc = meshcore_runtime_require_initialized();
  union meshcore_runtime_request_data data;

  if (rc != 0) {
    return rc;
  }

  rc = meshcore_runtime_request_validate_control_data(payload, payload_len);
  if (rc != 0) {
    return rc;
  }

  memset(&data, 0, sizeof(data));
  memcpy(data.control_data.payload, payload, payload_len);
  data.control_data.payload_len = payload_len;
  return meshcore_runtime_request_add(MESHCORE_RUNTIME_REQUEST_CONTROL_DATA,
                                      &data);
}

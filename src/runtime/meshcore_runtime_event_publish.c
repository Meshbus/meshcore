// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_runtime_internal.h"

#include <stdio.h>
#include <string.h>

#include "meshcore_platform_bridge.h"

static meshcore_common_node_role_t
meshcore_runtime_role_from_advert_type(uint8_t type);

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

void meshcore_runtime_text_message_publish(
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
    message.rx_snr = ((float)packet->snr_q4) / 4.0f;
  }

  (void)meshcore_platform_bridge_message_handler(&message);
}

uint8_t meshcore_runtime_packet_path_copy(
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
  return path_bytes;
}

void meshcore_runtime_channel_text_publish(
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
  message.target_len = MESHCORE_MESSAGE_TARGET_PREFIX_BYTES;
  memcpy(message.target, channel->secret,
         MIN(sizeof(message.target), sizeof(channel->secret)));
  message.payload_len =
      (uint16_t)MIN(payload_len, (size_t)sizeof(message.payload));
  memcpy(message.payload, payload, message.payload_len);
  message.sender_timestamp = sender_timestamp;
  message.has_rx_snr = packet != NULL;
  if (packet != NULL) {
    message.rx_snr = ((float)packet->snr_q4) / 4.0f;
  }

  (void)meshcore_platform_bridge_message_handler(&message);
}

void meshcore_runtime_channel_data_publish(
    const struct meshcore_group_channel *channel,
    const struct meshcore_packet *packet, const uint8_t *data, size_t len) {
  meshcore_common_channel_data_event_t event = {0};
  uint16_t data_type;
  uint8_t data_len;
  size_t available_len;

  if (channel == NULL || packet == NULL || data == NULL || len < 3U) {
    return;
  }

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

void meshcore_runtime_control_data_publish(struct meshcore_packet *packet) {
  meshcore_common_control_data_event_t event = {0};

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
  (void)meshcore_platform_bridge_control_data_handler(&event);
}

void meshcore_runtime_node_discover_publish(struct meshcore_packet *packet) {
  meshcore_common_node_discover_event_t event = {0};
  uint8_t advert_type;
  size_t key_len;

  if (packet == NULL || packet->payload_len < 6U ||
      (packet->payload[0] & 0xF0U) != MESHCORE_RUNTIME_CTL_TYPE_NODE_DISCOVER_RESP) {
    return;
  }

  advert_type = packet->payload[0] & 0x0FU;
  event.role = meshcore_runtime_role_from_advert_type(advert_type);
  if (event.role == MESHCORE_COMMON_NODE_ROLE_NONE) {
    return;
  }

  key_len = (size_t)packet->payload_len - 6U;
  if (key_len < MESHCORE_NODE_DISCOVER_PUBLIC_KEY_PREFIX_BYTES) {
    return;
  }
  if (key_len > sizeof(event.public_key)) {
    key_len = sizeof(event.public_key);
  }

  event.path_len =
      meshcore_runtime_packet_path_copy(packet, event.path, sizeof(event.path));
  event.public_key_len = (uint8_t)key_len;
  event.uplink_snr = (int8_t)packet->payload[1];
  memcpy(&event.tag, &packet->payload[2], sizeof(event.tag));
  memcpy(event.public_key, &packet->payload[6], key_len);
  event.downlink_snr = packet->snr_q4;

  (void)meshcore_platform_bridge_node_discover_handler(&event);
}

void meshcore_runtime_raw_data_publish(struct meshcore_packet *packet) {
  meshcore_common_raw_data_event_t event = {0};

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

void meshcore_runtime_binary_request_publish(
    const meshcore_common_peer_identity_t *peer,
    const struct meshcore_packet *packet, uint32_t tag, const uint8_t *payload,
    size_t payload_len) {
  meshcore_common_binary_request_event_t event = {0};

  if (peer == NULL || packet == NULL || payload == NULL ||
      payload_len == 0U || payload_len > sizeof(event.payload) || tag == 0U) {
    return;
  }

  event.route = meshcore_runtime_message_route_from_packet(packet);
  memcpy(event.key_prefix, peer->public_key, sizeof(event.key_prefix));
  event.tag = tag;
  memcpy(event.public_key, peer->public_key, sizeof(event.public_key));
  event.path_len = (uint8_t)packet->path_len;
  (void)meshcore_runtime_packet_path_copy(packet, event.path,
                                          sizeof(event.path));
  event.payload_len = (uint8_t)payload_len;
  memcpy(event.payload, payload, payload_len);
  event.has_rx_snr = true;
  event.rx_snr_q4 = packet->snr_q4;

  (void)meshcore_platform_bridge_binary_request_handler(&event);
}

void meshcore_runtime_peer_path_publish(
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

static meshcore_common_node_role_t
meshcore_runtime_role_from_advert_type(uint8_t type) {
  switch (type & 0x0FU) {
    case ADV_TYPE_CHAT:
      return MESHCORE_COMMON_NODE_ROLE_CHAT;
    case ADV_TYPE_REPEATER:
      return MESHCORE_COMMON_NODE_ROLE_REPEATER;
    case ADV_TYPE_ROOM:
      return MESHCORE_COMMON_NODE_ROLE_ROOM;
    case ADV_TYPE_SENSOR:
      return MESHCORE_COMMON_NODE_ROLE_SENSOR;
    default:
      return MESHCORE_COMMON_NODE_ROLE_NONE;
  }
}

static uint8_t meshcore_runtime_raw_advert_copy(
    const struct meshcore_packet *packet, uint8_t *dest, size_t dest_len) {
  struct meshcore_packet normalized;
  uint8_t raw_len;

  if (packet == NULL || dest == NULL || dest_len == 0U) {
    return 0U;
  }

  normalized = *packet;
  normalized.header &= (uint8_t)~PH_ROUTE_MASK;
  normalized.header |= ROUTE_TYPE_FLOOD;
  raw_len = meshcore_packet_write_to(&normalized, dest);
  if (raw_len == 0U || raw_len > dest_len) {
    return 0U;
  }

  return raw_len;
}

bool meshcore_runtime_advert_publish(
    const struct meshcore_packet *packet,
    const struct meshcore_identity *identity, uint32_t timestamp,
    const struct meshcore_advert_data_parser *parser) {
  meshcore_common_advert_event_t event = {0};
  uint8_t path_byte_len;
  uint8_t path_hash_size;
  const char *name;

  if (packet == NULL || identity == NULL || parser == NULL ||
      !meshcore_advert_data_parser_is_valid(parser)) {
    return false;
  }

  event.role = meshcore_runtime_role_from_advert_type(
      meshcore_advert_data_parser_get_type(parser));
  if (event.role == MESHCORE_COMMON_NODE_ROLE_NONE) {
    return false;
  }
  memcpy(event.public_key, identity->pub_key, sizeof(event.public_key));
  event.is_new = true;
  event.advert_timestamp = timestamp;

  if (meshcore_advert_data_parser_has_name(parser)) {
    name = meshcore_advert_data_parser_get_name(parser);
    (void)snprintf(event.name, sizeof(event.name), "%s", name);
  }
  if (meshcore_advert_data_parser_has_lat_lon(parser)) {
    event.has_position = true;
    event.latitude = meshcore_advert_data_parser_get_int_lat(parser);
    event.longitude = meshcore_advert_data_parser_get_int_lon(parser);
  }
  if (meshcore_runtime_path_len_decode((uint8_t)packet->path_len,
                                       &path_byte_len, &path_hash_size)) {
    event.has_out_path = true;
    event.out_path_len =
        (uint8_t)MIN((size_t)path_byte_len, sizeof(event.out_path));
    event.path_hash_size = path_hash_size;
    if (event.out_path_len > 0U) {
      memcpy(event.out_path, packet->path, event.out_path_len);
    }
  }

  event.raw_advert_len = meshcore_runtime_raw_advert_copy(
      packet, event.raw_advert, sizeof(event.raw_advert));

  return meshcore_platform_bridge_advert_handler(&event) == 0;
}

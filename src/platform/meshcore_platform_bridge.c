// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_platform_bridge.h"

#include <string.h>

#include "meshcore_group_channel.h"
#include "meshcore_identity.h"
#include "meshcore_packet.h"

static meshcore_common_message_route_t meshcore_platform_bridge_route_from_packet(
    const struct meshcore_packet *packet) {
  if (meshcore_packet_is_route_flood(packet)) {
    return MESHCORE_COMMON_MESSAGE_ROUTE_FLOOD;
  }
  if (meshcore_packet_is_route_direct(packet)) {
    return MESHCORE_COMMON_MESSAGE_ROUTE_DIRECT;
  }
  return MESHCORE_COMMON_MESSAGE_ROUTE_UNSPECIFIED;
}

static void meshcore_platform_bridge_packet_view_init(
    meshcore_common_packet_view_t *view, const struct meshcore_packet *packet,
    uint8_t *raw, size_t raw_capacity) {
  int raw_len;
  uint8_t written_len;

  if (view == NULL) {
    return;
  }

  memset(view, 0, sizeof(*view));
  if (packet == NULL) {
    return;
  }

  view->route = meshcore_platform_bridge_route_from_packet(packet);
  view->payload_type = meshcore_packet_get_payload_type(packet);
  view->payload_ver = meshcore_packet_get_payload_ver(packet);
  view->path_len = (uint8_t)packet->path_len;
  view->path_hash_size = meshcore_packet_get_path_hash_size(packet);
  view->path_hash_count = meshcore_packet_get_path_hash_count(packet);
  view->path_byte_len = meshcore_packet_get_path_byte_len(packet);
  view->path = packet->path;
  view->payload_len = packet->payload_len;
  view->payload = packet->payload;
  view->has_rx_snr = true;
  view->rx_snr_q4 = packet->snr_q4;
  view->has_transport_snr = meshcore_packet_has_snr_flag(packet);
  view->has_transport_codes = meshcore_packet_has_transport_codes(packet);

  raw_len = meshcore_packet_get_raw_length(packet);
  if (raw_len <= 0) {
    return;
  }

  view->raw_len = (uint16_t)raw_len;
  if (raw == NULL || (size_t)raw_len > raw_capacity) {
    return;
  }

  written_len = meshcore_packet_write_to(packet, raw);
  if (written_len > 0U) {
    view->raw = raw;
    view->raw_len = written_len;
  }
}

static void meshcore_platform_bridge_identity_view_init(
    meshcore_common_identity_view_t *view,
    const struct meshcore_identity *identity) {
  if (view == NULL) {
    return;
  }

  memset(view, 0, sizeof(*view));
  if (identity != NULL) {
    memcpy(view->public_key, identity->pub_key, sizeof(view->public_key));
  }
}

static void meshcore_platform_bridge_channel_view_init(
    meshcore_common_channel_view_t *view,
    const struct meshcore_group_channel *channel) {
  size_t secret_len;

  if (view == NULL) {
    return;
  }

  memset(view, 0, sizeof(*view));
  if (channel != NULL) {
    secret_len = (sizeof(view->secret) < sizeof(channel->secret))
                     ? sizeof(view->secret)
                     : sizeof(channel->secret);
    memcpy(view->hash, channel->hash, sizeof(view->hash));
    memcpy(view->secret, channel->secret, secret_len);
  }
}

static void meshcore_platform_bridge_group_channel_init(
    struct meshcore_group_channel *channel,
    const meshcore_common_channel_view_t *view) {
  size_t secret_len;

  if (channel == NULL) {
    return;
  }

  memset(channel, 0, sizeof(*channel));
  if (view != NULL) {
    secret_len = (sizeof(channel->secret) < sizeof(view->secret))
                     ? sizeof(channel->secret)
                     : sizeof(view->secret);
    memcpy(channel->hash, view->hash, sizeof(channel->hash));
    memcpy(channel->secret, view->secret, secret_len);
  }
}

void meshcore_platform_bridge_radio_begin(void) {
  meshcore_platform_radio_begin();
}

int meshcore_platform_bridge_radio_packet_send(const uint8_t *data,
                                                size_t len) {
  return meshcore_platform_radio_packet_send(data, len);
}

uint32_t meshcore_platform_bridge_radio_airtime(size_t len) {
  return meshcore_platform_radio_airtime(len);
}

float meshcore_platform_bridge_radio_packet_score(int8_t snr_q4, size_t len) {
  return meshcore_platform_radio_packet_score(snr_q4, len);
}

bool meshcore_platform_bridge_radio_in_rx_mode_get(void) {
  return meshcore_platform_radio_in_rx_mode_get();
}

bool meshcore_platform_bridge_radio_receiving_get(void) {
  return meshcore_platform_radio_receiving_get();
}

void meshcore_platform_bridge_radio_noise_floor_calibrate(int threshold) {
  meshcore_platform_radio_noise_floor_calibrate(threshold);
}

void meshcore_platform_bridge_radio_agc_reset(void) {
  meshcore_platform_radio_agc_reset();
}

void meshcore_platform_bridge_rng_random(uint8_t *dest, size_t size) {
  meshcore_platform_rng_random(dest, size);
}

unsigned long meshcore_platform_bridge_millis_get(void) {
  return meshcore_platform_millis_get();
}

uint32_t meshcore_platform_bridge_rtc_get_current_time(void) {
  return meshcore_platform_rtc_get_current_time();
}

int meshcore_platform_bridge_telemetry_node_get(
    const meshcore_platform_request_source_t *requester,
    uint8_t permission_mask, meshcore_platform_telemetry_payload_t *out) {
  return meshcore_platform_telemetry_node_get(requester, permission_mask, out);
}

bool meshcore_platform_bridge_crypto_sha256(uint8_t *hash, size_t hash_len,
                                      const uint8_t *msg, int msg_len) {
  return meshcore_platform_crypto_sha256(hash, hash_len, msg, msg_len);
}

bool meshcore_platform_bridge_crypto_sha256_two_fragments(
    uint8_t *hash, size_t hash_len, const uint8_t *frag1, int frag1_len,
    const uint8_t *frag2, int frag2_len) {
  return meshcore_platform_crypto_sha256_two_fragments(
      hash, hash_len, frag1, frag1_len, frag2, frag2_len);
}

bool meshcore_platform_bridge_crypto_aes128_encrypt_block(const uint8_t *key,
                                                    uint8_t *dest,
                                                    const uint8_t *src) {
  return meshcore_platform_crypto_aes128_encrypt_block(key, dest, src);
}

bool meshcore_platform_bridge_crypto_aes128_decrypt_block(const uint8_t *key,
                                                    uint8_t *dest,
                                                    const uint8_t *src) {
  return meshcore_platform_crypto_aes128_decrypt_block(key, dest, src);
}

bool meshcore_platform_bridge_crypto_hmac_sha256(uint8_t *mac, size_t mac_len,
                                           const uint8_t *key, size_t key_len,
                                           const uint8_t *msg,
                                           size_t msg_len) {
  return meshcore_platform_crypto_hmac_sha256(mac, mac_len, key, key_len, msg,
                                              msg_len);
}

int meshcore_platform_bridge_node_identity_get(
    meshcore_common_node_identity_t *out) {
  return meshcore_platform_node_identity_get(out);
}

int meshcore_platform_bridge_node_config_last_modify_get(
    uint32_t *out_timestamp) {
  return meshcore_platform_node_config_last_modify_get(out_timestamp);
}

int meshcore_platform_bridge_node_runtime_policy_get(
    meshcore_common_node_runtime_policy_t *out) {
  return meshcore_platform_node_runtime_policy_get(out);
}

int meshcore_platform_bridge_node_advert_profile_get(
    meshcore_common_node_advert_profile_t *out) {
  return meshcore_platform_node_advert_profile_get(out);
}

int meshcore_platform_bridge_peer_path_get_by_key(
    const uint8_t *public_key, meshcore_common_peer_path_t *out) {
  return meshcore_platform_peer_path_get_by_key(public_key, out);
}

int meshcore_platform_bridge_peer_seen_update(const uint8_t *public_key,
                                           bool has_snr, int8_t snr_q4) {
  return meshcore_platform_peer_seen_update(public_key, has_snr, snr_q4);
}

int meshcore_platform_bridge_channel_secret_match_exists(uint8_t channel_hash,
                                                      const uint8_t *secret,
                                                      size_t secret_len) {
  return meshcore_platform_channel_secret_match_exists(channel_hash, secret,
                                                       secret_len);
}

int meshcore_platform_bridge_channel_secret_hash(const uint8_t *secret,
                                              size_t secret_len,
                                              uint8_t *out_hash) {
  return meshcore_platform_channel_secret_hash(secret, secret_len, out_hash);
}

void meshcore_platform_bridge_request_error(uint8_t request_type, int err_code) {
  meshcore_platform_runtime_request_error(request_type, err_code);
}

uint8_t meshcore_platform_bridge_mesh_extra_ack_transmit_count_get(void) {
  return meshcore_platform_mesh_extra_ack_transmit_count_get();
}

uint32_t meshcore_platform_bridge_mesh_retransmit_delay_get(
    const struct meshcore_packet *packet) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    return meshcore_platform_mesh_retransmit_delay_get(NULL);
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  return meshcore_platform_mesh_retransmit_delay_get(&view);
}

int meshcore_platform_bridge_message_handler(
    const meshcore_common_message_t *message) {
  return meshcore_platform_event_message(message);
}

int meshcore_platform_bridge_message_ack_handler(const uint8_t *target,
                                              uint8_t attempt) {
  return meshcore_platform_event_message_ack(target, attempt);
}

int meshcore_platform_bridge_advert_handler(
    const meshcore_common_advert_event_t *advert) {
  return meshcore_platform_event_advert(advert);
}

int meshcore_platform_bridge_peer_path_publish(
    const meshcore_common_peer_path_event_t *peer_path, bool is_discover) {
  return meshcore_platform_event_peer_path_publish(peer_path, is_discover);
}

int meshcore_platform_bridge_peer_path_handler(
    const meshcore_common_peer_path_event_t *peer_path) {
  return meshcore_platform_event_peer_path(peer_path);
}

int meshcore_platform_bridge_trace_path_handler(
    const uint8_t *key_prefix, uint8_t state, uint32_t tag, const int8_t *out_path_snr,
    uint8_t out_count, const int8_t *return_path_snr, uint8_t return_count,
    bool has_response_snr, int8_t response_snr, uint32_t timestamp) {
  return meshcore_platform_event_trace_path(
      key_prefix, state, tag, out_path_snr, out_count, return_path_snr,
      return_count, has_response_snr, response_snr, timestamp);
}

int meshcore_platform_bridge_telemetry_handler(const uint8_t *key_prefix,
                                            uint32_t timestamp,
                                            uint32_t tag,
                                            const uint8_t *payload,
                                            size_t payload_len) {
  return meshcore_platform_event_telemetry(key_prefix, timestamp, tag, payload,
                                           payload_len);
}

int meshcore_platform_bridge_binary_response_handler(const uint8_t *key_prefix,
                                                  uint32_t timestamp,
                                                  uint32_t tag,
                                                  const uint8_t *payload,
                                                  size_t payload_len) {
  return meshcore_platform_event_binary_response(key_prefix, timestamp, tag,
                                                 payload, payload_len);
}

int meshcore_platform_bridge_channel_data_handler(
    const meshcore_common_channel_data_event_t *event) {
  return meshcore_platform_event_channel_data(event);
}

int meshcore_platform_bridge_raw_data_handler(
    const meshcore_common_raw_data_event_t *event) {
  return meshcore_platform_event_raw_data(event);
}

int meshcore_platform_bridge_control_data_handler(
    const meshcore_common_control_data_event_t *event) {
  return meshcore_platform_event_control_data(event);
}

void meshcore_platform_bridge_dispatcher_log_rx_raw(float snr, float rssi,
                                                 const uint8_t raw[],
                                                 int len) {
  meshcore_platform_dispatcher_log_rx_raw(snr, rssi, raw, len);
}

void meshcore_platform_bridge_dispatcher_log_rx(
    const struct meshcore_packet *packet, int len, float score) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    meshcore_platform_dispatcher_log_rx(NULL, len, score);
    return;
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  meshcore_platform_dispatcher_log_rx(&view, len, score);
}

void meshcore_platform_bridge_dispatcher_log_tx(
    const struct meshcore_packet *packet, int len) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    meshcore_platform_dispatcher_log_tx(NULL, len);
    return;
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  meshcore_platform_dispatcher_log_tx(&view, len);
}

void meshcore_platform_bridge_dispatcher_log_tx_fail(
    const struct meshcore_packet *packet, int len) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    meshcore_platform_dispatcher_log_tx_fail(NULL, len);
    return;
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  meshcore_platform_dispatcher_log_tx_fail(&view, len);
}

float meshcore_platform_bridge_dispatcher_airtime_budget_factor_get(void) {
  return meshcore_platform_dispatcher_airtime_budget_factor_get();
}

int meshcore_platform_bridge_dispatcher_rx_delay_calc(float score,
                                                   uint32_t air_time) {
  return meshcore_platform_dispatcher_rx_delay_calc(score, air_time);
}

uint32_t meshcore_platform_bridge_dispatcher_cad_fail_max_duration_get(void) {
  return meshcore_platform_dispatcher_cad_fail_max_duration_get();
}

int meshcore_platform_bridge_dispatcher_interference_threshold_get(void) {
  return meshcore_platform_dispatcher_interference_threshold_get();
}

int meshcore_platform_bridge_dispatcher_agc_reset_interval_get(void) {
  return meshcore_platform_dispatcher_agc_reset_interval_get();
}

unsigned long meshcore_platform_bridge_dispatcher_duty_cycle_window_ms_get(void) {
  return meshcore_platform_dispatcher_duty_cycle_window_ms_get();
}

uint32_t meshcore_platform_bridge_mesh_cad_fail_retry_delay_get(void) {
  return meshcore_platform_mesh_cad_fail_retry_delay_get();
}

bool meshcore_platform_bridge_mesh_filter_recv_flood_packet(
    const struct meshcore_packet *packet) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    return meshcore_platform_mesh_filter_recv_flood_packet(NULL);
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  return meshcore_platform_mesh_filter_recv_flood_packet(&view);
}

bool meshcore_platform_bridge_mesh_allow_packet_forward(
    const struct meshcore_packet *packet) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    return meshcore_platform_mesh_allow_packet_forward(NULL);
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  return meshcore_platform_mesh_allow_packet_forward(&view);
}

uint32_t meshcore_platform_bridge_mesh_direct_retransmit_delay_get(
    const struct meshcore_packet *packet) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    return meshcore_platform_mesh_direct_retransmit_delay_get(NULL);
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  return meshcore_platform_mesh_direct_retransmit_delay_get(&view);
}

int meshcore_platform_bridge_mesh_next_peer_shared_secret_by_hash(
    const uint8_t *hash, size_t start_slot, size_t *slot_id,
    uint8_t *dest_secret, meshcore_common_peer_identity_t *peer_identity) {
  return meshcore_platform_peer_next_shared_secret_by_hash(
      hash, start_slot, slot_id, dest_secret, peer_identity);
}

int meshcore_platform_bridge_mesh_search_channels_by_hash(
    const uint8_t *hash, struct meshcore_group_channel *channels,
    int max_matches) {
  meshcore_common_channel_view_t channel_views[MESHCORE_CHANNEL_SEARCH_MAX_MATCHES];
  int limit = max_matches;
  int rc;
  int i;

  if (channels == NULL || max_matches <= 0) {
    return meshcore_platform_channel_search_by_hash(hash, NULL, max_matches);
  }
  if (limit > (int)MESHCORE_CHANNEL_SEARCH_MAX_MATCHES) {
    limit = (int)MESHCORE_CHANNEL_SEARCH_MAX_MATCHES;
  }

  memset(channel_views, 0, sizeof(channel_views));
  rc = meshcore_platform_channel_search_by_hash(hash, channel_views, limit);
  if (rc <= 0) {
    return rc;
  }
  if (rc > limit) {
    rc = limit;
  }

  for (i = 0; i < rc; i++) {
    meshcore_platform_bridge_group_channel_init(&channels[i], &channel_views[i]);
  }
  return rc;
}

void meshcore_platform_bridge_mesh_on_peer_data_recv(
    struct meshcore_packet *packet, uint8_t type,
    const meshcore_common_peer_identity_t *sender, const uint8_t *secret,
    uint8_t *data, size_t len) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    meshcore_platform_mesh_on_peer_data_recv(NULL, type, sender, secret, data,
                                             len);
    return;
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  meshcore_platform_mesh_on_peer_data_recv(&view, type, sender, secret, data,
                                           len);
}

void meshcore_platform_bridge_mesh_on_trace_recv(
    struct meshcore_packet *packet, uint32_t tag, uint32_t auth_code,
    uint8_t flags, const uint8_t *path_snrs, const uint8_t *path_hashes,
    uint8_t path_len) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    meshcore_platform_mesh_on_trace_recv(NULL, tag, auth_code, flags,
                                         path_snrs, path_hashes, path_len);
    return;
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  meshcore_platform_mesh_on_trace_recv(&view, tag, auth_code, flags, path_snrs,
                                       path_hashes, path_len);
}

bool meshcore_platform_bridge_mesh_on_peer_path_recv(
    struct meshcore_packet *packet,
    const meshcore_common_peer_identity_t *sender, const uint8_t *secret,
    uint8_t *path, uint8_t path_len, uint8_t extra_type, uint8_t *extra,
    uint8_t extra_len) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    return meshcore_platform_mesh_on_peer_path_recv(
        NULL, sender, secret, path, path_len, extra_type, extra, extra_len);
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  return meshcore_platform_mesh_on_peer_path_recv(
      &view, sender, secret, path, path_len, extra_type, extra, extra_len);
}

void meshcore_platform_bridge_mesh_on_advert_recv(
    struct meshcore_packet *packet, const struct meshcore_identity *identity,
    uint32_t timestamp, const uint8_t *app_data, size_t app_data_len) {
  meshcore_common_packet_view_t packet_view;
  meshcore_common_identity_view_t identity_view;
  uint8_t raw[MESHCORE_MAX_TRANS_UNIT_LEN];
  const meshcore_common_packet_view_t *packet_arg = NULL;
  const meshcore_common_identity_view_t *identity_arg = NULL;

  if (packet != NULL) {
    meshcore_platform_bridge_packet_view_init(&packet_view, packet, raw,
                                           sizeof(raw));
    packet_arg = &packet_view;
  }
  if (identity != NULL) {
    meshcore_platform_bridge_identity_view_init(&identity_view, identity);
    identity_arg = &identity_view;
  }

  meshcore_platform_mesh_on_advert_recv(packet_arg, identity_arg, timestamp,
                                        app_data, app_data_len);
}

void meshcore_platform_bridge_mesh_on_anon_data_recv(
    struct meshcore_packet *packet, const uint8_t *secret,
    const struct meshcore_identity *sender, uint8_t *data, size_t len) {
  meshcore_common_packet_view_t packet_view;
  meshcore_common_identity_view_t sender_view;
  const meshcore_common_packet_view_t *packet_arg = NULL;
  const meshcore_common_identity_view_t *sender_arg = NULL;

  if (packet != NULL) {
    meshcore_platform_bridge_packet_view_init(&packet_view, packet, NULL, 0U);
    packet_arg = &packet_view;
  }
  if (sender != NULL) {
    meshcore_platform_bridge_identity_view_init(&sender_view, sender);
    sender_arg = &sender_view;
  }

  meshcore_platform_mesh_on_anon_data_recv(packet_arg, secret, sender_arg, data,
                                           len);
}

void meshcore_platform_bridge_mesh_on_path_recv(struct meshcore_packet *packet,
                                             struct meshcore_identity *sender,
                                             uint8_t *path, uint8_t path_len,
                                             uint8_t extra_type,
                                             uint8_t *extra,
                                             uint8_t extra_len) {
  meshcore_common_packet_view_t packet_view;
  meshcore_common_identity_view_t sender_view;
  const meshcore_common_packet_view_t *packet_arg = NULL;
  const meshcore_common_identity_view_t *sender_arg = NULL;

  if (packet != NULL) {
    meshcore_platform_bridge_packet_view_init(&packet_view, packet, NULL, 0U);
    packet_arg = &packet_view;
  }
  if (sender != NULL) {
    meshcore_platform_bridge_identity_view_init(&sender_view, sender);
    sender_arg = &sender_view;
  }

  meshcore_platform_mesh_on_path_recv(packet_arg, sender_arg, path, path_len,
                                      extra_type, extra, extra_len);
}

void meshcore_platform_bridge_mesh_on_control_data_recv(
    struct meshcore_packet *packet) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    meshcore_platform_mesh_on_control_data_recv(NULL);
    return;
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  meshcore_platform_mesh_on_control_data_recv(&view);
}

void meshcore_platform_bridge_mesh_on_raw_data_recv(
    struct meshcore_packet *packet) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    meshcore_platform_mesh_on_raw_data_recv(NULL);
    return;
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  meshcore_platform_mesh_on_raw_data_recv(&view);
}

void meshcore_platform_bridge_mesh_on_group_data_recv(
    struct meshcore_packet *packet, uint8_t type,
    const struct meshcore_group_channel *channel, uint8_t *data, size_t len) {
  meshcore_common_packet_view_t packet_view;
  meshcore_common_channel_view_t channel_view;
  const meshcore_common_packet_view_t *packet_arg = NULL;
  const meshcore_common_channel_view_t *channel_arg = NULL;

  if (packet != NULL) {
    meshcore_platform_bridge_packet_view_init(&packet_view, packet, NULL, 0U);
    packet_arg = &packet_view;
  }
  if (channel != NULL) {
    meshcore_platform_bridge_channel_view_init(&channel_view, channel);
    channel_arg = &channel_view;
  }

  meshcore_platform_mesh_on_group_data_recv(packet_arg, type, channel_arg, data,
                                            len);
}

void meshcore_platform_bridge_mesh_on_ack_recv(struct meshcore_packet *packet,
                                            uint32_t ack_crc) {
  meshcore_common_packet_view_t view;

  if (packet == NULL) {
    meshcore_platform_mesh_on_ack_recv(NULL, ack_crc);
    return;
  }

  meshcore_platform_bridge_packet_view_init(&view, packet, NULL, 0U);
  meshcore_platform_mesh_on_ack_recv(&view, ack_crc);
}
